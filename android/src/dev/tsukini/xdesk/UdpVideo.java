/*
 * Video of the android app over the encrypted udp of the native viewers (include/xdesk/net/Datagram.hpp, UdpLink.cpp):
 *  - datagram = [u8 type][u32 connection][u64 sequence] + AES-256-GCM(payload) + tag, the header authenticated, nonce =
 *    u32 direction (1 host -> viewer, 2 viewer -> host) + u64 sequence; the keys come from the welcome (inside TLS)
 *  - frames cut in pieces of 1150 bytes, one xor parity per group of 8 (one lost piece rebuilt without a round trip),
 *    the others asked again (nack), the frame acked once complete; anything not authentic is dropped
 *  - hellos until the host answers (2.5 s, else the video stays on tcp), keep alive every second, silent host 5 s = tcp
 * Plain java (no android class): one thread does everything, the listener is called from it
*/
package dev.tsukini.xdesk;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public final class UdpVideo {
    // ---------------------------------------------------------------- //
    // Protocol (Datagram.hpp)

    static final int HELLO = 1, HELLO_ACK = 2, DATA = 3, ACK = 4, NACK = 5, PING = 6, PONG = 7;
    static final int CHUNK = 1150; // bytes of a frame per datagram
    static final int GROUP = 8; // data pieces covered by one parity
    static final int HEADER = 13; // type, connection, sequence
    static final int TAG = 16; // gcm tag
    static final int DATA_HEADER = 12; // u32 frame, u16 index, u16 count, u32 size
    static final int WINDOW = 2048; // sequences remembered against replays
    static final int GIVE_UP = 3000; // ms before an incomplete frame is given up
    static final int PARTIALS = 64; // frames put together at once at most
    static final int DONE_KEPT = 256; // completed frames remembered
    static final int MAX_MESSAGE = 64 << 20; // XDESK_MAX_MESSAGE
    static final int HELLO_EVERY = 250, HELLO_GIVE_UP = 2500, PING_EVERY = 1000, SILENCE = 5000;

    public interface Listener {
        void frame(byte[] frame); // a Binary::Frame put together (acked already)
        void active(boolean active); // false also when the hellos got no answer
        void gaveUp(); // a frame lost for good: a complete one is needed
    }

    // A frame being put together
    private static final class Partial {
        int count;
        long size;
        final HashMap<Integer, byte[]> data = new HashMap<>();
        final HashMap<Integer, byte[]> parity = new HashMap<>();
        long first, last, nacked;
    }

    private final Listener _listener;
    private volatile Thread _thread;
    private volatile DatagramSocket _socket;
    private volatile int _rtt = 50;

    public UdpVideo(Listener listener) {this._listener = listener;}

    public void setRtt(int rtt) {this._rtt = Math.max(1, rtt);}

    // ---------------------------------------------------------------- //
    // Start / stop

    // host: the address of the pinned host (never one given by the page); false: no usable offer
    public synchronized boolean start(final String host, final int port, final long connection, final byte[] down, final byte[] up) {
        this.stop();
        if (host == null || host.isEmpty() || port <= 0 || port > 65535 || connection <= 0 || connection > 0xFFFFFFFFL || down == null || down.length != 32 || up == null || up.length != 32) return false;
        Thread thread = new Thread(() -> this.run(host, port, (int) connection, down, up), "xdesk-udp");
        thread.setDaemon(true);
        this._thread = thread;
        thread.start();
        return true;
    }

    public synchronized void stop() {
        Thread thread = this._thread;
        DatagramSocket socket = this._socket;
        this._thread = null;
        if (thread == null) return;
        thread.interrupt();
        if (socket != null) socket.close();
        try {thread.join(500);} catch (InterruptedException e) {Thread.currentThread().interrupt();}
    }

    // ---------------------------------------------------------------- //
    // Loop of the thread

    private void run(String host, int port, int connection, byte[] down, byte[] up) {
        Crypto crypto;
        InetAddress address;
        DatagramSocket socket;
        final Thread me = Thread.currentThread();
        final HashMap<Integer, Partial> partial = new HashMap<>();
        final ArrayDeque<Integer> doneOrder = new ArrayDeque<>();
        final HashSet<Integer> done = new HashSet<>();
        final byte[] buffer = new byte[2048];
        boolean active = false;
        long since = now(), hello = 0, ping = 0, seen = 0;

        try {
            crypto = new Crypto(connection, up, down);
            address = InetAddress.getByName(host);
            socket = new DatagramSocket();
            socket.setReceiveBufferSize(4 << 20); // a big frame arrives at once
            socket.setSoTimeout(5);
            socket.connect(address, port); // only the host is heard
            this._socket = socket;
        } catch (Exception e) {
            if (this._thread == me) this._listener.active(false);
            return;
        }
        try {
            while (this._thread == me) {
                // Receive what is there (5 ms at most), then the timers
                try {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    socket.receive(packet);
                    byte[] payload = crypto.open(buffer, packet.getLength());
                    if (payload != null) {
                        long t = now();
                        seen = t;
                        if (!active) {active = true; this._listener.active(true);} // a HelloAck, or data when the ack was lost
                        if (crypto.lastType == DATA) {
                            int[] id = new int[1];
                            byte[][] frame = new byte[1][];
                            int result = add(partial, done, doneOrder, payload, t, frame, id);
                            if (result == 1 || result == 2) send(socket, crypto, ACK, ByteBuffer.allocate(4).putInt(id[0]).array());
                            if (result == 1) this._listener.frame(frame[0]);
                        }
                    }
                } catch (SocketTimeoutException e) { /* timers */ }
                long t = now();
                if (!active) {
                    if (t - since > HELLO_GIVE_UP) {this._listener.active(false); break;} // no answer: tcp
                    if (t - hello >= HELLO_EVERY) {send(socket, crypto, HELLO, new byte[0]); hello = t;}
                    continue;
                }
                if (t - seen > SILENCE) {this._listener.active(false); break;} // the host went silent: tcp
                if (t - ping >= PING_EVERY) {send(socket, crypto, PING, ByteBuffer.allocate(8).putLong(t).array()); ping = t;}
                for (byte[] nack: missing(partial, t, this._rtt))
                    send(socket, crypto, NACK, nack);
                if (expired(partial, done, doneOrder, t)) this._listener.gaveUp();
            }
        } catch (Exception e) {
            if (this._thread == me && active) this._listener.active(false);
        } finally {
            socket.close();
        }
    }

    private static long now() {return System.nanoTime() / 1000000L;}

    private static void send(DatagramSocket socket, Crypto crypto, int type, byte[] payload) throws java.io.IOException {
        byte[] packet = crypto.seal(type, payload);
        if (packet != null) socket.send(new DatagramPacket(packet, packet.length));
    }

    // ---------------------------------------------------------------- //
    // Pieces (FrameAssembler of Datagram.cpp): 1 = complete, 2 = piece of a frame already complete, 0 = waiting, -1 = invalid

    static int add(HashMap<Integer, Partial> partial, HashSet<Integer> done, ArrayDeque<Integer> doneOrder, byte[] payload, long t, byte[][] out, int[] outId) {
        if (payload.length < DATA_HEADER) return -1;
        ByteBuffer b = ByteBuffer.wrap(payload);
        int id = b.getInt(0);
        int index = b.getShort(4) & 0xFFFF;
        int count = b.getShort(6) & 0xFFFF;
        long size = b.getInt(8) & 0xFFFFFFFFL;
        int length = payload.length - DATA_HEADER;
        int groups = (count + GROUP - 1) / GROUP;
        outId[0] = id;

        // Pieces checked against the frame they claim (authentic, but never trusted for sizes)
        if (count == 0 || size == 0 || size > MAX_MESSAGE || count != (size + CHUNK - 1) / CHUNK || index >= count + groups) return -1;
        if (index < count && length != Math.min(CHUNK, size - (long) index * CHUNK)) return -1;
        if (index >= count && length != CHUNK) return -1;
        if (done.contains(id)) return 2;
        Partial p = partial.get(id);
        if (p == null) {
            // A new frame: the oldest one dropped when too many wait
            if (partial.size() >= PARTIALS) {
                Integer oldest = null;
                for (Map.Entry<Integer, Partial> e: partial.entrySet())
                    if (oldest == null || e.getValue().first < partial.get(oldest).first) oldest = e.getKey();
                partial.remove(oldest);
            }
            p = new Partial();
            p.count = count;
            p.size = size;
            p.first = t;
            partial.put(id, p);
        }
        if (p.count != count || p.size != size) return -1;
        byte[] bytes = new byte[length];
        System.arraycopy(payload, DATA_HEADER, bytes, 0, length);
        if (index < count) p.data.put(index, bytes);
        else p.parity.put(index - count, bytes);
        p.last = t;

        // Groups missing one piece with their parity: rebuilt
        for (Map.Entry<Integer, byte[]> e: p.parity.entrySet()) {
            int start = e.getKey() * GROUP;
            int end = Math.min(count, start + GROUP);
            int lost = -1, missing = 0;
            for (int i = start; i < end; ++i)
                if (!p.data.containsKey(i)) {lost = i; missing++;}
            if (missing != 1) continue;
            byte[] rebuilt = e.getValue().clone();
            for (int i = start; i < end; ++i) {
                if (i == lost) continue;
                byte[] piece = p.data.get(i);
                for (int j = 0; j < piece.length; ++j)
                    rebuilt[j] ^= piece[j];
            }
            int keep = (int) Math.min(CHUNK, size - (long) lost * CHUNK);
            byte[] cut = new byte[keep];
            System.arraycopy(rebuilt, 0, cut, 0, keep);
            p.data.put(lost, cut);
        }
        if (p.data.size() != count) return 0;

        // Complete
        byte[] frame = new byte[(int) size];
        for (int i = 0; i < count; ++i)
            System.arraycopy(p.data.get(i), 0, frame, i * CHUNK, p.data.get(i).length);
        partial.remove(id);
        remember(done, doneOrder, id);
        out[0] = frame;
        return 1;
    }

    private static void remember(HashSet<Integer> done, ArrayDeque<Integer> doneOrder, int id) {
        if (done.add(id)) doneOrder.addLast(id);
        while (doneOrder.size() > DONE_KEPT)
            done.remove(doneOrder.removeFirst());
    }

    // Nacks due now: pieces not rebuildable, after the reordering delay, then one ping later
    static List<byte[]> missing(HashMap<Integer, Partial> partial, long t, int rtt) {
        final long wait = Math.max(4, rtt / 8);
        final long again = Math.max(15, rtt);
        ArrayList<byte[]> due = new ArrayList<>();

        for (Map.Entry<Integer, Partial> e: partial.entrySet()) {
            Partial p = e.getValue();
            if (t - p.last <= wait || t - p.nacked <= again) continue;
            ArrayList<Integer> lost = new ArrayList<>();
            for (int i = 0; i < p.count && lost.size() < 500; ++i)
                if (!p.data.containsKey(i)) lost.add(i);
            if (lost.isEmpty()) continue;
            p.nacked = t;
            ByteBuffer b = ByteBuffer.allocate(6 + 2 * lost.size());
            b.putInt(e.getKey()).putShort((short) lost.size());
            for (int index: lost)
                b.putShort((short) index);
            due.add(b.array());
        }
        return due;
    }

    // Frames given up (removed, their late pieces only acked): true when there was one
    static boolean expired(HashMap<Integer, Partial> partial, HashSet<Integer> done, ArrayDeque<Integer> doneOrder, long t) {
        boolean any = false;

        for (Iterator<Map.Entry<Integer, Partial>> it = partial.entrySet().iterator(); it.hasNext();) {
            Map.Entry<Integer, Partial> e = it.next();
            if (t - e.getValue().first <= GIVE_UP) continue;
            it.remove();
            remember(done, doneOrder, e.getKey());
            any = true;
        }
        return any;
    }

    // ---------------------------------------------------------------- //
    // Keys, sequences and replay window of the viewer side (DatagramCrypto)

    static final class Crypto {
        private final SecretKeySpec _send;
        private final SecretKeySpec _recv;
        private final int _connection;
        private final Cipher _cipher;
        private long _sequence = 0; // last sent
        private long _highest = 0; // highest received
        private final long[] _bits = new long[WINDOW / 64]; // bit i = sequence (highest - i)
        int lastType = 0; // type of the last datagram opened

        Crypto(int connection, byte[] sendKey, byte[] recvKey) throws Exception {
            this._connection = connection;
            this._send = new SecretKeySpec(sendKey, "AES");
            this._recv = new SecretKeySpec(recvKey, "AES");
            this._cipher = Cipher.getInstance("AES/GCM/NoPadding");
        }

        private static byte[] nonce(int direction, long sequence) {
            return ByteBuffer.allocate(12).putInt(direction).putLong(sequence).array();
        }

        byte[] seal(int type, byte[] payload) {
            try {
                long sequence = ++this._sequence;
                byte[] head = ByteBuffer.allocate(HEADER).put((byte) type).putInt(this._connection).putLong(sequence).array();
                this._cipher.init(Cipher.ENCRYPT_MODE, this._send, new GCMParameterSpec(TAG * 8, nonce(2, sequence)));
                this._cipher.updateAAD(head);
                byte[] body = this._cipher.doFinal(payload); // ciphertext + tag
                byte[] packet = new byte[HEADER + body.length];
                System.arraycopy(head, 0, packet, 0, HEADER);
                System.arraycopy(body, 0, packet, HEADER, body.length);
                return packet;
            } catch (Exception e) {
                return null;
            }
        }

        // null: forged, damaged or replayed (nothing remembered)
        byte[] open(byte[] packet, int length) {
            if (length < HEADER + TAG) return null;
            ByteBuffer b = ByteBuffer.wrap(packet, 0, length);
            int connection = b.getInt(1);
            long sequence = b.getLong(5);
            if (connection != this._connection || !this.fresh(sequence)) return null;
            try {
                this._cipher.init(Cipher.DECRYPT_MODE, this._recv, new GCMParameterSpec(TAG * 8, nonce(1, sequence)));
                this._cipher.updateAAD(packet, 0, HEADER);
                byte[] payload = this._cipher.doFinal(packet, HEADER, length - HEADER);
                this.remember(sequence); // only an authentic datagram moves the window
                this.lastType = packet[0] & 0xFF;
                return payload;
            } catch (Exception e) {
                return null;
            }
        }

        private boolean fresh(long sequence) {
            if (sequence <= 0) return false;
            if (sequence > this._highest) return true;
            long age = this._highest - sequence;
            if (age >= WINDOW) return false;
            return (this._bits[(int) (age / 64)] & (1L << (age % 64))) == 0;
        }

        private void remember(long sequence) {
            if (sequence > this._highest) {
                long shift = sequence - this._highest;
                if (shift >= WINDOW) java.util.Arrays.fill(this._bits, 0L);
                else {
                    int whole = (int) (shift / 64);
                    int part = (int) (shift % 64);
                    for (int i = this._bits.length - 1; i >= 0; --i) {
                        int source = i - whole;
                        long value = 0;
                        if (source >= 0) {
                            value = this._bits[source] << part;
                            if (part != 0 && source - 1 >= 0) value |= this._bits[source - 1] >>> (64 - part);
                        }
                        this._bits[i] = value;
                    }
                }
                this._highest = sequence;
            }
            long age = this._highest - sequence;
            this._bits[(int) (age / 64)] |= 1L << (age % 64);
        }
    }
}
