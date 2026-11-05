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
##  @file Desktop.hpp

File Description:
##  Core of xdesk: command line, mode selection (host / viewer, gui / tty) and local admin commands
\**************************************************************/

#ifndef DESKTOP_H
    #define DESKTOP_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "client/Client.hpp"        // xdesk::client::Options
    #include "core/Config.hpp"          // xdesk::core::Config
    #include <string>                   // std::string
    #include <vector>                   // std::vector

namespace xdesk { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Desktop {
    private:
        core::Config _config;
        std::string _command = "host"; // host | connect | viewer | scan | devices | unban | paths
        std::string _target = ""; // connect: address | unban: device id or address
        bool _tty = false; // terminal interface
        bool _gui = false; // graphical interface (forced)
        bool _screen = false; // tty client: show the screen instead of a terminal
        bool _viewOnly = false;
        bool _regenCert = false;
        int _port = -1; // -1 = config / default
        std::string _code = "";
        std::string _expose = ""; // exposure for this run
        int _argc = 0;
        char** _argv = nullptr;
        int _exit = 0; // exit code of run()

        // ---------- Pre-Function -------- //
        void runHost_(void);
        void runConnect_(void);
        void runScan_(void);
        void runDevices_(void);
        void runUnban_(void);

    public:
        // ---------- Pre-Function -------- //
        void init(const int argc, const char* const argv[]);
        void run(void);

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int exit(void) const {return this->_exit;};

        // ------------ Operator ---------- //
        Desktop& operator=(const Desktop& other) = delete;
        Desktop& operator=(Desktop&& other) = delete;

        // ---------- Constructor --------- //
        Desktop() = default;
        Desktop(const Desktop& other) = delete;
        Desktop(Desktop&& other) = delete;

        // ----------- Destructor --------- //
        ~Desktop() = default;
};

} // namespace end
#endif /* DESKTOP_H */
