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
##  @file IntegrityGuard.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <tlhelp32.h>
    #include <windows.h>
    #include <aclapi.h>
    #include <sddl.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/IntegrityGuard.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/core/Config.hpp"
#include <QRegularExpression>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QSaveFile>
#include <QString>
#include <QFile>
#include <QDir>
#include <QSet>
#ifndef _WIN32
    #include <sys/prctl.h>
    #include <unistd.h>
    #include <signal.h>
#endif

// Commands that stop, trace or change a process
static const QRegularExpression KILL_COMMAND(R"((^|[\s;|&(`$])(kill|pkill|killall|taskkill|stop-process|systemctl|loginctl|gdb|strace|ltrace|ptrace|pgrep|chmod|chown|chattr|rm|mv|cp|dd|truncate|sed|tee|install|ln)(\s|$))", QRegularExpression::CaseInsensitiveOption);

xdesk::security::IntegrityGuard::IntegrityGuard(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString&) {this->rehash_();});
    QObject::connect(&this->_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString&) {this->rehash_();});
    QObject::connect(&this->_timer, &QTimer::timeout, this, [this]() {this->rehash_(); this->scanProcesses_();});
}

void xdesk::security::IntegrityGuard::start(const QStringList& files, const QStringList& dirs, const QString& markerPath)
{
    this->_files = files;
    this->_dirs = dirs;
    this->_markerPath = markerPath;
    this->_hashes.clear();
    for (const QString& path: files) {
        QFile file(path);
        this->_hashes[path] = file.open(QIODevice::ReadOnly) ? sha256(file.readAll()) : QByteArray();
    }
    if (!files.isEmpty()) this->_watcher.addPaths(files);
    if (!dirs.isEmpty()) this->_watcher.addPaths(dirs);
    this->_timer.start(3000);
}

void xdesk::security::IntegrityGuard::rehash_(void)
{
    for (const QString& path: this->_files) {
        QFile file(path);
        const QByteArray hash = file.open(QIODevice::ReadOnly) ? sha256(file.readAll()) : QByteArray();

        // A replaced file (atomic save) leaves the watcher: watch it again
        if (file.exists() && !this->_watcher.files().contains(path)) this->_watcher.addPath(path);
        if (hash == this->_hashes.value(path)) continue;
        this->_hashes[path] = hash;
        if (!this->_paused) emit this->tamper("", "Protected file changed: " + path);
    }
}

void xdesk::security::IntegrityGuard::beginTrustedWrite(void)
{
    this->_paused = true;
}

void xdesk::security::IntegrityGuard::endTrustedWrite(void)
{
    // Hashes taken now: the queued watcher events of the write will see no difference
    this->_paused = true;
    this->rehash_();
    this->_paused = false;
}

bool xdesk::security::IntegrityGuard::mentionsProtected_(const QString& text) const
{
    const QString lower = text.toLower();
    const QString pid = QString::number(QCoreApplication::applicationPid());
    static const QRegularExpression NAME(R"((^|[\s/\;|&('"`])xdesk(\.exe)?(\s|$|['"`);|&]))", QRegularExpression::CaseInsensitiveOption);

    for (const QString& path: this->_files)
        if (!path.isEmpty() && lower.contains(path.toLower())) return true;
    for (const QString& path: this->_dirs)
        if (!path.isEmpty() && lower.contains(path.toLower())) return true;
    if (lower.contains("/xdesk/") || lower.contains("\\xdesk\\") || lower.contains("/.config/xdesk")) return true;
    if (NAME.match(text).hasMatch()) return true;

    // The host pid next to a process command
    if (KILL_COMMAND.match(text).hasMatch() && QRegularExpression("(^|\\D)" + pid + "(\\D|$)").match(text).hasMatch()) return true;
    return false;
}

bool xdesk::security::IntegrityGuard::checkCommand(const QString& session, const QString& line)
{
    const QString trimmed = line.trimmed();

    if (trimmed.isEmpty() || !this->mentionsProtected_(trimmed)) return false;
    emit this->tamper(session, "Command touching xdesk typed in a remote terminal: " + trimmed.left(200));
    return true;
}

void xdesk::security::IntegrityGuard::addTerminal(const QString& session, const qint64 pid)
{
    this->_roots.insert(session + "#" + QString::number(pid), pid);
}

void xdesk::security::IntegrityGuard::removeTerminal(const QString& session, const qint64 pid)
{
    this->_roots.remove(session + "#" + QString::number(pid));
}

