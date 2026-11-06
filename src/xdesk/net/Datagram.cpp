/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 06/11/2026 by @author Tsukini

File Name:
##  @file Datagram.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/net/Protocol.hpp"
#include "xdesk/net/Datagram.hpp"
#include <openssl/evp.h>
#include <functional>
#include <algorithm>
#include <QtEndian>
#include <memory>

//----------------------------------------------------------------//
/* DEFINE */

#define DATA_HEADER 12 // u32 frame, u16 index, u16 count, u32 size
#define DONE_KEPT 256 // completed frames remembered (their late pieces are acked again, not put together twice)

//----------------------------------------------------------------//
// Tools

static void put_u32(char* at, const std::uint32_t value)
{
    qToBigEndian(value, at);
}

static void put_u64(char* at, const std::uint64_t value)
{
    qToBigEndian(value, at);
}

// AES-256-GCM of one datagram (encrypt: in -> out + tag, decrypt: in + tag -> out), false when it fails / the tag is wrong
static bool aes_gcm(const bool encrypt, const QByteArray& key, const QByteArray& nonce, const QByteArray& aad, const QByteArray& in, QByteArray& out, QByteArray& tag)
{
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    const unsigned char* k = reinterpret_cast<const unsigned char*>(key.constData());
    const unsigned char* iv = reinterpret_cast<const unsigned char*>(nonce.constData());
    int length = 0;
    int final = 0;

    if (!ctx || key.size() != 32) return false;
    out.resize(in.size());
    if (encrypt) {
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return false;
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()), nullptr) != 1) return false;
        if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, k, iv) != 1) return false;
        if (EVP_EncryptUpdate(ctx.get(), nullptr, &length, reinterpret_cast<const unsigned char*>(aad.constData()), static_cast<int>(aad.size())) != 1) return false;
        if (!in.isEmpty() && EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &length, reinterpret_cast<const unsigned char*>(in.constData()), static_cast<int>(in.size())) != 1) return false;
        if (EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(out.data()) + length, &final) != 1) return false;
        tag.resize(XDESK_UDP_TAG);
        return EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, XDESK_UDP_TAG, tag.data()) == 1;
    }
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return false;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()), nullptr) != 1) return false;
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, k, iv) != 1) return false;
    if (EVP_DecryptUpdate(ctx.get(), nullptr, &length, reinterpret_cast<const unsigned char*>(aad.constData()), static_cast<int>(aad.size())) != 1) return false;
    if (!in.isEmpty() && EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &length, reinterpret_cast<const unsigned char*>(in.constData()), static_cast<int>(in.size())) != 1) return false;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, XDESK_UDP_TAG, tag.data()) != 1) return false;
    return EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(out.data()) + length, &final) == 1;
}

// Replay window: bit i = sequence (highest - i), shifted when a higher one arrives
static void shift_bits(QList<std::uint64_t>& bits, const std::uint64_t shift)
{
    const qsizetype words = bits.size();
    const std::uint64_t whole = static_cast<std::uint64_t>(shift / 64);
    const int part = static_cast<int>(shift % 64);

    if (shift >= static_cast<std::uint64_t>(words) * 64) {
        std::fill(bits.begin(), bits.end(), 0);
        return;
    }
    for (qsizetype i = words - 1; i >= 0; --i) {
        const qsizetype source = i - static_cast<qsizetype>(whole);
        std::uint64_t value = 0;
        if (source >= 0) {
            value = bits[source] << part;
            if (part != 0 && source - 1 >= 0) value |= bits[source - 1] >> (64 - part);
        }
        bits[i] = value;
    }
}

//----------------------------------------------------------------//
// Pieces

QList<QByteArray> xdesk::net::split_frame(const std::uint32_t id, const QByteArray& frame, const bool fec)
{
    const qsizetype size = frame.size();
    const int count = static_cast<int>((size + XDESK_UDP_CHUNK - 1) / XDESK_UDP_CHUNK);
    QList<QByteArray> payloads;

    const std::function<QByteArray(const int, const QByteArray&)> piece = [id, count, size](const int index, const QByteArray& bytes) {
        QByteArray out(DATA_HEADER, '\0');
        put_u32(out.data(), id);
        qToBigEndian(static_cast<std::uint16_t>(index), out.data() + 4);
        qToBigEndian(static_cast<std::uint16_t>(count), out.data() + 6);
        put_u32(out.data() + 8, static_cast<std::uint32_t>(size));
        return out + bytes;
    };

    if (size == 0 || count > 0xFFFF / 2) return payloads;
    payloads.reserve(count + (fec ? count / XDESK_UDP_GROUP + 1 : 0));
    for (int i = 0; i < count; ++i)
        payloads.append(piece(i, frame.mid(static_cast<qsizetype>(i) * XDESK_UDP_CHUNK, XDESK_UDP_CHUNK)));

    // One xor parity per group: one lost piece of the group is rebuilt without a round trip
    if (fec) {
        for (int group = 0; group * XDESK_UDP_GROUP < count; ++group) {
            QByteArray parity(XDESK_UDP_CHUNK, '\0');
            for (int i = group * XDESK_UDP_GROUP; i < std::min(count, (group + 1) * XDESK_UDP_GROUP); ++i) {
                const char* data = frame.constData() + static_cast<qsizetype>(i) * XDESK_UDP_CHUNK;
                const qsizetype length = std::min<qsizetype>(XDESK_UDP_CHUNK, size - static_cast<qsizetype>(i) * XDESK_UDP_CHUNK);
                for (qsizetype j = 0; j < length; ++j)
                    parity[j] = static_cast<char>(parity[j] ^ data[j]);
            }
            payloads.append(piece(count + group, parity));
        }
    }
    return payloads;
}

