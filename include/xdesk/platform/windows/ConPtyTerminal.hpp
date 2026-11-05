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
##  @file ConPtyTerminal.hpp

File Description:
##  Windows pseudo terminal (ConPTY), shell in a job object killed with the terminal
\**************************************************************/

#ifndef CONPTYTERMINAL_H
    #define CONPTYTERMINAL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00 // ConPTY: Windows 10 1809+
    #endif
    #include <windows.h> // xstyle: ignore CPP-INCLUDE-ORDER (Windows headers before libutils: _unused clash)
    #define _Attribute
    #include <utils/utils.hpp>          // _hot, _cold, _nodiscard
    #include "../ITerminal.hpp"         // xdesk::platform::ITerminal
    #include <QThread>                  // QThread
    #include <QString>                  // QString
    #include <atomic>                   // std::atomic

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class ConPtyTerminal final: public ITerminal {
    private:
        HPCON _console = nullptr; // pseudo console
        HANDLE _input = INVALID_HANDLE_VALUE; // we write the keyboard here
        HANDLE _output = INVALID_HANDLE_VALUE; // we read the screen here
        HANDLE _process = nullptr;
        HANDLE _job = nullptr; // kill on close: the whole process tree
        DWORD _pid = 0;
        QThread* _reader = nullptr; // reads _output
        QThread* _waiter = nullptr; // waits the process
        std::atomic<bool> _closing = false;
        QString _error = "";

    public:
        // ---------- Pre-Function -------- //
        _cold bool start(const QString& shell, const int cols, const int rows, const QMap<QString, QString>& env) override;
        _hot void write(const QByteArray& data) override;
        _cold void resize(const int cols, const int rows) override;
        _cold void terminate(void) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard qint64 pid(void) const override    {return this->_pid ? static_cast<qint64>(this->_pid) : -1;};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ---------- Constructor --------- //
        explicit ConPtyTerminal(QObject* parent = nullptr): ITerminal(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~ConPtyTerminal() override {this->terminate();};
};

} // namespace end
#endif /* CONPTYTERMINAL_H */
