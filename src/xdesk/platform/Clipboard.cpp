/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/11/2026 by @author Tsukini

File Name:
##  @file Clipboard.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Encapsulation
#include <utils/utils.hpp>
#include "xdesk/platform/Clipboard.hpp"
#include <QGuiApplication>
#include <QMetaObject>
#include <QClipboard>
#include <QPointer>
#if defined(__linux__) && __has_include(<X11/extensions/Xfixes.h>)
    #define XDESK_X11_CLIPBOARD
    #include <X11/extensions/Xfixes.h>
    #include <X11/Xatom.h>
    #include <X11/Xlib.h>
    #include <QLibrary>
    #include <unistd.h>
    #include <atomic>
    #include <thread>
    #include <poll.h>
    #include <mutex>
#endif

//----------------------------------------------------------------//
/* DEFINE */

#define CLIP_MAX (1 << 20) // characters of a clipboard text at most

//----------------------------------------------------------------//
/* STRUCT */

#ifdef XDESK_X11_CLIPBOARD
    // Entry points of libX11 / libXfixes (loaded at run time: nothing linked, nothing required)
    struct X11Api {
        Display* (*openDisplay)(const char*) = nullptr;
        int (*closeDisplay)(Display*) = nullptr;
        Atom (*internAtom)(Display*, const char*, Bool) = nullptr;
        Window (*defaultRoot)(Display*) = nullptr;
        Window (*createWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long) = nullptr;
        int (*pending)(Display*) = nullptr;
        int (*nextEvent)(Display*, XEvent*) = nullptr;
        int (*convertSelection)(Display*, Atom, Atom, Atom, Window, Time) = nullptr;
        int (*getProperty)(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long*, unsigned long*, unsigned char**) = nullptr;
        int (*changeProperty)(Display*, Window, Atom, Atom, int, int, const unsigned char*, int) = nullptr;
        int (*setOwner)(Display*, Atom, Window, Time) = nullptr;
        Window (*getOwner)(Display*, Atom) = nullptr;
        Status (*sendEvent)(Display*, Window, Bool, long, XEvent*) = nullptr;
        int (*flush)(Display*) = nullptr;
        int (*connection)(Display*) = nullptr;
        int (*free)(void*) = nullptr;
        Bool (*fixesQuery)(Display*, int*, int*) = nullptr;
        XErrorHandler (*setErrorHandler)(XErrorHandler) = nullptr;
        void (*setIOErrorExit)(Display*, void (*)(Display*, void*), void*) = nullptr; // libX11 1.7+
        void (*fixesSelect)(Display*, Window, Atom, unsigned long) = nullptr;
    };

    // The bridge: its own display connection and thread, the text set by xdesk served to who pastes it
    struct xdesk::platform::X11Clipboard {
        X11Api x;
        QLibrary libX11{"X11", 6};
        QLibrary libXfixes{"Xfixes", 3};
        Display* display = nullptr;
        Window window = 0;
        Atom clipboard = 0;
        Atom utf8 = 0;
        Atom targets = 0;
        Atom property = 0;
        int fixesEvent = 0;
        utils::encapsulation::Pipe wake; // setText wakes the thread
        std::mutex lock;
        QByteArray owned; // the text this window owns (utf-8)
        QByteArray next; // the text to own at the next wake
        bool hasNext = false;
        std::atomic<bool> stop = false;
        std::atomic<bool> dead = false; // the X server went away (Xwayland restarted): the bridge stops, the host lives
        std::thread thread;
        QPointer<Clipboard> owner;

        _cold bool open(void);
        _cold void run(void);
        _cold void serve(const XSelectionRequestEvent& request);
        _cold void read(void);
        _cold ~X11Clipboard();
    };
#else
    struct xdesk::platform::X11Clipboard {}; // no X11 headers: the bridge is never used
#endif

//----------------------------------------------------------------//
/* TOOLS */

