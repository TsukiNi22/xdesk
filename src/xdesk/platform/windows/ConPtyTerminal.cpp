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
##  @file ConPtyTerminal.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
#endif
#include <windows.h> // xstyle: ignore CPP-INCLUDE-ORDER (Windows headers before libutils: _unused clash)
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/windows/ConPtyTerminal.hpp"
#include <QProcessEnvironment>
#include <QByteArray>
#include <QPointer>
#include <vector>

bool xdesk::platform::ConPtyTerminal::start(const QString& shell, const int cols, const int rows, const QMap<QString, QString>& env)
{
    HANDLE inRead = INVALID_HANDLE_VALUE;
    HANDLE outWrite = INVALID_HANDLE_VALUE;
    STARTUPINFOEXW startup{};
    PROCESS_INFORMATION process{};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    SIZE_T attributes = 0;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    std::wstring block;
    std::wstring command = shell.toStdWString();
    QPointer<ConPtyTerminal> self = this;

    // Pipes + pseudo console
    if (!CreatePipe(&inRead, &this->_input, nullptr, 0) || !CreatePipe(&this->_output, &outWrite, nullptr, 0)) {this->_error = "CreatePipe failed"; return false;}
    if (FAILED(CreatePseudoConsole(COORD{static_cast<SHORT>(cols), static_cast<SHORT>(rows)}, inRead, outWrite, 0, &this->_console))) {
        this->_error = "CreatePseudoConsole failed (Windows 10 1809+ needed)";
        CloseHandle(inRead);
        CloseHandle(outWrite);
        return false;
    }
    CloseHandle(inRead); // owned by the pseudo console now
    CloseHandle(outWrite);

    // Startup info with the pseudo console attribute
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributes);
    startup.lpAttributeList = static_cast<PPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attributes));
    InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributes);
    UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, this->_console, sizeof(HPCON), nullptr, nullptr);

    // Environment block (KEY=VALUE\0...\0\0)
    for (QMap<QString, QString>::const_iterator it = env.cbegin(); it != env.cend(); ++it)
        environment.insert(it.key(), it.value());
    for (const QString& entry: environment.toStringList())
        block += entry.toStdWString() + L'\0';
    block += L'\0';

    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
        block.data(), nullptr, &startup.StartupInfo, &process)) {
        this->_error = "CreateProcess failed for " + shell;
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
        return false;
    }
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);

    // Job: every child of the shell dies with the terminal
    this->_job = CreateJobObjectW(nullptr, nullptr);
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(this->_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    AssignProcessToJobObject(this->_job, process.hProcess);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    this->_process = process.hProcess;
    this->_pid = process.dwProcessId;

    // Output reader
    this->_reader = QThread::create([self, output = this->_output]() {
        char buffer[8192];
        DWORD count = 0;
        while (ReadFile(output, buffer, sizeof(buffer), &count, nullptr) && count > 0) {
            const QByteArray data(buffer, static_cast<qsizetype>(count));
            QMetaObject::invokeMethod(self, [self, data]() {if (self) emit self->output(data);}, Qt::QueuedConnection);
        }
    });
    this->_reader->start();

    // Process end
    this->_waiter = QThread::create([self, process = this->_process]() {
        DWORD code = 0;
        WaitForSingleObject(process, INFINITE);
        GetExitCodeProcess(process, &code);
        QMetaObject::invokeMethod(self, [self, code]() {if (self && !self->_closing) emit self->finished(static_cast<int>(code));}, Qt::QueuedConnection);
    });
    this->_waiter->start();
    return true;
}

void xdesk::platform::ConPtyTerminal::write(const QByteArray& data)
{
    DWORD written = 0;

    if (this->_input != INVALID_HANDLE_VALUE) WriteFile(this->_input, data.constData(), static_cast<DWORD>(data.size()), &written, nullptr);
}

void xdesk::platform::ConPtyTerminal::resize(const int cols, const int rows)
{
    if (this->_console) ResizePseudoConsole(this->_console, COORD{static_cast<SHORT>(cols), static_cast<SHORT>(rows)});
}

void xdesk::platform::ConPtyTerminal::terminate(void)
{
    if (this->_closing.exchange(true)) return;

    // Job closed = process tree killed, then the console (ends the reader)
    if (this->_job) {CloseHandle(this->_job); this->_job = nullptr;}
    if (this->_console) {ClosePseudoConsole(this->_console); this->_console = nullptr;}
    if (this->_input != INVALID_HANDLE_VALUE) {CloseHandle(this->_input); this->_input = INVALID_HANDLE_VALUE;}
    for (QThread* thread: {this->_reader, this->_waiter}) {
        if (!thread) continue;
        thread->wait(2000);
        delete thread;
    }
    this->_reader = this->_waiter = nullptr;
    if (this->_output != INVALID_HANDLE_VALUE) {CloseHandle(this->_output); this->_output = INVALID_HANDLE_VALUE;}
    if (this->_process) {CloseHandle(this->_process); this->_process = nullptr;}
}
