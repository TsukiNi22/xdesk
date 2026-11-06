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
##  @file Datagram.hpp

File Description:
##  Encrypted UDP datagrams of the video (AES-256-GCM, anti replay), frames cut in pieces with xor parity and put together again
\**************************************************************/

#ifndef DATAGRAM_H
    #define DATAGRAM_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <cstdint>          // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, std::int64_t
    #include <QList>            // QList
    #include <QHash>            // QHash
    #include <QPair>            // QPair

    //----------------------------------------------------------------//
    /* DEFINE */

    #define XDESK_UDP_CHUNK 1150 // bytes of a frame per datagram (fits a 1280 bytes path with the headers)
    #define XDESK_UDP_GROUP 8 // data datagrams covered by one xor parity
    #define XDESK_UDP_HEADER 13 // u8 type, u32 connection, u64 sequence (authenticated, in clear)
    #define XDESK_UDP_TAG 16 // gcm tag
    #define XDESK_UDP_WINDOW 2048 // sequences remembered against replays
    #define XDESK_UDP_GIVE_UP 3000 // ms before an incomplete frame is given up (the viewer asks a complete one)
    #define XDESK_UDP_PARTIALS 64 // frames put together at once at most

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* ENUM */

/*
 * Datagram = [u8 type][u32 connection][u64 sequence] + AES-256-GCM(payload) + tag, the header authenticated (aad)
 * nonce = u32 direction (1 host -> viewer, 2 viewer -> host) + u64 sequence: a fresh sequence per datagram, a key per
 * direction; the keys come from the host inside the authenticated TLS session (welcome) and never travel in clear
*/
enum class Datagram: std::uint8_t {
    Hello = 1, // V->H (empty): the viewer proves the keys from its address
    HelloAck = 2, // H->V (empty)
    Data = 3, // H->V u32 frame, u16 index (data pieces first, then one parity per group), u16 data count, u32 frame size, bytes
    Ack = 4, // V->H u32 frame (complete)
    Nack = 5, // V->H u32 frame, u16 n, n * u16 missing indexes
    Ping = 6, // V->H u64 ms (keeps the NAT open)
    Pong = 7, // H->V u64 ms (the same)
};

//----------------------------------------------------------------//
/* STRUCT */

// A frame being put together by the viewer
struct PartialFrame {
    std::uint16_t count = 0; // data pieces
    std::uint32_t size = 0; // frame bytes
    QHash<int, QByteArray> data; // index -> bytes
    QHash<int, QByteArray> parity; // group -> xor of its pieces
    std::int64_t first = 0; // ms of its first piece
    std::int64_t last = 0; // ms of its last piece
    std::int64_t nacked = 0; // ms of the last nack
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* pieces */
_hot _nodiscard QList<QByteArray> split_frame(const std::uint32_t id, const QByteArray& frame, const bool fec); // data payloads, then the parity ones when fec
_hot _nodiscard std::uint32_t frame_id(const QByteArray& message); // id of a Binary::Frame message (0 = not one)

//----------------------------------------------------------------//
/* CLASS */

// Keys, sequences and replay window of one connection (one side)
class DatagramCrypto {
    private:
        QByteArray _sendKey; // 32 bytes
        QByteArray _recvKey; // 32 bytes
        std::uint32_t _connection = 0;
        std::uint32_t _sendDirection = 1;
        std::uint64_t _sequence = 0; // last sent
        std::uint64_t _highest = 0; // highest received
        QList<std::uint64_t> _bits; // replay window below _highest (one bit per sequence)

        // ---------- Pre-Function -------- //
        _hot _nodiscard bool fresh_(const std::uint64_t sequence) const; // not seen yet, not too old
        _hot void remember_(const std::uint64_t sequence);

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard QByteArray seal(const Datagram type, const QByteArray& payload); // "" when the keys are missing
        _hot _nodiscard bool open(const QByteArray& packet, Datagram& type, QByteArray& payload); // false: forged, damaged or replayed (nothing remembered)
        _cold _nodiscard static std::uint32_t connectionOf(const QByteArray& packet); // 0 = too short

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline std::uint64_t highest(void) const    {return this->_highest;};
        _cold _nodiscard inline std::uint32_t connection(void) const {return this->_connection;};

        // ---------- Constructor --------- //
        DatagramCrypto(void) = default;

        DatagramCrypto(const std::uint32_t connection, const QByteArray& sendKey, const QByteArray& recvKey, const bool host): _sendKey{sendKey}, _recvKey{recvKey}, _connection{connection}, _sendDirection{host ? 1U : 2U} { /* Nothing */ }; // host: sends direction 1
};

// Viewer side: pieces of frames put together (any order), lost ones rebuilt from the parity or asked again
class FrameAssembler {
    private:
        QHash<std::uint32_t, PartialFrame> _partial; // frame -> pieces
        QList<std::uint32_t> _done; // last frames completed (their late pieces are only acked again)

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard int add(const QByteArray& payload, const std::int64_t now, QByteArray& frame, std::uint32_t& id); // 1 = frame complete (in frame), 2 = piece of a frame already complete, 0 = waiting, -1 = invalid
        _hot _nodiscard QList<QPair<std::uint32_t, QList<std::uint16_t>>> missing(const std::int64_t now, const int rtt); // nacks due now (pieces not rebuildable, after the reordering delay, then every ping)
        _cold _nodiscard QList<std::uint32_t> expired(const std::int64_t now); // frames given up (removed)
        _cold void clear(void);

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int pending(void) const {return static_cast<int>(this->_partial.size());};
};

} // namespace end
#endif /* DATAGRAM_H */
