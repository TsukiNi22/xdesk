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
##  @file PtyTerminal.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/unix/PtyTerminal.hpp"
#include <QProcessEnvironment>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <QFileInfo>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <vector>
#include <cerrno>
#include <QDir>
#ifdef __linux__
    #include <sys/syscall.h>
#endif
#if defined(__APPLE__)
    #include <util.h>
#elif defined(__FreeBSD__)
    #include <libutil.h>
#else
    #include <pty.h>
#endif

//----------------------------------------------------------------//
/* DEFINE */

#define READ_CHUNK 65536 // bytes read per call
#define KILL_DELAY 1500 // ms between SIGHUP and SIGKILL

//----------------------------------------------------------------//
/* TOOLS */

// Signal every process of the session of the shell (job control moves the jobs in other process groups)
_cold static void signal_session(const pid_t sid, const int sig)
{
    if (sid <= 0) return;
    (void)::kill(-sid, sig);
    #ifdef __linux__
        const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry: entries) {
            bool number = false;
            const pid_t pid = static_cast<pid_t>(entry.toInt(&number));
            if (!number || pid <= 1) continue;
            if (::getsid(pid) == sid) (void)::kill(pid, sig);
        }
    #endif
}

// Close every fd of the child but the standard ones (async signal safe)
static void close_other_fds(void)
{
    #if defined(__linux__) && defined(SYS_close_range)
        if (::syscall(SYS_close_range, 3u, ~0u, 0u) == 0) return;
    #endif
    const long limit = ::sysconf(_SC_OPEN_MAX);
    for (int fd = 3; fd < (limit > 0 ? static_cast<int>(std::min(limit, 65536L)) : 1024); ++fd)
        (void)::close(fd);
}

//----------------------------------------------------------------//
/* CLASS */

xdesk::platform::PtyTerminal::PtyTerminal(QObject* parent)
: ITerminal(parent)
{
    this->_reaper.setInterval(500);
    QObject::connect(&this->_reaper, &QTimer::timeout, this, [this](void) {this->reap_(false);});
}

xdesk::platform::PtyTerminal::~PtyTerminal()
{
    if (!this->_finished && this->_pid > 0) {
        signal_session(static_cast<pid_t>(this->_pid), SIGKILL);
        (void)::waitpid(static_cast<pid_t>(this->_pid), nullptr, 0);
        this->_finished = true;
    }
    this->closeMaster_();
}