std::uint32_t xdesk::net::frame_id(const QByteArray& message)
{
    if (message.size() < 5 || static_cast<std::uint8_t>(message[0]) != static_cast<std::uint8_t>(Binary::Frame)) return 0;
    return qFromBigEndian<std::uint32_t>(message.constData() + 1);
}

//----------------------------------------------------------------//
// Crypto

QByteArray xdesk::net::DatagramCrypto::seal(const Datagram type, const QByteArray& payload)
{
    QByteArray head(XDESK_UDP_HEADER, '\0');
    QByteArray nonce(12, '\0');
    QByteArray body;
    QByteArray tag;
    const std::uint64_t sequence = ++this->_sequence;

    if (this->_sendKey.size() != 32) return {};
    head[0] = static_cast<char>(type);
    put_u32(head.data() + 1, this->_connection);
    put_u64(head.data() + 5, sequence);
    put_u32(nonce.data(), this->_sendDirection);
    put_u64(nonce.data() + 4, sequence);
    if (!aes_gcm(true, this->_sendKey, nonce, head, payload, body, tag)) return {};
    return head + body + tag;
}

bool xdesk::net::DatagramCrypto::open(const QByteArray& packet, Datagram& type, QByteArray& payload)
{
    QByteArray nonce(12, '\0');
    QByteArray tag;

    if (packet.size() < XDESK_UDP_HEADER + XDESK_UDP_TAG || this->_recvKey.size() != 32) return false;
    const std::uint32_t connection = qFromBigEndian<std::uint32_t>(packet.constData() + 1);
    const std::uint64_t sequence = qFromBigEndian<std::uint64_t>(packet.constData() + 5);
    if (connection != this->_connection || !this->fresh_(sequence)) return false;
    put_u32(nonce.data(), this->_sendDirection == 1 ? 2U : 1U);
    put_u64(nonce.data() + 4, sequence);
    tag = packet.right(XDESK_UDP_TAG);
    if (!aes_gcm(false, this->_recvKey, nonce, packet.left(XDESK_UDP_HEADER), packet.mid(XDESK_UDP_HEADER, packet.size() - XDESK_UDP_HEADER - XDESK_UDP_TAG), payload, tag)) return false;
    this->remember_(sequence); // only an authentic datagram moves the window
    type = static_cast<Datagram>(static_cast<std::uint8_t>(packet[0]));
    return true;
}

std::uint32_t xdesk::net::DatagramCrypto::connectionOf(const QByteArray& packet)
{
    if (packet.size() < XDESK_UDP_HEADER + XDESK_UDP_TAG) return 0;
    return qFromBigEndian<std::uint32_t>(packet.constData() + 1);
}

bool xdesk::net::DatagramCrypto::fresh_(const std::uint64_t sequence) const
{
    const std::uint64_t age = this->_highest - sequence;

    if (sequence == 0) return false;
    if (sequence > this->_highest) return true;
    if (age >= XDESK_UDP_WINDOW || this->_bits.isEmpty()) return false;
    return (this->_bits[static_cast<qsizetype>(age / 64)] & (1ULL << (age % 64))) == 0;
}

void xdesk::net::DatagramCrypto::remember_(const std::uint64_t sequence)
{
    if (this->_bits.isEmpty()) this->_bits = QList<std::uint64_t>(XDESK_UDP_WINDOW / 64, 0);
    if (sequence > this->_highest) {
        shift_bits(this->_bits, sequence - this->_highest);
        this->_highest = sequence;
    }
    const std::uint64_t age = this->_highest - sequence;
    this->_bits[static_cast<qsizetype>(age / 64)] |= 1ULL << (age % 64);
}

//----------------------------------------------------------------//
// Assembler