#ifdef XDESK_X11_CLIPBOARD
    // An X error is never fatal to the host (the request it answers is simply lost)
    _cold static int ignore_x_error(Display*, XErrorEvent*)
    {
        return 0;
    }

    // The X server went away: the bridge stops (Xlib does not exit when this returns)
    _cold static void x_gone(Display*, void* data)
    {
        static_cast<xdesk::platform::X11Clipboard*>(data)->dead = true;
    }

    // Every entry point found, a display opened and the XFixes extension present
    _cold bool xdesk::platform::X11Clipboard::open(void)
    {
        int error = 0;

        if (!this->libX11.load() || !this->libXfixes.load()) return false;
        this->x.openDisplay = reinterpret_cast<decltype(this->x.openDisplay)>(this->libX11.resolve("XOpenDisplay"));
        this->x.closeDisplay = reinterpret_cast<decltype(this->x.closeDisplay)>(this->libX11.resolve("XCloseDisplay"));
        this->x.internAtom = reinterpret_cast<decltype(this->x.internAtom)>(this->libX11.resolve("XInternAtom"));
        this->x.defaultRoot = reinterpret_cast<decltype(this->x.defaultRoot)>(this->libX11.resolve("XDefaultRootWindow"));
        this->x.createWindow = reinterpret_cast<decltype(this->x.createWindow)>(this->libX11.resolve("XCreateSimpleWindow"));
        this->x.pending = reinterpret_cast<decltype(this->x.pending)>(this->libX11.resolve("XPending"));
        this->x.nextEvent = reinterpret_cast<decltype(this->x.nextEvent)>(this->libX11.resolve("XNextEvent"));
        this->x.convertSelection = reinterpret_cast<decltype(this->x.convertSelection)>(this->libX11.resolve("XConvertSelection"));
        this->x.getProperty = reinterpret_cast<decltype(this->x.getProperty)>(this->libX11.resolve("XGetWindowProperty"));
        this->x.changeProperty = reinterpret_cast<decltype(this->x.changeProperty)>(this->libX11.resolve("XChangeProperty"));
        this->x.setOwner = reinterpret_cast<decltype(this->x.setOwner)>(this->libX11.resolve("XSetSelectionOwner"));
        this->x.getOwner = reinterpret_cast<decltype(this->x.getOwner)>(this->libX11.resolve("XGetSelectionOwner"));
        this->x.sendEvent = reinterpret_cast<decltype(this->x.sendEvent)>(this->libX11.resolve("XSendEvent"));
        this->x.flush = reinterpret_cast<decltype(this->x.flush)>(this->libX11.resolve("XFlush"));
        this->x.connection = reinterpret_cast<decltype(this->x.connection)>(this->libX11.resolve("XConnectionNumber"));
        this->x.free = reinterpret_cast<decltype(this->x.free)>(this->libX11.resolve("XFree"));
        this->x.fixesQuery = reinterpret_cast<decltype(this->x.fixesQuery)>(this->libXfixes.resolve("XFixesQueryExtension"));
        this->x.fixesSelect = reinterpret_cast<decltype(this->x.fixesSelect)>(this->libXfixes.resolve("XFixesSelectSelectionInput"));
        this->x.setErrorHandler = reinterpret_cast<decltype(this->x.setErrorHandler)>(this->libX11.resolve("XSetErrorHandler"));
        this->x.setIOErrorExit = reinterpret_cast<decltype(this->x.setIOErrorExit)>(this->libX11.resolve("XSetIOErrorExitHandler"));
        if (!this->x.openDisplay || !this->x.closeDisplay || !this->x.internAtom || !this->x.defaultRoot || !this->x.createWindow || !this->x.pending
            || !this->x.nextEvent || !this->x.convertSelection || !this->x.getProperty || !this->x.changeProperty || !this->x.setOwner || !this->x.getOwner
            || !this->x.sendEvent || !this->x.flush || !this->x.connection || !this->x.free || !this->x.fixesQuery || !this->x.fixesSelect || !this->x.setErrorHandler || !this->x.setIOErrorExit)
            return false;
        // Xlib exits the process on an error by default: a client destroying its window while it asks the clipboard
        // (BadWindow) or Xwayland going away would kill the host (and ban its sessions at the next start)
        (void)this->x.setErrorHandler(ignore_x_error);
        this->display = this->x.openDisplay(nullptr);
        if (!this->display) return false;
        this->x.setIOErrorExit(this->display, x_gone, this);
        if (!this->x.fixesQuery(this->display, &this->fixesEvent, &error)) return false;
        this->window = this->x.createWindow(this->display, this->x.defaultRoot(this->display), 0, 0, 1, 1, 0, 0, 0);
        this->clipboard = this->x.internAtom(this->display, "CLIPBOARD", False);
        this->utf8 = this->x.internAtom(this->display, "UTF8_STRING", False);
        this->targets = this->x.internAtom(this->display, "TARGETS", False);
        this->property = this->x.internAtom(this->display, "XDESK_CLIPBOARD", False);
        this->x.fixesSelect(this->display, this->window, this->clipboard, XFixesSetSelectionOwnerNotifyMask);
        if (this->x.getOwner(this->display, this->clipboard) != None) this->x.convertSelection(this->display, this->clipboard, this->utf8, this->property, this->window, CurrentTime); // the text already there
        this->x.flush(this->display);
        try {
        this->wake.trigger();
    } catch (...) {
        return false;
    }
        this->thread = std::thread([this]() {this->run();});
        return true;
    }

    // Events of the display and the wake pipe until stop
    _cold void xdesk::platform::X11Clipboard::run(void)
    {
        pollfd fds[2] = {{this->x.connection(this->display), POLLIN, 0}, {this->wake.getRead(), POLLIN, 0}};
        XEvent event;
        char drain[64];

        while (!this->stop && !this->dead) {
            while (!this->dead && this->x.pending(this->display)) {
                this->x.nextEvent(this->display, &event);
                if (event.type == this->fixesEvent + XFixesSelectionNotify) {
                    const XFixesSelectionNotifyEvent& notify = reinterpret_cast<const XFixesSelectionNotifyEvent&>(event);
                    if (notify.owner != this->window && notify.owner != None) this->x.convertSelection(this->display, this->clipboard, this->utf8, this->property, this->window, CurrentTime);
                } else if (event.type == SelectionNotify && event.xselection.property != None) {
                    this->read();
                } else if (event.type == SelectionRequest) {
                    this->serve(event.xselectionrequest);
                } else if (event.type == SelectionClear) {
                    const std::lock_guard<std::mutex> guard(this->lock);
                    this->owned.clear();
                }
            }
            {
                const std::lock_guard<std::mutex> guard(this->lock);
                if (this->hasNext) {
                    this->owned = this->next;
                    this->hasNext = false;
                    this->x.setOwner(this->display, this->clipboard, this->window, CurrentTime);
                }
            }
            this->x.flush(this->display);
            if (poll(fds, 2, 500) > 0 && (fds[1].revents & POLLIN)) (void)!::read(this->wake.getRead(), drain, sizeof(drain));
        }
    }

    // Who pastes asks the text this window owns: the list of formats, or the utf-8 text
    _cold void xdesk::platform::X11Clipboard::serve(const XSelectionRequestEvent& request)
    {
        XEvent reply = {};
        XSelectionEvent& answer = reply.xselection;
        const Atom formats[2] = {this->targets, this->utf8};
        QByteArray text;

        {
            const std::lock_guard<std::mutex> guard(this->lock);
            text = this->owned;
        }
        answer.type = SelectionNotify;
        answer.requestor = request.requestor;
        answer.selection = request.selection;
        answer.target = request.target;
        answer.time = request.time;
        answer.property = request.property != None ? request.property : request.target;
        if (request.target == this->targets)
            this->x.changeProperty(this->display, request.requestor, answer.property, XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(formats), 2);
        else if (request.target == this->utf8 || request.target == XA_STRING)
            this->x.changeProperty(this->display, request.requestor, answer.property, request.target, 8, PropModeReplace, reinterpret_cast<const unsigned char*>(text.constData()), static_cast<int>(text.size()));
        else
            answer.property = None;
        this->x.sendEvent(this->display, request.requestor, False, 0, &reply);
    }

    // The text of the new owner (INCR transfers, larger than a request, are skipped)
    _cold void xdesk::platform::X11Clipboard::read(void)
    {
        Atom type = 0;
        int format = 0;
        unsigned long items = 0;
        unsigned long after = 0;
        unsigned char* data = nullptr;
        QString text;

        if (this->x.getProperty(this->display, this->window, this->property, 0, CLIP_MAX, True, AnyPropertyType, &type, &format, &items, &after, &data) != Success) return;
        if (data && format == 8 && (type == this->utf8 || type == XA_STRING)) text = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<qsizetype>(items));
        if (data) this->x.free(data);
        if (text.isEmpty() || !this->owner) return;
        QMetaObject::invokeMethod(this->owner.data(), [target = this->owner, text]() {if (target) target->received_(text);}, Qt::QueuedConnection);
    }

    _cold xdesk::platform::X11Clipboard::~X11Clipboard()
    {
        this->stop = true;
        if (this->wake.getWrite() >= 0) (void)!write(this->wake.getWrite(), "x", 1);
        if (this->thread.joinable()) this->thread.join();
        if (this->display && !this->dead) this->x.closeDisplay(this->display);
    }
