/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 05/11/2026 by @author Tsukini

File Name:
##  @file Protocol.hpp

File Description:
##  Wire protocol of xdesk: message types, constants and the binary reader / writer
\**************************************************************/

#ifndef PROTOCOL_H
    #define PROTOCOL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <QString>          // QString
    #include <cstdint>          // std::uint8_t, std::uint16_t, std::uint32_t, std::int16_t

    //----------------------------------------------------------------//
    /* DEFINE */

    /* version */
    #define XDESK_PROTOCOL_VERSION 1 // bumped on every incompatible change of the wire format
    #define XDESK_DEFAULT_PORT 47800 // https + wss port of the host
    #define XDESK_UNBAN_WORD "unban" // typed by the person in front of the PC to lift a ban (panels, xdesk unban)
    #define XDESK_DISCOVERY_PORT 47801 // udp broadcast of the hosts on the LAN

    /* limits */
    #define XDESK_MAX_MESSAGE (32 * 1024 * 1024) // biggest websocket message accepted (a full 4K frame)
    #define XDESK_MAX_CONTROL (64 * 1024) // biggest json control message accepted
    #define XDESK_TILE_SIZE 64 // side of the tiles of the dirty rectangles encoder

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* ENUM */

/*
 * Every websocket message is either:
 *  - a text frame: one JSON object {"t": "<type>", ...} (control, see docs/protocol)
 *  - a binary frame: [u8 type][payload] big endian, the types below
*/
enum class Binary: std::uint8_t {
    Frame = 0x10, // S->C u32 id, u16 width, u16 height, u16 count, count * tile
    FrameAck = 0x11, // C->S u32 id, sent on arrival (the acks size the frames in flight and measure the ping / bandwidth)
    Cursor = 0x12, // S->C i16 x, i16 y, u8 visible
    Video = 0x13, // S->C u32 id, u16 width, u16 height, u8 flags (1 = key frame), H.264 annex B access unit of the whole frame (constrained baseline, BT.709 limited range, size rounded up to even: drawn cropped); acked like a Frame, only to the clients listing "h264" in "enc" and while their frames keep their order
    MouseMove = 0x20, // C->S u16 x, u16 y (absolute, pixels of the host screen)
    MouseButton = 0x21, // C->S u8 button (MouseButton), u8 down
    MouseWheel = 0x22, // C->S i16 dx, i16 dy (1/120 of a notch, like Qt angleDelta)
    Key = 0x23, // C->S u8 down, u8 length, code (KeyboardEvent.code: "KeyA", "ShiftLeft"...)
    Text = 0x24, // C->S utf8 text typed (soft keyboards, IME)
    TermData = 0x31, // both u8 terminal id, raw bytes (C->S: input, S->C: output)
};

enum class TileEncoding: std::uint8_t {
    Jpeg = 1, // u32 length, jpeg data
    Solid = 2, // u8 r, u8 g, u8 b (whole tile of one color)
    Palette = 3, // u8 n (1..64), n x (u8 r, u8 g, u8 b), u32 length, zlib stream of w x h u8 indices (lossless: text, ui); only to the clients whose hello lists "palette" in "enc"
    Copy = 4, // u16 sx, u16 sy: the w x h rectangle at (sx, sy) of the frame as it was before this message (scroll); first tiles of a frame, only to the clients listing "copy" in "enc" and while their frames keep their order
};

enum class MouseButton: std::uint8_t {
    Left = 0,
    Middle = 1,
    Right = 2,
    Back = 3,
    Forward = 4,
};

enum class ClientKind: std::uint8_t {
    Gui = 0, // native Qt viewer
    Tty = 1, // native terminal client
    Web = 2, // browser
    Apk = 3, // android application (webview of the web client)
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* kind */
_cold _nodiscard QString client_kind_name(const ClientKind kind); // "gui" | "tty" | "web" | "apk"
_cold _nodiscard ClientKind client_kind_from(const QString& name); // unknown -> Web (the most restricted)

//----------------------------------------------------------------//
/* CLASS */

// Append big endian values to a binary message
class ByteWriter {
    private:
        QByteArray _data; // message being built

    public:
        // ------------ Function ---------- //
        /* setter */
        _hot inline ByteWriter& u8(const std::uint8_t v)   {this->_data.append(static_cast<char>(v)); return *this;};
        _hot inline ByteWriter& u16(const std::uint16_t v) {this->u8(static_cast<std::uint8_t>(v >> 8)); return this->u8(static_cast<std::uint8_t>(v));};
        _hot inline ByteWriter& u32(const std::uint32_t v) {this->u16(static_cast<std::uint16_t>(v >> 16)); return this->u16(static_cast<std::uint16_t>(v));};
        _hot inline ByteWriter& i16(const std::int16_t v)  {return this->u16(static_cast<std::uint16_t>(v));};
        _hot inline ByteWriter& bytes(const QByteArray& v) {this->_data.append(v); return *this;};

        /* getter */
        _hot _nodiscard inline QByteArray& data(void) {return this->_data;};

        // ---------- Constructor --------- //
        explicit ByteWriter(const Binary type, const std::size_t reserve = 64) {this->_data.reserve(static_cast<qsizetype>(reserve)); this->u8(static_cast<std::uint8_t>(type));}; // start a message of this type
};

// Read big endian values of a binary message, every read is bound checked (ok() false after an overflow)
class ByteReader {
    private:
        const QByteArray& _data; // message read (must outlive the reader)
        qsizetype _pos = 1; // after the type byte
        bool _ok = true; // false once a read went past the end

    public:
        // ------------ Function ---------- //
        /* getter */
        _hot _nodiscard inline bool ok(void) const             {return this->_ok;};
        _hot _nodiscard inline qsizetype remaining(void) const {return this->_data.size() - this->_pos;};

        /* parsing */
        _hot _nodiscard inline std::uint8_t u8(void)
        {
            if (this->_pos + 1 > this->_data.size()) {this->_ok = false; return 0;}
            return static_cast<std::uint8_t>(this->_data[this->_pos++]);
        };
        _hot _nodiscard inline std::uint16_t u16(void) {const std::uint16_t hi = this->u8(); return static_cast<std::uint16_t>((hi << 8) | this->u8());};
        _hot _nodiscard inline std::uint32_t u32(void) {const std::uint32_t hi = this->u16(); return (hi << 16) | this->u16();};
        _hot _nodiscard inline std::int16_t i16(void)  {return static_cast<std::int16_t>(this->u16());};
        _hot _nodiscard inline QByteArray bytes(const qsizetype size)
        {
            if (size < 0 || this->_pos + size > this->_data.size()) {this->_ok = false; return {};}
            const QByteArray out = this->_data.mid(this->_pos, size);
            this->_pos += size;
            return out;
        };
        _hot _nodiscard inline QByteArray rest(void) {return this->bytes(this->remaining());};

        // ---------- Constructor --------- //
        explicit ByteReader(const QByteArray& data): _data{data} { /* Nothing */ }; // the type byte is skipped
};

} // namespace end
#endif /* PROTOCOL_H */
