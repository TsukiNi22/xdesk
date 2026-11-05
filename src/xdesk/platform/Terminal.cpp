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
##  @file Terminal.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/ITerminal.hpp"
#ifdef _WIN32
    #include "xdesk/platform/windows/ConPtyTerminal.hpp"
#else
    #include "xdesk/platform/unix/PtyTerminal.hpp"
#endif
#include <QStandardPaths>
#include <QFileInfo>
#include <QtGlobal>
#ifndef _WIN32
    #include <unistd.h>
    #include <pwd.h>
#endif

xdesk::platform::ITerminal* xdesk::platform::create_terminal(QObject* parent)
{
    #ifdef _WIN32
        return new ConPtyTerminal(parent);
    #else
        return new PtyTerminal(parent);
    #endif
}

QString xdesk::platform::default_shell(void)
{
    #ifdef _WIN32
        const QString root = qEnvironmentVariable("SystemRoot", "C:\\Windows");
        const QString pwsh = QStandardPaths::findExecutable("pwsh.exe");
        const QString powershell = root + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";

        if (!pwsh.isEmpty()) return pwsh;
        if (QFileInfo::exists(powershell)) return powershell;
        return qEnvironmentVariable("COMSPEC", root + "\\System32\\cmd.exe");
    #else
        const QString shell = qEnvironmentVariable("SHELL");
        const passwd* user = getpwuid(getuid());

        if (!shell.isEmpty() && QFileInfo(shell).isExecutable()) return shell;
        if (user && user->pw_shell && QFileInfo(QString::fromLocal8Bit(user->pw_shell)).isExecutable()) return QString::fromLocal8Bit(user->pw_shell);
        return "/bin/sh";
    #endif
}
