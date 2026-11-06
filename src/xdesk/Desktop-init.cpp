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
##  @file Desktop-init.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#define _Verbose
#define _Arguments
#include <utils/utils.hpp>
#include "xdesk/Desktop.hpp"
#include <optional>
#include <string>
#include <vector>

_nodiscard static std::optional<std::string> VerboseParsingHook(const std::string& option)
{
    if (option == "none" || option == "basic" || option == "advanced" || option == "debug") return std::nullopt;
    return "Invalid verbose level, should be (none|basic|advanced|debug), but got: " + option;
}

_nodiscard static std::optional<std::string> ExposeParsingHook(const std::string& option)
{
    for (const char* mode: {"lan", "tailscale", "upnp", "bore", "cloudflared", "ngrok", "manual"})
        if (option == mode) return std::nullopt;
    return "Invalid exposure, should be (lan|tailscale|upnp|bore|cloudflared|ngrok|manual), but got: " + option;
}

void xdesk::Desktop::init(const int argc, const char* const argv[])
{
    static const std::vector<std::string> COMMANDS = {"host", "connect", "viewer", "scan", "devices", "unban", "paths"};
    utils::arguments::ArgParser parser = utils::arguments::ArgParser("xdesk",
        "Remote desktop & terminal, secured end to end (TLS 1.3 + ML-KEM post-quantum key exchange, Argon2id codes).\n"
        "Commands:\n"
        "  xdesk [host]               share this PC (graphical panel, or terminal panel with --tty / without display)\n"
        "  xdesk connect [address]    control a PC (graphical viewer, or terminal with --tty: no address = its connect page; --screen starts on the screen)\n"
        "  xdesk viewer               graphical viewer with its connect page\n"
        "  xdesk scan                 list the hosts announced on the LAN\n"
        "  xdesk devices              list the known / trusted / banned devices\n"
        "  xdesk unban <id|address>   unban (only on this PC, typed confirmation, host stopped)\n"
        "  xdesk paths                print the configuration, data and log folders");
    std::vector<std::string> args;

    this->_argc = argc;
    this->_argv = const_cast<char**>(argv);

    // Command and its target first (the parser only sees the flags)
    args.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (i == 1 && std::find(COMMANDS.begin(), COMMANDS.end(), arg) != COMMANDS.end()) {this->_command = arg; continue;}
        if ((this->_command == "connect" || this->_command == "unban") && this->_target.empty() && !arg.starts_with('-') && (i == 2 || args.size() == 1 || !args.back().starts_with('-'))) {
            this->_target = arg;
            continue;
        }
        args.push_back(arg);
    }

    // Flags
    parser.setDefaultUsage();
    parser.setFlag("verbose", {"v", "", "verbose", "XDESK_VERBOSE"}, {{"level", true, VerboseParsingHook}}, "Verbose level none|basic|advanced|debug (default: none)");
    parser.setFlag("tty", {"t", "", "tty", ""}, {}, "Terminal interface (host panel or client)");
    parser.setFlag("gui", {"g", "", "gui", ""}, {}, "Graphical interface (Qt)");
    parser.setFlag("port", {"p", "", "port", "XDESK_PORT"}, {{"port", true, utils::arguments::defaultInt32ParsingHook}}, "Port (default 47800)");
    parser.setFlag("code", {"c", "", "code", "XDESK_CODE"}, {{"code", true, utils::arguments::defaultTrueParsingHook}}, "Pairing code shown by the host (connect)");
    parser.setFlag("screen", {"s", "", "screen", ""}, {}, "Terminal client: show the remote screen (truecolor half blocks)");
    parser.setFlag("view-only", {"", "", "view-only", ""}, {}, "Never send keyboard / mouse (connect)");
    parser.setFlag("expose", {"e", "", "expose", "XDESK_EXPOSE"}, {{"mode", true, ExposeParsingHook}}, "Exposure for this run: lan|tailscale|upnp|bore|cloudflared|ngrok|manual");
    parser.setFlag("regen-cert", {"", "", "regen-cert", ""}, {}, "New host identity (the clients will confirm the new fingerprint)");
    parser.setFlag("version", {"V", "", "version", ""}, {}, "Print the version");

    // Extract settings
    std::vector<const char*> cargs;
    for (const std::string& a: args)
        cargs.push_back(a.c_str());
    utils::arguments::ParsedUsages usages = parser.parse(static_cast<int>(cargs.size()), cargs.data());
    set_verbose(None)
    for (const auto& [id, _, options]: usages.front().arguments) {
        const std::string& value = (options.empty() ? "" : options.front());
        if (id == "verbose") {
            if      (value == "none")     set_verbose(None)
            else if (value == "basic")    set_verbose(Basic)
            else if (value == "advanced") set_verbose(Advanced)
            else if (value == "debug")    set_verbose(Debug)
        } else if (id == "tty")        this->_tty = true;
        else if (id == "gui")        this->_gui = true;
        else if (id == "port")       this->_port = std::stoi(value);
        else if (id == "code")       this->_code = value;
        else if (id == "screen")     this->_screen = true;
        else if (id == "view-only")  this->_viewOnly = true;
        else if (id == "expose")     this->_expose = value;
        else if (id == "regen-cert") this->_regenCert = true;
        else if (id == "version")    this->_command = "version";
    }
    if (this->_port == 0 || this->_port > 65535) throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "Invalid port " + std::to_string(this->_port));
    if (this->_command == "unban" && this->_target.empty()) throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "xdesk unban <device id | address>");
    this->_config.load();
    onBasicVerbose("xdesk " XDESK_VERSION " - command " + this->_command);
}
