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
##  @file PtyTerminal.hpp

File Description:
##  Unix pseudo terminal (forkpty) of a remote session
\**************************************************************/

#ifndef PTYTERMINAL_H
    #define PTYTERMINAL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../ITerminal.hpp"     // xdesk::platform::ITerminal
    #include <QSocketNotifier>      // QSocketNotifier
    #include <QByteArray>           // QByteArray
    #include <QString>              // QString
    #include <QTimer>               // QTimer
    #include <QMap>                 // QMap

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class PtyTerminal final: public ITerminal {
    Q_OBJECT

    private:
        int _master = -1; // master side of the pty (non blocking)
        qint64 _pid = -1; // shell pid (process group leader)
        QSocketNotifier* _reader = nullptr;
        QSocketNotifier* _writer = nullptr; // enabled while _pending is not empty
        QByteArray _pending; // input not written yet (pty full)
        QTimer _reaper; // waitpid polling (a background job can keep the slave open)
        bool _finished = false;
        QString _error = "";

        // ---------- Pre-Function -------- //
        void onReadable_(void);
        void onWritable_(void);
        void reap_(const bool block);
        void closeMaster_(void);

    public:
        // ---------- Pre-Function -------- //
        _cold bool start(const QString& shell, const int cols, const int rows, const QMap<QString, QString>& env) override;
        _hot void write(const QByteArray& data) override;
        _cold void resize(const int cols, const int rows) override;
        _cold void terminate(void) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard qint64 pid(void) const override    {return this->_finished ? -1 : this->_pid;};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ---------- Constructor --------- //
        explicit PtyTerminal(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~PtyTerminal() override; // shell killed
};

} // namespace end
#endif /* PTYTERMINAL_H */
