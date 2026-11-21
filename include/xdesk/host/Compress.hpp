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
##  @file Compress.hpp

File Description:
##  Compression of the messages that are not jpeg / video (terminals, palette tiles): zlib, lz4 and zstd when present
\**************************************************************/

#ifndef COMPRESS_H
    #define COMPRESS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QStringList>      // QStringList
    #include <QByteArray>       // QByteArray
    #include <QString>          // QString

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard QStringList compress_codecs(void); // "zlib" (Qt, every client: DecompressionStream "deflate" in the browsers), "lz4" / "zstd" when their library loads
_hot _nodiscard QByteArray compress_with(const QString& codec, const QByteArray& data); // empty = failed
_hot _nodiscard QByteArray decompress_with(const QString& codec, const QByteArray& data, const int size); // size = the bytes before compression (empty = failed)

} // namespace end
#endif /* COMPRESS_H */
