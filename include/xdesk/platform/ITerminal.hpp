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
##  @file ITerminal.hpp

File Description:
##  Interface of the pseudo terminals (Unix forkpty, Windows ConPTY)
\**************************************************************/

#ifndef ITERMINAL_H
    #define ITERMINAL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <QMap>             // QMap

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

// Lives in the thread that created it (the host thread), output read with a QSocketNotifier / a reader thread
class ITerminal: public QObject {
    Q_OBJECT

    public:
        // ---------- Pre-Function -------- //
        _cold virtual bool start(const QString& shell, const int cols, const int rows, const QMap<QString, QString>& env) = 0; // false = error()
        _hot virtual void write(const QByteArray& data) = 0;
        _cold virtual void resize(const int cols, const int rows) = 0;
        _cold virtual void terminate(void) = 0; // kill the process tree of the shell
        _cold _nodiscard virtual qint64 pid(void) const = 0; // shell pid (-1 = not running)
        _cold _nodiscard virtual QString error(void) const = 0;

        // ---------- Constructor --------- //
        explicit ITerminal(QObject* parent = nullptr): QObject(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~ITerminal() override = default;

    signals:
        void output(const QByteArray& data);
        void finished(const int code);
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard ITerminal* create_terminal(QObject* parent = nullptr); // forkpty | ConPTY
_cold _nodiscard QString default_shell(void); // $SHELL | /bin/sh | %COMSPEC% | powershell.exe

} // namespace end
#endif /* ITERMINAL_H */