#endif

//----------------------------------------------------------------//
/* PRE-FUNCTION */

// Set by a remote session: owned at once, no changed() for it
void xdesk::platform::Clipboard::setText(const QString& text)
{
    const QString value = text.left(CLIP_MAX);

    if (value.isEmpty()) return;
    this->_last = value;
    #ifdef XDESK_X11_CLIPBOARD
        if (this->_x11) {
            const std::lock_guard<std::mutex> guard(this->_x11->lock);
            this->_x11->next = value.toUtf8();
            this->_x11->hasNext = true;
            (void)!write(this->_x11->wake.getWrite(), "x", 1);
            return;
        }
    #endif
    if (this->_backend == "qt") QGuiApplication::clipboard()->setText(value);
}

QString xdesk::platform::Clipboard::text(void) const
{
    if (this->_backend == "qt") return QGuiApplication::clipboard()->text().left(CLIP_MAX);
    return this->_last;
}

void xdesk::platform::Clipboard::received_(const QString& text)
{
    if (text.isEmpty() || text == this->_last) return;
    this->_last = text;
    emit this->changed(text);
}

void xdesk::platform::Clipboard::onQt_(void)
{
    const QString text = QGuiApplication::clipboard()->text().left(CLIP_MAX);

    this->received_(text);
}

//----------------------------------------------------------------//
/* CONSTRUCTOR */

// Qt where it sees the clipboard without the focus (Windows, macOS, X11), the X11 bridge on Wayland or without a GUI
xdesk::platform::Clipboard::Clipboard(QObject* parent) : QObject(parent)
{
    const bool gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
    const bool wayland = gui ? QGuiApplication::platformName() == "wayland" : qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland";

    if (gui && !wayland) {
        this->_backend = "qt";
    } else {
        #ifdef XDESK_X11_CLIPBOARD
            if (qEnvironmentVariableIsSet("DISPLAY")) {
                this->_x11 = std::make_unique<X11Clipboard>();
                this->_x11->owner = this;
                if (this->_x11->open()) {this->_backend = "x11"; return;}
                this->_x11.reset();
            }
        #endif
        if (gui) this->_backend = "qt"; // Wayland without Xwayland: only while a window of xdesk has the focus
    }
    if (this->_backend == "qt") QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &Clipboard::onQt_);
}

//----------------------------------------------------------------//
/* DESTRUCTOR */

xdesk::platform::Clipboard::~Clipboard() = default;