bool xdesk::platform::PtyTerminal::start(const QString& shell, const int cols, const int rows, const QMap<QString, QString>& env)
{
    const QString program = shell.isEmpty() ? default_shell() : shell;
    const std::string path = program.toStdString();
    const std::string argv0 = "-" + QFileInfo(program).fileName().toStdString(); // login shell
    const std::string home = QDir::homePath().toStdString();
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    std::vector<std::string> envStrings;
    std::vector<char*> envp;
    winsize size;
    int master = -1;
    pid_t pid = -1;

    if (!QFileInfo(program).isExecutable()) {
        this->_error = "shell not executable: " + program;
        return false;
    }

    // Everything the child needs is prepared before the fork (only async signal safe calls after it)
    environment.insert("TERM", "xterm-256color");
    environment.insert("COLORTERM", "truecolor");
    environment.insert("XDESK_TERMINAL", "1");
    for (auto it = env.constBegin(); it != env.constEnd(); ++it)
        environment.insert(it.key(), it.value());
    for (const QString& key: environment.keys())
        envStrings.push_back((key + "=" + environment.value(key)).toStdString());
    for (std::string& entry: envStrings)
        envp.push_back(entry.data());
    envp.push_back(nullptr);
    std::memset(&size, 0, sizeof(size));
    size.ws_col = static_cast<unsigned short>(std::clamp(cols, 2, 1000));
    size.ws_row = static_cast<unsigned short>(std::clamp(rows, 2, 1000));

    pid = ::forkpty(&master, nullptr, nullptr, &size);
    if (pid < 0) {
        this->_error = QString("forkpty failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    if (pid == 0) {
        // child
        sigset_t mask;
        sigemptyset(&mask);
        (void)sigprocmask(SIG_SETMASK, &mask, nullptr);
        for (const int sig: {SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGCHLD, SIGTSTP, SIGTTIN, SIGTTOU})
            (void)::signal(sig, SIG_DFL);
        close_other_fds();
        (void)::chdir(home.c_str());
        char* const argv[] = {const_cast<char*>(argv0.c_str()), nullptr};
        ::execve(path.c_str(), argv, envp.data());
        ::_exit(127);
    }

    // parent
    this->_master = master;
    this->_pid = pid;
    (void)::fcntl(master, F_SETFL, ::fcntl(master, F_GETFL) | O_NONBLOCK);
    (void)::fcntl(master, F_SETFD, FD_CLOEXEC);
    this->_reader = new QSocketNotifier(static_cast<qintptr>(master), QSocketNotifier::Read, this);
    QObject::connect(this->_reader, &QSocketNotifier::activated, this, [this](void) {this->onReadable_();});
    this->_writer = new QSocketNotifier(static_cast<qintptr>(master), QSocketNotifier::Write, this);
    this->_writer->setEnabled(false);
    QObject::connect(this->_writer, &QSocketNotifier::activated, this, [this](void) {this->onWritable_();});
    this->_reaper.start();
    return true;
}

void xdesk::platform::PtyTerminal::onReadable_(void)
{
    QByteArray data;
    char buffer[READ_CHUNK];

    while (this->_master >= 0) {
        const ssize_t n = ::read(this->_master, buffer, sizeof(buffer));
        if (n > 0) {
            data.append(buffer, static_cast<qsizetype>(n));
            if (data.size() >= 4 * READ_CHUNK) break; // let the event loop breathe
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        // EOF / EIO: every slave fd is closed (the shell exited)
        if (!data.isEmpty()) emit this->output(data);
        data.clear();
        this->closeMaster_();
        this->reap_(false);
        break;
    }
    if (!data.isEmpty()) emit this->output(data);
}

void xdesk::platform::PtyTerminal::onWritable_(void)
{
    while (this->_master >= 0 && !this->_pending.isEmpty()) {
        const ssize_t n = ::write(this->_master, this->_pending.constData(), static_cast<std::size_t>(this->_pending.size()));
        if (n > 0) {this->_pending.remove(0, static_cast<qsizetype>(n)); continue;}
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        this->_pending.clear(); // pty closed
        break;
    }
    if (this->_writer) this->_writer->setEnabled(!this->_pending.isEmpty() && this->_master >= 0);
}

void xdesk::platform::PtyTerminal::write(const QByteArray& data)
{
    if (this->_master < 0 || data.isEmpty()) return;
    this->_pending.append(data);
    this->onWritable_();
}

void xdesk::platform::PtyTerminal::resize(const int cols, const int rows)
{
    winsize size;

    if (this->_master < 0) return;
    std::memset(&size, 0, sizeof(size));
    size.ws_col = static_cast<unsigned short>(std::clamp(cols, 2, 1000));
    size.ws_row = static_cast<unsigned short>(std::clamp(rows, 2, 1000));
    (void)::ioctl(this->_master, TIOCSWINSZ, &size); // SIGWINCH sent by the kernel
}

void xdesk::platform::PtyTerminal::terminate(void)
{
    const pid_t pid = static_cast<pid_t>(this->_pid);

    if (this->_finished || pid <= 0) return;
    signal_session(pid, SIGHUP);
    signal_session(pid, SIGCONT); // stopped jobs must see the SIGHUP
    QTimer::singleShot(KILL_DELAY, this, [this, pid](void) {
        if (this->_finished) return;
        signal_session(pid, SIGKILL);
        this->reap_(true);
    });
}

void xdesk::platform::PtyTerminal::reap_(const bool block)
{
    int status = 0;
    pid_t result = 0;
    int code = 0;

    if (this->_finished || this->_pid <= 0) return;
    do {
        result = ::waitpid(static_cast<pid_t>(this->_pid), &status, block ? 0 : WNOHANG);
    } while (result < 0 && errno == EINTR);
    if (result == 0) return; // still running
    if (result == static_cast<pid_t>(this->_pid)) {
        if (WIFEXITED(status)) code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) code = 128 + WTERMSIG(status);
    } else {
        code = -1; // not our child anymore (reaped elsewhere)
    }
    this->_finished = true;
    this->_reaper.stop();
    if (this->_master >= 0) this->onReadable_(); // last output
    this->closeMaster_();
    emit this->finished(code);
}

void xdesk::platform::PtyTerminal::closeMaster_(void)
{
    if (this->_reader) {this->_reader->setEnabled(false); this->_reader->deleteLater(); this->_reader = nullptr;}
    if (this->_writer) {this->_writer->setEnabled(false); this->_writer->deleteLater(); this->_writer = nullptr;}
    if (this->_master >= 0) {::close(this->_master); this->_master = -1;}
    this->_pending.clear();
}
