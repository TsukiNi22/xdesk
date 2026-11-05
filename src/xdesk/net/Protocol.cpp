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
##  @file Protocol.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/net/Protocol.hpp"
#include <QString>

QString xdesk::net::client_kind_name(const ClientKind kind)
{
    switch (kind) {
        case ClientKind::Gui: return "gui";
        case ClientKind::Tty: return "tty";
        case ClientKind::Web: return "web";
        case ClientKind::Apk: return "apk";
    }
    return "web";
}

xdesk::net::ClientKind xdesk::net::client_kind_from(const QString& name)
{
    if (name == "gui") return ClientKind::Gui;
    if (name == "tty") return ClientKind::Tty;
    if (name == "apk") return ClientKind::Apk;
    return ClientKind::Web;
}
