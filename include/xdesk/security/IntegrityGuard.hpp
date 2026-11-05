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
##  @file IntegrityGuard.hpp

File Description:
##  Tamper detection: protected files, process hardening, remote terminal commands and processes
\**************************************************************/

#ifndef INTEGRITYGUARD_H
    #define INTEGRITYGUARD_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include <QFileSystemWatcher>   // QFileSystemWatcher
    #include <QStringList>          // QStringList
    #include <QByteArray>           // QByteArray
    #include <QObject>              // QObject
    #include <QString>              // QString
    #include <QTimer>               // QTimer
    #include <QHash>                // QHash

namespace xdesk::security { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * What is protected: the binary, the config folder, the data folder (devices, certificate) and the host process
 * What is watched:
 *  - file hashes (watcher + periodic re-hash): a change while remote sessions run = tamper (session unknown -> every
 *    session with control / terminal rights)
 *  - the lines typed in the remote terminals (paths, process name, pid, kill / taskkill / systemctl...)
 *  - the processes started from the remote terminals (Linux: cmdline + open files of the descendants)
 *  - a crash / kill of the host while sessions were running (marker checked at the next start)
 * The host process is hardened: not dumpable / not traceable (Linux), terminate and memory rights removed (Windows)
*/
class IntegrityGuard: public QObject {
    Q_OBJECT

    private:
        QStringList _files; // protected files (hashed)
        QStringList _dirs; // protected folders (watched)
        QHash<QString, QByteArray> _hashes; // path -> sha256
        QHash<QString, qint64> _roots; // session id -> pid of its terminal shell
        QFileSystemWatcher _watcher;
        QTimer _timer; // periodic scan
        bool _paused = false; // writes of the host itself (settings, devices)
        QString _markerPath; // running.json (sessions alive, removed on a clean stop)

        // ---------- Pre-Function -------- //
        void rehash_(void); // compare every protected file
        void scanProcesses_(void); // descendants of the terminal shells
        _cold _nodiscard bool mentionsProtected_(const QString& text) const; // path, binary name, host pid

    public:
        // ---------- Pre-Function -------- //
        void start(const QStringList& files, const QStringList& dirs, const QString& markerPath);
        void harden(void); // process hardening (see above)
        void beginTrustedWrite(void); // the host writes its own files: no alert
        void endTrustedWrite(void); // re-hash
        void addTerminal(const QString& session, const qint64 pid);
        void removeTerminal(const QString& session, const qint64 pid);
        _hot _nodiscard bool checkCommand(const QString& session, const QString& line); // true = suspicious (emits tamper)
        void writeMarker(const QStringList& sessions); // sessions alive (with control / terminal)
        _cold _nodiscard QStringList previousCrashSessions(void) const; // sessions of an unclean stop
        _cold _nodiscard static bool isInsideRemoteSession(void); // this process descends from an xdesk terminal

        // ---------- Constructor --------- //
        explicit IntegrityGuard(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~IntegrityGuard() override = default;

    signals:
        void tamper(const QString& session, const QString& reason); // session empty = unknown author
};

} // namespace end
#endif /* INTEGRITYGUARD_H */