void xdesk::security::IntegrityGuard::scanProcesses_(void)
{
    #if defined(__linux__)
        QHash<qint64, qint64> parents; // pid -> ppid
        QSet<qint64> reported;

        if (this->_roots.isEmpty()) return;
        for (const QString& entry: QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            bool ok = false;
            const qint64 pid = entry.toLongLong(&ok);
            QFile stat("/proc/" + entry + "/stat");
            QByteArray content;
            qsizetype close = -1;

            if (!ok || !stat.open(QIODevice::ReadOnly)) continue;
            content = stat.readAll();
            close = content.lastIndexOf(')'); // the name may hold spaces
            if (close < 0) continue;
            parents[pid] = content.mid(close + 2).split(' ').value(1).toLongLong();
        }
        for (QHash<qint64, qint64>::const_iterator it = parents.cbegin(); it != parents.cend(); ++it) {
            QString owner = "";
            qint64 current = it.key();

            // Descendant of a terminal shell of a session?
            for (int depth = 0; depth < 64 && current > 1 && owner.isEmpty(); ++depth) {
                for (QHash<QString, qint64>::const_iterator root = this->_roots.cbegin(); root != this->_roots.cend(); ++root)
                    if (root.value() == current) owner = root.key().section('#', 0, 0);
                current = parents.value(current, 0);
            }
            if (owner.isEmpty() || reported.contains(it.key())) continue;

            // Its command line and its open files
            QFile cmdline("/proc/" + QString::number(it.key()) + "/cmdline");
            QString command = cmdline.open(QIODevice::ReadOnly) ? QString::fromLocal8Bit(cmdline.readAll()).replace(QChar('\0'), ' ') : QString();
            if (this->mentionsProtected_(command)) {
                reported.insert(it.key());
                emit this->tamper(owner, "Process started from a remote terminal touches xdesk: " + command.trimmed().left(200));
                continue;
            }
            for (const QFileInfo& fd: QDir("/proc/" + QString::number(it.key()) + "/fd").entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot)) {
                const QString target = fd.symLinkTarget();
                bool hit = false;
                for (const QString& path: this->_files)
                    hit = hit || target == path;
                for (const QString& path: this->_dirs)
                    hit = hit || (!path.isEmpty() && target.startsWith(path + "/") && !target.contains("/logs/"));
                if (!hit) continue;
                reported.insert(it.key());
                emit this->tamper(owner, "Process of a remote terminal opened a protected file: " + target);
                break;
            }
        }
    #endif
}

void xdesk::security::IntegrityGuard::harden(void)
{
    #ifdef _WIN32
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        PACL dacl = nullptr;
        BOOL present = FALSE;
        BOOL defaulted = FALSE;

        // Everyone: synchronize + query only (no terminate, no memory write, no thread injection)
        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;0x00101000;;;WD)", SDDL_REVISION_1, &descriptor, nullptr)) {
            if (GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted) && present)
                SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl, nullptr);
            LocalFree(descriptor);
        }
    #elif defined(__linux__)
        // Not dumpable: no ptrace attach, no /proc/<pid>/mem, no core dump (by the same user)
        prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    #endif
}

void xdesk::security::IntegrityGuard::writeMarker(const QStringList& sessions)
{
    QSaveFile file(this->_markerPath);
    QJsonObject root;

    if (this->_markerPath.isEmpty()) return;
    if (sessions.isEmpty()) {QFile::remove(this->_markerPath); return;}
    root["pid"] = QCoreApplication::applicationPid();
    root["sessions"] = QJsonArray::fromStringList(sessions);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.commit();
}

QStringList xdesk::security::IntegrityGuard::previousCrashSessions(void) const
{
    QFile file(this->_markerPath);
    QJsonObject root;
    QStringList out;

    if (!file.open(QIODevice::ReadOnly)) return out;
    root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue& v: root.value("sessions").toArray())
        out.append(v.toString());
    return out;
}

bool xdesk::security::IntegrityGuard::isInsideRemoteSession(void)
{
    if (!qEnvironmentVariableIsEmpty("XDESK_SESSION")) return true;
    #if defined(__linux__)
        const QString self = QFileInfo("/proc/self/exe").canonicalFilePath();
        qint64 pid = static_cast<qint64>(getppid());

        // An ancestor running the same binary = a shell of an xdesk host
        for (int depth = 0; depth < 64 && pid > 1; ++depth) {
            QFile stat("/proc/" + QString::number(pid) + "/stat");
            QByteArray content;
            qsizetype close = -1;

            if (QFileInfo("/proc/" + QString::number(pid) + "/exe").canonicalFilePath() == self) return true;
            if (!stat.open(QIODevice::ReadOnly)) break;
            content = stat.readAll();
            close = content.lastIndexOf(')');
            if (close < 0) break;
            pid = content.mid(close + 2).split(' ').value(1).toLongLong();
        }
    #elif defined(_WIN32)
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W entry{};
        QHash<DWORD, std::pair<DWORD, QString>> processes; // pid -> <ppid, exe name>
        DWORD pid = GetCurrentProcessId();

        if (snapshot == INVALID_HANDLE_VALUE) return false;
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry))
            processes[entry.th32ProcessID] = {entry.th32ParentProcessID, QString::fromWCharArray(entry.szExeFile).toLower()};
        CloseHandle(snapshot);
        pid = processes.value(pid).first;
        for (int depth = 0; depth < 64 && processes.contains(pid); ++depth) {
            if (processes.value(pid).second == "xdesk.exe") return true;
            pid = processes.value(pid).first;
        }
    #endif
    return false;
}
