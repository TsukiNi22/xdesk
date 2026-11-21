/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/11/2026 by @author Tsukini

File Name:
##  @file Compress.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/host/Compress.hpp"
#include <QLibrary>
#include <QtEndian>
#include <cstddef>

//----------------------------------------------------------------//
/* STRUCT */

// liblz4 / libzstd (loaded at run time: measured by the bench, never required)
struct Packers {
    int (*lz4Bound)(int) = nullptr;
    int (*lz4Compress)(const char*, char*, int, int) = nullptr;
    int (*lz4Decompress)(const char*, char*, int, int) = nullptr;
    std::size_t (*zstdBound)(std::size_t) = nullptr;
    std::size_t (*zstdCompress)(void*, std::size_t, const void*, std::size_t, int) = nullptr;
    std::size_t (*zstdDecompress)(void*, std::size_t, const void*, std::size_t) = nullptr;
    unsigned (*zstdIsError)(std::size_t) = nullptr;
};

//----------------------------------------------------------------//
/* TOOLS */

_cold static Packers load_packers(void)
{
    Packers p;
    QLibrary lz4("lz4", 1);
    QLibrary zstd("zstd", 1);

    if (lz4.load()) {
        p.lz4Bound = reinterpret_cast<decltype(p.lz4Bound)>(lz4.resolve("LZ4_compressBound"));
        p.lz4Compress = reinterpret_cast<decltype(p.lz4Compress)>(lz4.resolve("LZ4_compress_default"));
        p.lz4Decompress = reinterpret_cast<decltype(p.lz4Decompress)>(lz4.resolve("LZ4_decompress_safe"));
    }
    if (zstd.load()) {
        p.zstdBound = reinterpret_cast<decltype(p.zstdBound)>(zstd.resolve("ZSTD_compressBound"));
        p.zstdCompress = reinterpret_cast<decltype(p.zstdCompress)>(zstd.resolve("ZSTD_compress"));
        p.zstdDecompress = reinterpret_cast<decltype(p.zstdDecompress)>(zstd.resolve("ZSTD_decompress"));
        p.zstdIsError = reinterpret_cast<decltype(p.zstdIsError)>(zstd.resolve("ZSTD_isError"));
    }
    return p;
}

_cold static const Packers& packers(void)
{
    static const Packers p = load_packers();

    return p;
}

//----------------------------------------------------------------//
/* FUNCTION */

QStringList xdesk::host::compress_codecs(void)
{
    QStringList codecs = {"zlib"};
    const Packers& p = packers();

    if (p.lz4Bound && p.lz4Compress && p.lz4Decompress) codecs << "lz4";
    if (p.zstdBound && p.zstdCompress && p.zstdDecompress && p.zstdIsError) codecs << "zstd";
    return codecs;
}

// zlib: the stream of qCompress without its 4 bytes of size (what DecompressionStream("deflate") reads)
QByteArray xdesk::host::compress_with(const QString& codec, const QByteArray& data)
{
    const Packers& p = packers();

    if (codec == "zlib") return qCompress(data, 1).mid(4);
    if (codec == "lz4" && p.lz4Compress) {
        QByteArray out(p.lz4Bound(static_cast<int>(data.size())), Qt::Uninitialized);
        const int size = p.lz4Compress(data.constData(), out.data(), static_cast<int>(data.size()), static_cast<int>(out.size()));
        if (size <= 0) return {};
        out.resize(size);
        return out;
    }
    if (codec == "zstd" && p.zstdCompress) {
        QByteArray out(static_cast<qsizetype>(p.zstdBound(static_cast<std::size_t>(data.size()))), Qt::Uninitialized);
        const std::size_t size = p.zstdCompress(out.data(), static_cast<std::size_t>(out.size()), data.constData(), static_cast<std::size_t>(data.size()), 1);
        if (p.zstdIsError(size)) return {};
        out.resize(static_cast<qsizetype>(size));
        return out;
    }
    return {};
}

QByteArray xdesk::host::decompress_with(const QString& codec, const QByteArray& data, const int size)
{
    const Packers& p = packers();

    if (size < 0) return {};
    if (codec == "zlib") {
        QByteArray framed(4, Qt::Uninitialized);
        qToBigEndian<quint32>(static_cast<quint32>(size), framed.data());
        return qUncompress(framed + data);
    }
    if (codec == "lz4" && p.lz4Decompress) {
        QByteArray out(size, Qt::Uninitialized);
        if (p.lz4Decompress(data.constData(), out.data(), static_cast<int>(data.size()), size) != size) return {};
        return out;
    }
    if (codec == "zstd" && p.zstdDecompress) {
        QByteArray out(size, Qt::Uninitialized);
        const std::size_t got = p.zstdDecompress(out.data(), static_cast<std::size_t>(size), data.constData(), static_cast<std::size_t>(data.size()));
        if (p.zstdIsError(got) || got != static_cast<std::size_t>(size)) return {};
        return out;
    }
    return {};
}