int xdesk::net::FrameAssembler::add(const QByteArray& payload, const std::int64_t now, QByteArray& frame, std::uint32_t& id)
{
    if (payload.size() < DATA_HEADER) return -1;
    id = qFromBigEndian<std::uint32_t>(payload.constData());
    const int index = qFromBigEndian<std::uint16_t>(payload.constData() + 4);
    const int count = qFromBigEndian<std::uint16_t>(payload.constData() + 6);
    const std::uint32_t size = qFromBigEndian<std::uint32_t>(payload.constData() + 8);
    const QByteArray bytes = payload.mid(DATA_HEADER);
    const int groups = (count + XDESK_UDP_GROUP - 1) / XDESK_UDP_GROUP;

    // Pieces checked against the frame they claim (authentic, but never trusted for sizes)
    if (count == 0 || size == 0 || size > XDESK_MAX_MESSAGE || static_cast<std::uint64_t>(count) != (size + XDESK_UDP_CHUNK - 1) / XDESK_UDP_CHUNK || index >= count + groups) return -1;
    if (index < count && bytes.size() != std::min<qsizetype>(XDESK_UDP_CHUNK, size - static_cast<qsizetype>(index) * XDESK_UDP_CHUNK)) return -1;
    if (index >= count && bytes.size() != XDESK_UDP_CHUNK) return -1;
    if (this->_done.contains(id)) return 2;

    // A new frame: the oldest one dropped when too many wait
    if (!this->_partial.contains(id)) {
        if (this->_partial.size() >= XDESK_UDP_PARTIALS) {
            std::uint32_t oldest = this->_partial.constBegin().key();
            for (QHash<std::uint32_t, PartialFrame>::const_iterator it = this->_partial.constBegin(); it != this->_partial.constEnd(); ++it)
                if (it->first < this->_partial.value(oldest).first) oldest = it.key();
            this->_partial.remove(oldest);
        }
        PartialFrame created;
        created.count = static_cast<std::uint16_t>(count);
        created.size = size;
        created.first = now;
        this->_partial.insert(id, created);
    }
    PartialFrame& p = this->_partial[id];
    if (p.count != count || p.size != size) return -1;
    if (index < count) p.data.insert(index, bytes);
    else p.parity.insert(index - count, bytes);
    p.last = now;

    // Groups missing one piece with their parity: rebuilt
    for (QHash<int, QByteArray>::const_iterator it = p.parity.constBegin(); it != p.parity.constEnd(); ++it) {
        const int first = it.key() * XDESK_UDP_GROUP;
        const int end = std::min(count, first + XDESK_UDP_GROUP);
        int lost = -1;
        int missing = 0;
        for (int i = first; i < end; ++i)
            if (!p.data.contains(i)) {lost = i; missing++;}
        if (missing != 1) continue;
        QByteArray rebuilt = it.value();
        for (int i = first; i < end; ++i) {
            if (i == lost) continue;
            const QByteArray& piece = p.data[i];
            for (qsizetype j = 0; j < piece.size(); ++j)
                rebuilt[j] = static_cast<char>(rebuilt[j] ^ piece[j]);
        }
        p.data.insert(lost, rebuilt.left(std::min<qsizetype>(XDESK_UDP_CHUNK, size - static_cast<qsizetype>(lost) * XDESK_UDP_CHUNK)));
    }
    if (p.data.size() != count) return 0;

    // Complete
    frame.clear();
    frame.reserve(size);
    for (int i = 0; i < count; ++i)
        frame.append(p.data.value(i));
    this->_partial.remove(id);
    this->_done.append(id);
    if (this->_done.size() > DONE_KEPT) this->_done.removeFirst();
    return 1;
}

QList<QPair<std::uint32_t, QList<std::uint16_t>>> xdesk::net::FrameAssembler::missing(const std::int64_t now, const int rtt)
{
    const std::int64_t wait = std::max(4, rtt / 8); // reordering
    const std::int64_t again = std::max(15, rtt); // a nack lost: asked again one ping later
    QList<QPair<std::uint32_t, QList<std::uint16_t>>> due;

    for (QHash<std::uint32_t, PartialFrame>::iterator it = this->_partial.begin(); it != this->_partial.end(); ++it) {
        PartialFrame& p = it.value();
        QList<std::uint16_t> lost;
        if (now - p.last <= wait || now - p.nacked <= again) continue;
        for (int i = 0; i < p.count && lost.size() < 500; ++i)
            if (!p.data.contains(i)) lost.append(static_cast<std::uint16_t>(i));
        if (lost.isEmpty()) continue;
        p.nacked = now;
        due.append(qMakePair(it.key(), lost));
    }
    return due;
}

QList<std::uint32_t> xdesk::net::FrameAssembler::expired(const std::int64_t now)
{
    QList<std::uint32_t> gone;

    for (QHash<std::uint32_t, PartialFrame>::const_iterator it = this->_partial.constBegin(); it != this->_partial.constEnd(); ++it)
        if (now - it->first > XDESK_UDP_GIVE_UP) gone.append(it.key());
    for (const std::uint32_t id: gone) {
        this->_partial.remove(id);
        this->_done.append(id); // its late pieces are not put together again
    }
    return gone;
}

void xdesk::net::FrameAssembler::clear(void)
{
    this->_partial.clear();
    this->_done.clear();
}
