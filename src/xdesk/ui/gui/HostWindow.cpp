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
##  @file HostWindow.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/gui/HostWindow.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/ui/gui/Theme.hpp"
#include <qrcodegen/qrcodegen.hpp>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QApplication>
#include <QInputDialog>
#include <QButtonGroup>
#include <QHeaderView>
#include <QFormLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QScrollArea>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <functional>
#include <QClipboard>
#include <algorithm>
#include <QDateTime>
#include <QHostInfo>
#include <QSplitter>
#include <QPainter>
#include <QScreen>
#include <QDialog>
#include <QAction>
#include <QImage>
#include <QMenu>
#include <QFont>
#include <QUrl>

//----------------------------------------------------------------//
// Small builders

static QFrame* make_card(QWidget* parent = nullptr)
{
    QFrame* card = new QFrame(parent);

    card->setObjectName("card");
    return card;
}

static QLabel* make_label(const QString& text, const QString& object = "", QWidget* parent = nullptr)
{
    QLabel* label = new QLabel(object == "subtitle" ? text.toUpper() : text, parent); // section labels in small capitals

    if (!object.isEmpty()) label->setObjectName(object);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

static QPushButton* make_button(const QString& text, const QString& kind = "", const QString& icon = "")
{
    QPushButton* button = new QPushButton(text);

    if (!kind.isEmpty()) button->setProperty("kind", kind);
    if (!icon.isEmpty()) button->setIcon(xdesk::ui::gui::Theme::icon(icon, kind == "primary" ? xdesk::ui::gui::Theme::palette().accentText : QColor()));
    button->setProperty("icon", icon); // drawn again when the theme changes
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

// Style property: the style sheet is applied again only when the value changes (no flicker on the 1 Hz refresh)
static void set_prop(QWidget* widget, const char* name, const QVariant& value)
{
    if (widget->property(name) == value) return;
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

static void set_tone(QLabel* label, const QString& tone)
{
    set_prop(label, "tone", tone);
}

// Card with a header strip (title in small capitals, actions on the right): the caller lays out the body
static QFrame* make_panel(const QString& title, QWidget*& body, const QList<QWidget*>& actions = {}, QLabel** titleLabel = nullptr)
{
    QFrame* card = make_card();
    QVBoxLayout* layout = new QVBoxLayout(card);
    QWidget* head = new QWidget(card);
    QHBoxLayout* headLayout = new QHBoxLayout(head);
    QLabel* label = make_label(title, "subtitle");

    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    head->setObjectName("cardHead");
    head->setAttribute(Qt::WA_StyledBackground, true);
    headLayout->setContentsMargins(16, 6, 10, 6);
    headLayout->setSpacing(6);
    headLayout->addWidget(label);
    headLayout->addStretch();
    for (QWidget* action: actions)
        headLayout->addWidget(action);
    body = new QWidget(card);
    layout->addWidget(head);
    layout->addWidget(body, 1);
    if (titleLabel) *titleLabel = label;
    return card;
}

// Title of a page, one muted line under it, a widget on the right
static QWidget* make_page_head(const QString& title, const QString& text, QWidget* right = nullptr, QLabel** textLabel = nullptr)
{
    QWidget* head = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(head);
    QVBoxLayout* texts = new QVBoxLayout();
    QLabel* line = make_label(text, "muted");

    head->setObjectName("pageHead");
    layout->setContentsMargins(0, 0, 0, 6);
    texts->setSpacing(3);
    texts->addWidget(make_label(title, "title"));
    line->setWordWrap(true);
    texts->addWidget(line);
    layout->addLayout(texts, 1);
    if (right) layout->addWidget(right, 0, Qt::AlignTop);
    if (textLabel) *textLabel = line;
    return head;
}

static QTableWidget* make_table(const QStringList& headers)
{
    QTableWidget* table = new QTableWidget(0, static_cast<int>(headers.size()));

    QStringList upper;

    for (const QString& header: headers)
        upper.append(header.toUpper());
    table->setHorizontalHeaderLabels(upper);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(34);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setFrameShape(QFrame::NoFrame);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    xdesk::ui::gui::Theme::decorate(table);
    return table;
}

static QComboBox* make_exposure_box(void)
{
    QComboBox* box = new QComboBox();

    // The LAN is always open while sharing: each mode only adds one path
    box->addItem("LAN only", "lan");
    box->addItem("LAN + Tailscale", "tailscale");
    box->addItem("LAN + internet (UPnP router port)", "upnp");
    box->addItem("LAN + internet (bore.pub tunnel)", "bore");
    box->addItem("LAN + internet (Cloudflare tunnel)", "cloudflared");
    box->addItem("LAN + internet (ngrok tunnel)", "ngrok");
    box->addItem("LAN + internet (port forwarded by hand)", "manual");
    box->setItemData(0, "Only the devices on the same network", Qt::ToolTipRole);
    box->setItemData(1, "Also your devices signed in to the same tailnet (WireGuard)", Qt::ToolTipRole);
    box->setItemData(2, "Also anyone on the internet: xdesk asks the router to open the port", Qt::ToolTipRole);
    box->setItemData(3, "Also anyone on the internet, through bore.pub (TLS stays end to end)", Qt::ToolTipRole);
    box->setItemData(4, "Also anyone on the internet, through Cloudflare (the relay decrypts the traffic)", Qt::ToolTipRole);
    box->setItemData(5, "Also anyone on the internet, through ngrok (TLS stays end to end, account needed)", Qt::ToolTipRole);
    box->setItemData(6, "Also anyone on the internet, through the port you forwarded on the router", Qt::ToolTipRole);
    box->setMinimumWidth(250);
    return box;
}

static QString since_text(const qint64 ms)
{
    const qint64 seconds = (QDateTime::currentMSecsSinceEpoch() - ms) / 1000;

    if (ms <= 0) return "-";
    if (seconds < 60) return QString::number(seconds) + " s";
    if (seconds < 3600) return QString::number(seconds / 60) + " min";
    return QString::number(seconds / 3600) + " h " + QString::number((seconds % 3600) / 60) + " min";
}

// QR code drawn black on white with its quiet zone (4 modules), as big as fits in size
static QPixmap qr_pixmap(const QString& text, const int size)
{
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int modules = qr.getSize() + 8;
    const int scale = std::max(1, size / modules);
    QImage image(modules * scale, modules * scale, QImage::Format_RGB32);
    QPainter p(&image);

    image.fill(Qt::white);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (int y = 0; y < qr.getSize(); ++y)
        for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y)) p.drawRect((x + 4) * scale, (y + 4) * scale, scale, scale);
    p.end();
    return QPixmap::fromImage(image);
}

static QString endpoint_kind(const xdesk::net::Endpoint& e)
{
    if (e.label == "Tailscale") return "tailscale";
    if (e.label == "LAN") return "lan";
    if (e.label == "This PC") return "local";
    return "internet";
}

static QString endpoint_command(const xdesk::net::Endpoint& e)
{
    return "xdesk connect " + e.host + (e.port != XDESK_DEFAULT_PORT ? " -p " + QString::number(e.port) : "");
}

//----------------------------------------------------------------//
// Window

xdesk::ui::gui::HostWindow::HostWindow(host::Host& host, QWidget* parent)
: QMainWindow(parent), _host{host}
{
    QWidget* root = new QWidget(this);
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    QWidget* sidebar = new QWidget(root);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidebar);
    QWidget* main = new QWidget(root);
    QVBoxLayout* mainLayout = new QVBoxLayout(main);
    QHBoxLayout* brand = new QHBoxLayout();
    QLabel* logo = new QLabel();
    QFrame* rule = new QFrame(sidebar);
    QHBoxLayout* bannerLayout = nullptr;
    QLabel* bannerIcon = new QLabel();
    QPushButton* bannerClose = nullptr;

    this->setWindowTitle("xdesk - remote access");
    this->setWindowIcon(QIcon(Theme::logo(64)));
    this->resize(1180, 780);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar: brand, pages, theme, then the sharing switch at the bottom (always in reach)
    sidebar->setObjectName("sidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setFixedWidth(220);
    sideLayout->setContentsMargins(14, 18, 14, 16);
    sideLayout->setSpacing(6);
    logo->setPixmap(Theme::logo(30));
    brand->addWidget(logo);
    brand->addWidget(make_label("xdesk", "brand"));
    brand->addStretch();
    brand->addWidget(make_label(QString("v") + XDESK_VERSION, "version"));
    sideLayout->addLayout(brand);
    sideLayout->addSpacing(16);
    this->_nav = new QListWidget(sidebar);
    this->_nav->setObjectName("nav");
    this->_nav->setFrameShape(QFrame::NoFrame);
    this->_nav->setIconSize(QSize(18, 18));
    for (const std::pair<QString, QString>& entry: {std::pair<QString, QString>{"Overview", "overview"}, {"Sessions", "sessions"}, {"Devices", "devices"}, {"Logs", "logs"}, {"Settings", "settings"}})
        this->_nav->addItem(new QListWidgetItem(Theme::icon(entry.second), entry.first));
    sideLayout->addWidget(this->_nav, 1);
    sideLayout->addWidget(make_label("Theme", "subtitle"));
    sideLayout->addWidget(Theme::switcher([this](const QString& theme) {this->_host.setTheme(theme);}, sidebar));
    sideLayout->addSpacing(12);
    rule->setObjectName("rule");
    sideLayout->addWidget(rule);
    sideLayout->addSpacing(8);
    this->_statusSessions = make_label("", "muted");
    this->_statusSessions->setWordWrap(true);
    sideLayout->addWidget(this->_statusSessions);
    this->_share = make_button("", "", "power");
    this->_share->setObjectName("share");
    sideLayout->addWidget(this->_share);
    QObject::connect(this->_share, &QPushButton::clicked, this, &HostWindow::toggleSharing_);

    // Pages + tamper banner
    mainLayout->setContentsMargins(26, 22, 26, 22);
    mainLayout->setSpacing(14);
    this->_banner = new QWidget(main);
    this->_banner->setObjectName("banner");
    this->_banner->setAttribute(Qt::WA_StyledBackground, true);
    bannerLayout = new QHBoxLayout(this->_banner);
    bannerLayout->setContentsMargins(14, 10, 10, 10);
    this->_bannerText = new QLabel(this->_banner);
    this->_bannerText->setWordWrap(true);
    bannerClose = make_button("Dismiss", "ghost");
    bannerClose->setStyleSheet("color: white;");
    bannerIcon->setPixmap(Theme::icon("shield", Qt::white).pixmap(20, 20));
    bannerLayout->addWidget(bannerIcon);
    bannerLayout->addWidget(this->_bannerText, 1);
    bannerLayout->addWidget(bannerClose);
    this->_banner->hide();
    QObject::connect(bannerClose, &QPushButton::clicked, this->_banner, &QWidget::hide);
    mainLayout->addWidget(this->_banner);

    // Request of a viewer whose control is paused by the focus of this panel
    this->_focusBanner = new QWidget(main);
    this->_focusBanner->setObjectName("focusBanner");
    this->_focusBanner->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout* focusLayout = new QHBoxLayout(this->_focusBanner);
    focusLayout->setContentsMargins(14, 10, 10, 10);
    this->_focusText = new QLabel(this->_focusBanner);
    this->_focusText->setWordWrap(true);
    QPushButton* focusRefuse = make_button("Refuse", "danger");
    focusLayout->addWidget(this->_focusText, 1);
    focusLayout->addWidget(focusRefuse);
    this->_focusBanner->hide();
    QObject::connect(focusRefuse, &QPushButton::clicked, this, [this]() {this->_host.refuseFocusRequest();});
    mainLayout->addWidget(this->_focusBanner);
    this->_pages = new QStackedWidget(main);
    this->_pages->addWidget(this->buildOverview_());
    this->_pages->addWidget(this->buildSessions_());
    this->_pages->addWidget(this->buildDevices_());
    this->_pages->addWidget(this->buildLogs_());
    this->_pages->addWidget(this->buildSettings_());
    mainLayout->addWidget(this->_pages, 1);
    QObject::connect(this->_nav, &QListWidget::currentRowChanged, this, [this](const int row) {
        this->_pages->setCurrentIndex(row);
        QTimer::singleShot(0, this, &HostWindow::fitPage_); // once the page is laid out
    });
    this->_nav->setCurrentRow(0);
    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(main, 1);
    this->setCentralWidget(root);
    this->buildTray_();

    // Host events
    QObject::connect(&this->_host, &host::Host::stateChanged, this, [this]() {this->refreshOverview_(); this->refreshDevices_();});
    QObject::connect(&this->_host, &host::Host::sessionAdded, this, &HostWindow::refreshSession_);
    QObject::connect(&this->_host, &host::Host::sessionChanged, this, &HostWindow::refreshSession_);
    QObject::connect(&this->_host, &host::Host::sessionRemoved, this, [this](const QString& id) {
        const int row = this->sessionRow_(id);
        if (row >= 0) this->_sessions->removeRow(row);
        if (this->_selected == id) this->selectSession_("");
        this->refreshLock_();
    });
    QObject::connect(&this->_host, &host::Host::activity, this, &HostWindow::pushActivity_);
    QObject::connect(&this->_host, &host::Host::approvalRequested, this, &HostWindow::showApproval_);
    QObject::connect(&this->_host, &host::Host::tamperDetected, this, &HostWindow::showTamper_);
    QObject::connect(&this->_host, &host::Host::focusRequestChanged, this, &HostWindow::refreshFocusRequest_);
    qApp->installEventFilter(this); // the keys / clicks of the panel pause a control given back by a request
    QObject::connect(&this->_host, &host::Host::terminalOutput, this, [this](const QString& session, const std::uint8_t id, const QByteArray& data) {
        this->mirror_(session, id)->feed(data);
    });
    QObject::connect(&this->_host, &host::Host::terminalInput, this, [this](const QString& session, const std::uint8_t id, const QByteArray& data) {
        const QString key = session + "/" + QString::number(id);
        for (const char c: data) {
            if (c == '\r' || c == '\n') {
                if (!this->_typed[key].trimmed().isEmpty()) this->pushActivity_(session, "terminal " + QString::number(id) + " $ " + QString::fromUtf8(this->_typed[key]));
                this->_typed[key].clear();
            } else if (c == 0x7F || c == 0x08) this->_typed[key].chop(1);
            else if (static_cast<unsigned char>(c) >= 0x20) this->_typed[key].append(c);
        }
    });
    QObject::connect(&this->_host.log(), &core::AuditLog::entry, this, &HostWindow::addLog_);
    for (const QJsonObject& entry: this->_host.log().recent(500))
        this->addLog_(entry);
    QObject::connect(&this->_tick, &QTimer::timeout, this, [this]() {
        this->refreshOverview_();
        this->refreshFocusRequest_();
        for (host::Session* s: this->_host.sessions())
            this->refreshSession_(s);
        this->refreshLock_();
    });
    this->_tick.start(1000);
    for (host::Session* s: this->_host.sessions())
        this->refreshSession_(s);
    this->refreshOverview_();
    this->refreshDevices_();
    this->loadSettings_();
}

void xdesk::ui::gui::HostWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, &HostWindow::fitPage_);
    #ifdef _WIN32
        // The remote sessions never see the panel (Windows 10 2004+)
        SetWindowDisplayAffinity(reinterpret_cast<HWND>(this->winId()), 0x00000011); // WDA_EXCLUDEFROMCAPTURE
    #endif
}

bool xdesk::ui::gui::HostWindow::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type type = event->type();
    const QWidget* widget = qobject_cast<QWidget*>(watched);

    if (this->_settingsScroll && watched == this->_settingsScroll->viewport() && type == QEvent::Resize) this->flowSettings_();
    // Someone in front of the PC uses the panel (the button Refuse included): the remote control waits again
    if ((type == QEvent::KeyPress || type == QEvent::MouseButtonPress) && widget && widget->window() == this) this->_host.localPanelInput();
    return QMainWindow::eventFilter(watched, event);
}

void xdesk::ui::gui::HostWindow::fitPage_(void)
{
    QWidget* page = this->_pages->currentWidget();
    QScrollArea* scroll = qobject_cast<QScrollArea*>(page);
    const QRect screen = this->screen() ? this->screen()->availableGeometry() : QRect();
    const QSize frame = this->frameGeometry().size() - this->geometry().size(); // title bar + borders
    QSize want;
    QRect placed;

    if (!page || !this->isVisible() || screen.isEmpty() || this->isMaximized() || this->isFullScreen()) return;

    // Size the page asks for (a scrolled page: its content, the settings with their columns side by side)
    if (scroll == this->_settingsScroll && this->_settingsColumns) {
        this->_settingsColumns->setDirection(QBoxLayout::LeftToRight);
        scroll->widget()->layout()->invalidate(); // the page hint is cached: computed again with the new direction
    }
    want = scroll && scroll->widget() ? scroll->widget()->sizeHint() + QSize(scroll->verticalScrollBar()->sizeHint().width() + 2, 2) : page->sizeHint();
    want += this->size() - page->size(); // sidebar, margins, banner

    // Only grows, never past the screen (the page scrolls then)
    want = want.boundedTo(screen.size() - frame).expandedTo(this->size());
    if (want != this->size()) {
        this->resize(want);
        placed = QRect(this->frameGeometry().topLeft(), want + frame);
        placed.moveLeft(std::max(screen.left(), std::min(placed.left(), screen.right() + 1 - placed.width())));
        placed.moveTop(std::max(screen.top(), std::min(placed.top(), screen.bottom() + 1 - placed.height())));
        if (placed.topLeft() != this->frameGeometry().topLeft()) this->move(placed.topLeft());
    }
    this->flowSettings_();
}

void xdesk::ui::gui::HostWindow::flowSettings_(void)
{
    int wide = 0;

    if (!this->_settingsScroll || !this->_settingsColumns) return;

    // Width of the columns side by side at their preferred size: less room = one under the other, no horizontal scroll
    for (int i = 0; i < this->_settingsColumns->count(); ++i)
        wide += this->_settingsColumns->itemAt(i)->sizeHint().width();
    wide += this->_settingsColumns->spacing() * std::max(0, this->_settingsColumns->count() - 1) + 4; // + right margin of the page
    this->_settingsColumns->setDirection(this->_settingsScroll->viewport()->width() >= wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
}

void xdesk::ui::gui::HostWindow::changeEvent(QEvent* event)
{
    // Remote input is paused while the local user works in the panel
    if (event->type() == QEvent::ActivationChange) this->_host.setPanelFocused(this->isActiveWindow());
    QMainWindow::changeEvent(event);
}

void xdesk::ui::gui::HostWindow::refreshFocusRequest_(void)
{
    const host::Session* asking = this->_host.session(this->_host.focusRequest());
    const host::Session* given = this->_host.session(this->_host.focusOverride());
    const qint64 left = std::max<qint64>(0, this->_host.focusDeadline() - QDateTime::currentMSecsSinceEpoch());

    if (asking) {
        this->_focusText->setText(asking->name() + " asks its remote control back (paused while this panel has the focus): given in " + QString::number((left + 999) / 1000) + " s unless you refuse or use the panel.");
        this->_focusBanner->findChild<QPushButton*>()->show();
    } else if (given) {
        this->_focusText->setText(given->name() + " controls this PC although the panel has the focus (its request was granted). A key or a click in the panel pauses it again.");
        this->_focusBanner->findChild<QPushButton*>()->hide();
    }
    this->_focusBanner->setVisible(asking || given);
}

void xdesk::ui::gui::HostWindow::retheme_(void)
{
    static const QStringList nav = {"overview", "sessions", "devices", "logs", "settings"};

    // The icons are drawn in the colours of the theme: draw them again
    for (int i = 0; i < this->_nav->count() && i < nav.size(); ++i)
        this->_nav->item(i)->setIcon(Theme::icon(nav[i]));
    for (QPushButton* button: this->findChildren<QPushButton*>()) {
        const QString icon = button->property("icon").toString();
        if (!icon.isEmpty()) button->setIcon(Theme::icon(icon, button->property("kind").toString() == "primary" ? Theme::palette().accentText : QColor()));
    }
    this->_endpointKey.clear(); // rows drawn again with the new icons
    this->refreshShare_();
    this->refreshOverview_();
    this->refreshDevices_();
}

void xdesk::ui::gui::HostWindow::closeEvent(QCloseEvent* event)
{
    // Sharing + a tray: the panel hides, xdesk keeps sharing; otherwise closing the panel quits
    if (this->_quitting || !this->_host.isSharing() || !this->_tray || !QSystemTrayIcon::isSystemTrayAvailable()) {
        if (!this->_quitting && !this->_host.sessions().isEmpty() && QMessageBox::question(this, "Quit xdesk", "Sessions are connected. Stop sharing and quit xdesk?") != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        this->_host.setPanelFocused(false);
        event->accept();
        QApplication::quit();
        return;
    }
    event->ignore();
    this->hide();
    this->_host.setPanelFocused(false);
    if (!this->_trayHintShown) {
        this->_tray->showMessage("xdesk is still sharing", "The panel is in the tray. Stop sharing or quit from the tray menu.", QSystemTrayIcon::Information, 4000);
        this->_trayHintShown = true;
    }
}

void xdesk::ui::gui::HostWindow::quit_(void)
{
    if (!this->_host.sessions().isEmpty() && QMessageBox::question(this, "Quit xdesk", "Every session will be disconnected. Stop sharing and quit xdesk?") != QMessageBox::Yes) return;
    this->_quitting = true;
    this->_host.kickAll("The host stopped sharing");
    QApplication::quit();
}

void xdesk::ui::gui::HostWindow::toggleSharing_(void)
{
    const bool on = this->_host.isSharing();

    if (on && !this->_host.sessions().isEmpty() && QMessageBox::question(this, "Stop sharing", "Every session will be disconnected. Stop sharing? (xdesk stays open)") != QMessageBox::Yes) return;
    try {
        this->_host.setSharing(!on);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Start sharing", QString("Sharing could not start:\n") + e.what());
    }
    this->refreshOverview_();
}

void xdesk::ui::gui::HostWindow::refreshShare_(void)
{
    const bool on = this->_host.isSharing();

    // Off: green "Start sharing" | on: red "Stop sharing"
    this->_share->setText(on ? "Stop sharing" : "Start sharing");
    this->_share->setToolTip(on ? "Close every connection and stop listening (xdesk stays open)" : "Listen for connections: the code and the addresses of the overview work again");
    this->_share->setIcon(Theme::icon("power", on ? QColor(Qt::white) : Theme::palette().accentText));
    set_prop(this->_share, "on", on);
    this->_shareState->setText(on ? "● SHARING" : "● OFF");
    this->_shareState->setToolTip(on ? "Connections are accepted on the addresses below" : "Nobody can connect: start sharing at the bottom left");
    set_prop(this->_shareState, "on", on);
    set_prop(this->_code, "off", !on);
    this->_codeActions->setVisible(on);
    if (this->_trayShare) this->_trayShare->setText(on ? "Stop sharing" : "Start sharing");
}

void xdesk::ui::gui::HostWindow::buildTray_(void)
{
    QMenu* menu = new QMenu(this);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    this->_tray = new QSystemTrayIcon(QIcon(Theme::logo(64)), this);
    this->_tray->setToolTip("xdesk");
    menu->addAction("Open the panel", this, [this]() {this->showNormal(); this->raise(); this->activateWindow();});
    this->_trayShare = menu->addAction("Start sharing", this, &HostWindow::toggleSharing_);
    menu->addAction("Copy the code", this, [this]() {QGuiApplication::clipboard()->setText(this->_host.code().display());});
    menu->addAction("New code", this, [this]() {this->_host.rotateCode();});
    menu->addSeparator();
    menu->addAction("Disconnect everybody", this, [this]() {this->_host.kickAll("Disconnected by the host");});
    menu->addAction("Quit xdesk", this, &HostWindow::quit_);
    this->_tray->setContextMenu(menu);
    QObject::connect(this->_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {this->showNormal(); this->raise(); this->activateWindow();}
    });
    this->_tray->show();
}

//----------------------------------------------------------------//
// Overview

QWidget* xdesk::ui::gui::HostWindow::buildOverview_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QHBoxLayout* top = new QHBoxLayout();
    QWidget* codeBody = nullptr;
    QWidget* statusBody = nullptr;
    QWidget* endpointsBody = nullptr;
    QPushButton* copy = make_button("Copy", "", "copy");
    QPushButton* rotate = make_button("New code", "ghost", "refresh");
    QPushButton* qr = make_button("QR code", "", "qr");
    QHBoxLayout* codeButtons = nullptr;
    QVBoxLayout* codeLayout = nullptr;
    QFormLayout* statusLayout = nullptr;
    QVBoxLayout* endpointsLayout = nullptr;
    QFrame* codeCard = make_panel("Connection code", codeBody);
    QFrame* statusCard = make_panel("Status", statusBody);
    QFrame* endpointsCard = nullptr;

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    this->_shareState = make_label("", "pill");
    layout->addWidget(make_page_head("Overview", "", this->_shareState, &this->_identity));

    // Code
    codeCard->setProperty("accent", true);
    codeLayout = new QVBoxLayout(codeBody);
    codeLayout->setContentsMargins(20, 16, 20, 18);
    codeLayout->setSpacing(8);
    codeLayout->addWidget(make_label("Give it to the person who connects: it works once, then changes", "muted"));
    this->_code = make_label("---", "code");
    codeLayout->addWidget(this->_code);
    this->_codeBar = new QProgressBar(codeBody);
    this->_codeBar->setTextVisible(false);
    this->_codeBar->setFixedHeight(3);
    codeLayout->addWidget(this->_codeBar);
    this->_codeTimer = make_label("", "muted");
    this->_codeTimer->setWordWrap(true);
    codeLayout->addWidget(this->_codeTimer);
    this->_fingerprint = make_label("", "muted");
    this->_fingerprint->setToolTip(security::fingerprint_text(this->_host.certificate().fingerprint()));
    codeLayout->addWidget(this->_fingerprint);
    codeLayout->addStretch();
    this->_codeActions = new QWidget(codeBody);
    codeButtons = new QHBoxLayout(this->_codeActions);
    codeButtons->setContentsMargins(0, 6, 0, 0);
    codeButtons->addWidget(copy);
    codeButtons->addWidget(qr);
    codeButtons->addWidget(rotate);
    codeButtons->addStretch();
    codeLayout->addWidget(this->_codeActions);
    copy->setToolTip("Copy the code");
    qr->setToolTip("Show a QR code to scan with a phone: it opens the xdesk app or the browser with the code filled in");
    QObject::connect(copy, &QPushButton::clicked, this, [this]() {QGuiApplication::clipboard()->setText(this->_host.code().display());});
    QObject::connect(qr, &QPushButton::clicked, this, [this]() {this->showQr_(-1);});
    QObject::connect(rotate, &QPushButton::clicked, this, [this]() {this->_host.rotateCode();});

    // Status
    statusLayout = new QFormLayout(statusBody);
    statusLayout->setContentsMargins(20, 16, 20, 18);
    statusLayout->setVerticalSpacing(12);
    statusLayout->setHorizontalSpacing(18);
    statusLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    this->_statusCapture = make_label("-");
    this->_statusCapture->setWordWrap(true);
    this->_statusInput = make_label("-");
    this->_statusTls = make_label(security::tls_summary());
    this->_statusTls->setWordWrap(true);
    statusLayout->addRow(make_label("Screen", "muted"), this->_statusCapture);
    statusLayout->addRow(make_label("Input", "muted"), this->_statusInput);
    statusLayout->addRow(make_label("Encryption", "muted"), this->_statusTls);
    top->setSpacing(16);
    top->addWidget(codeCard, 3);
    top->addWidget(statusCard, 2);
    layout->addLayout(top);

    // Addresses: one row per path, the exposure chooser in the header
    this->_exposure = make_exposure_box();
    endpointsCard = make_panel("Who can reach this PC", endpointsBody, {make_label("Open to", "muted"), this->_exposure});
    endpointsLayout = new QVBoxLayout(endpointsBody);
    endpointsLayout->setContentsMargins(0, 0, 0, 0);
    endpointsLayout->setSpacing(0);
    this->_exposureStatus = make_label("", "muted");
    this->_exposureStatus->setWordWrap(true);
    this->_exposureStatus->setContentsMargins(16, 12, 16, 10);
    endpointsLayout->addWidget(this->_exposureStatus);
    this->_endpointList = new QWidget(endpointsBody);
    new QVBoxLayout(this->_endpointList);
    this->_endpointList->layout()->setContentsMargins(0, 0, 0, 0);
    this->_endpointList->layout()->setSpacing(0);
    endpointsLayout->addWidget(this->_endpointList);
    endpointsLayout->addStretch();
    QObject::connect(this->_exposure, &QComboBox::activated, this, [this](const int) {
        core::Settings s = this->_host.config().settings();
        s.exposure = core::exposure_from(this->_exposure->currentData().toString());
        if (!this->_host.applySettings(s)) QMessageBox::warning(this, "Locked", "Settings cannot change while a remote session controls this PC or has a terminal.");
        this->loadSettings_();
    });
    layout->addWidget(endpointsCard);
    layout->addStretch(1);
    return page;
}

void xdesk::ui::gui::HostWindow::refreshOverview_(void)
{
    const security::PairingCode& code = this->_host.code();
    const bool on = this->_host.isSharing();
    const int left = code.secondsLeft();
    const int lifetime = std::max(1, this->_host.config().settings().codeLifetime);
    const net::Tailnet& tailnet = this->_host.exposure().tailnet();
    QString identity = "This PC is <b>" + QHostInfo::localHostName().toHtmlEscaped() + "</b>";
    int active = 0;

    this->refreshShare_();
    this->_code->setText(on ? code.display() : "OFF");
    this->_codeBar->setVisible(on && left >= 0);
    this->_codeBar->setRange(0, lifetime);
    this->_codeBar->setValue(std::max(0, left));
    if (!on) this->_codeTimer->setText("Nobody can connect while sharing is off. Start sharing (bottom left) to get a code.");
    else this->_codeTimer->setText(left >= 0 ? "Changes in " + QString::number(left / 60) + ":" + QString::number(left % 60).rightJustified(2, '0') + ", and after each use" : "Changes after each use");
    this->_fingerprint->setText("Host identity " + security::fingerprint_short(this->_host.certificate().fingerprint()) + "  (the native clients show the same one)");
    if (!on) this->_statusCapture->setText("off");
    else this->_statusCapture->setText(this->_host.captureBackend().isEmpty() ? QString("idle, starts with the first viewer") : this->_host.captureBackend());
    set_tone(this->_statusCapture, !on || this->_host.captureBackend().isEmpty() ? "muted" : (this->_host.captureBackend().startsWith("none") ? "danger" : "success"));
    this->_statusInput->setText(!on ? QString("off") : (!this->_host.inputError().isEmpty() ? "unavailable" : (this->_host.captureBackend().isEmpty() ? QString("idle") : this->_host.inputBackend())));
    this->_statusInput->setToolTip(this->_host.inputError());
    set_tone(this->_statusInput, !on ? "muted" : (!this->_host.inputError().isEmpty() ? "warning" : (this->_host.inputBackend() == "none" ? "muted" : "success")));
    if (on && this->_host.exposure().mode() == core::Exposure::Tailscale && tailnet.up && !tailnet.dns.isEmpty())
        identity += "  ·  on Tailscale it is <b>" + tailnet.dns.section('.', 0, 0).toHtmlEscaped() + "</b> (" + tailnet.dns.toHtmlEscaped() + ")";
    this->_identity->setText(identity);
    this->_exposure->setCurrentIndex(std::max(0, this->_exposure->findData(core::exposure_name(this->_host.config().settings().exposure))));
    this->_exposureStatus->setText(on ? this->_host.exposure().status() + "." : "Sharing is off: every path is closed. The choice above applies when sharing starts.");
    this->refreshEndpoints_();
    for (const host::Session* s: this->_host.sessions())
        active += s->state() == host::State::Active;
    if (!on) this->_statusSessions->setText("Not sharing: nobody can connect");
    else this->_statusSessions->setText(active == 0 ? "Sharing, no session yet" : QString::number(active) + (active == 1 ? " session connected" : " sessions connected"));
    if (this->_tray) this->_tray->setToolTip(on ? "xdesk - " + this->_statusSessions->text() + " - code " + code.display() : QString("xdesk - not sharing"));
}

void xdesk::ui::gui::HostWindow::refreshEndpoints_(void)
{
    const QList<net::Endpoint>& endpoints = this->_host.exposure().endpoints();
    const bool on = this->_host.isSharing();
    QLayout* layout = this->_endpointList->layout();
    QString key = on ? "on" : "off";

    // Rebuilt only when an address changes (the overview refreshes every second)
    for (const net::Endpoint& e: endpoints)
        key += "\n" + e.label + "|" + e.url + "|" + e.reach + "|" + e.host + "|" + e.ip + "|" + (e.endToEnd ? "1" : "0");
    if (key == this->_endpointKey) return;
    this->_endpointKey = key;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    if (!on || endpoints.isEmpty()) {
        QLabel* empty = make_label(on ? "Looking for the addresses of this PC..." : "No address while sharing is off.", "muted");
        empty->setContentsMargins(16, 6, 16, 12);
        layout->addWidget(empty);
        if (!on) {
            QWidget* actions = new QWidget(this->_endpointList);
            QHBoxLayout* actionsLayout = new QHBoxLayout(actions);
            QPushButton* start = make_button("Start sharing", "primary", "power");
            actionsLayout->setContentsMargins(16, 0, 16, 16);
            actionsLayout->addWidget(start);
            actionsLayout->addStretch();
            QObject::connect(start, &QPushButton::clicked, this, &HostWindow::toggleSharing_);
            layout->addWidget(actions);
        }
        return;
    }
    for (int i = 0; i < endpoints.size(); ++i) {
        const net::Endpoint& e = endpoints[i];
        const QString urlHost = QUrl(e.url).host();
        QFrame* row = new QFrame(this->_endpointList);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        QVBoxLayout* texts = new QVBoxLayout();
        QLabel* tag = make_label(e.label.toUpper(), "tag");
        QLabel* address = make_label(e.url, "address");
        QLabel* reach = make_label(e.reach);
        QPushButton* copy = make_button("", "ghost", "copy");
        QPushButton* qr = make_button("", "ghost", "qr");
        QStringList details = {e.reach};

        // <tag> <address / who can use it> <copy> <qr>
        if (e.host != urlHost) details << "xdesk connect " + e.host;
        if (!e.ip.isEmpty()) details << "IP " + e.ip;
        if (urlHost == this->_host.exposure().certificateName()) details << "certificate by Tailscale (no browser warning)";
        if (!e.endToEnd) details << "the relay decrypts the traffic";
        reach->setText(details.join("  ·  "));
        reach->setWordWrap(true);
        set_tone(reach, e.endToEnd ? "muted" : "warning");
        row->setObjectName("endpoint");
        row->setProperty("last", i == endpoints.size() - 1);
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setToolTip(endpoint_command(e) + "\n" + (e.endToEnd ? "Encrypted end to end (TLS 1.3 + ML-KEM)" : "The relay decrypts the traffic: prefer another path for sensitive work"));
        rowLayout->setContentsMargins(16, 10, 12, 10);
        rowLayout->setSpacing(14);
        tag->setProperty("kind", endpoint_kind(e));
        tag->setAlignment(Qt::AlignCenter);
        tag->setFixedWidth(92);
        texts->setSpacing(3);
        texts->addWidget(address);
        texts->addWidget(reach);
        copy->setToolTip("Copy " + e.url);
        qr->setToolTip("QR code of this address (with the code)");
        rowLayout->addWidget(tag, 0, Qt::AlignVCenter);
        rowLayout->addLayout(texts, 1);
        rowLayout->addWidget(copy);
        rowLayout->addWidget(qr);
        QObject::connect(copy, &QPushButton::clicked, this, [copy, url = e.url]() {
            QGuiApplication::clipboard()->setText(url);
            copy->setText("Copied");
            QTimer::singleShot(1500, copy, [copy]() {copy->setText("");});
        });
        QObject::connect(qr, &QPushButton::clicked, this, [this, i]() {this->showQr_(i);});
        layout->addWidget(row);
    }
}

void xdesk::ui::gui::HostWindow::showQr_(const int endpoint)
{
    QDialog* dialog = new QDialog(this);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QComboBox* address = new QComboBox(dialog);
    QWidget* target = new QWidget(dialog);
    QHBoxLayout* targetLayout = new QHBoxLayout(target);
    QButtonGroup* group = new QButtonGroup(dialog);
    QToolButton* app = new QToolButton(target);
    QToolButton* browser = new QToolButton(target);
    QLabel* image = new QLabel(dialog);
    QLabel* link = make_label("", "mono");
    QLabel* hint = make_label("", "muted");
    QLabel* lead = make_label("The camera of the phone opens the link: the code is filled in, and the app also pins the identity of this PC.", "muted");
    QPushButton* copy = make_button("Copy the link", "ghost", "copy");
    QPushButton* close = make_button("Close", "ghost");
    QHBoxLayout* buttons = new QHBoxLayout();
    const QList<net::Endpoint> endpoints = this->_host.exposure().endpoints();
    int first = -1;
    std::function<void(void)> draw;

    if (!this->_host.isSharing() || endpoints.isEmpty()) {
        QMessageBox::information(this, "QR code", "Start sharing first: the QR code holds an address of this PC and the connection code.");
        dialog->deleteLater();
        return;
    }
    dialog->setWindowTitle("QR code - xdesk");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(440);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);
    layout->addWidget(make_label("Scan with a phone", "title"));
    lead->setWordWrap(true);
    layout->addWidget(lead);
    // One entry per address (data = endpoint * 2 + 1 for its numeric address when the link has a name)
    for (int i = 0; i < endpoints.size(); ++i) {
        const QString host = QUrl(endpoints[i].url).host();
        if (i == endpoint || (endpoint < 0 && first < 0 && endpoint_kind(endpoints[i]) != "local")) first = address->count();
        address->addItem(endpoints[i].label + "  ·  " + host, i * 2);
        if (!endpoints[i].ip.isEmpty() && endpoints[i].ip != host) address->addItem(endpoints[i].label + "  ·  " + endpoints[i].ip, i * 2 + 1);
    }
    address->setCurrentIndex(std::max(0, first));
    layout->addWidget(address);

    // App link (xdesk://, pinned fingerprint) | browser link (https://, web client)
    target->setObjectName("segmented");
    target->setAttribute(Qt::WA_StyledBackground, true);
    targetLayout->setContentsMargins(1, 1, 1, 1);
    targetLayout->setSpacing(0);
    app->setText("xdesk app (Android)");
    browser->setText("Browser");
    for (QToolButton* button: {app, browser}) {
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        group->addButton(button);
        targetLayout->addWidget(button);
    }
    app->setChecked(true);
    layout->addWidget(target);
    image->setObjectName("qr");
    image->setAlignment(Qt::AlignCenter);
    layout->addWidget(image, 0, Qt::AlignHCenter);
    link->setWordWrap(true);
    link->setAlignment(Qt::AlignCenter);
    layout->addWidget(link);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    draw = [this, address, app, image, link, hint, endpoints]() {
        const net::Endpoint& e = endpoints.value(address->currentData().toInt() / 2);
        const QUrl url(e.url);
        const QString code = security::normalize_code(this->_host.code().display());
        // The app always gets the address: no SNI = the pinned certificate (a tailnet name gets the tailscale one)
        const QString host = (app->isChecked() || address->currentData().toInt() % 2 == 1) && !e.ip.isEmpty() ? e.ip : url.host();
        const QString hostPort = (host.contains(':') ? "[" + host + "]" : host) + ":" + QString::number(url.port(e.port));
        const bool trusted = host.endsWith(".ts.net", Qt::CaseInsensitive) && host == this->_host.exposure().certificateName();
        const QString text = app->isChecked()
            ? "xdesk://" + hostPort + "?fp=" + QString::fromLatin1(this->_host.certificate().fingerprint().toHex()) + "&code=" + code
            : "https://" + hostPort + "/?code=" + code;

        image->setPixmap(qr_pixmap(text, 300));
        link->setText(text.section('?', 0, 0) + "  ·  code " + this->_host.code().display()); // the fingerprint stays in the tooltip (too long to read)
        link->setToolTip(text);
        hint->setText(app->isChecked()
            ? "Android: the camera (or Google Lens) opens the xdesk app, which trusts this PC without asking. No app yet: use Browser, the web page offers it."
            : trusted ? "Any device of the tailnet: the camera opens the web client with the code (certificate issued by Tailscale, no warning)."
            : "Any phone: the camera opens the web client with the code. The browser warns once about the self-signed certificate: compare the host identity.");
    };
    QObject::connect(address, &QComboBox::currentIndexChanged, dialog, [draw](const int) {draw();});
    QObject::connect(group, &QButtonGroup::buttonClicked, dialog, [draw](QAbstractButton*) {draw();});
    QObject::connect(&this->_host.code(), &security::PairingCode::changed, dialog, [draw](const QString&) {draw();}); // the code changes after each use
    QObject::connect(&this->_host, &host::Host::stateChanged, dialog, [this, dialog]() {if (!this->_host.isSharing()) dialog->close();});
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    QObject::connect(copy, &QPushButton::clicked, dialog, [link]() {QGuiApplication::clipboard()->setText(link->toolTip());});
    draw();
    dialog->show();
}

//----------------------------------------------------------------//
// Sessions

QWidget* xdesk::ui::gui::HostWindow::buildSessions_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QSplitter* splitter = new QSplitter(Qt::Vertical, page);
    QWidget* listBody = nullptr;
    QWidget* detailBody = nullptr;
    QHBoxLayout* perms = new QHBoxLayout();
    QPushButton* kick = make_button("Disconnect", "", "power");
    QPushButton* ban = make_button("Ban", "danger", "lock");
    QFrame* listCard = make_panel("Connected devices", listBody);
    QFrame* detailCard = make_panel("Select a session", detailBody, {kick, ban}, &this->_detailTitle);
    QVBoxLayout* listLayout = new QVBoxLayout(listBody);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailBody);
    QTabWidget* tabs = new QTabWidget();

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(make_page_head("Sessions", "Everything each connected device does, live. Select a session to see its inputs and terminals."));
    listLayout->setContentsMargins(0, 0, 0, 0);
    this->_sessions = make_table({"Device", "Client", "Address", "State", "Mode", "Rights", "FPS", "Latency", "Kbit/s", "Since", "Last activity"});
    listLayout->addWidget(this->_sessions);
    splitter->setObjectName("gap");
    splitter->setHandleWidth(14);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(listCard);
    QObject::connect(this->_sessions, &QTableWidget::itemSelectionChanged, this, [this]() {
        const QList<QTableWidgetItem*> items = this->_sessions->selectedItems();
        this->selectSession_(items.isEmpty() ? QString() : items.front()->data(Qt::UserRole).toString());
    });

    // Detail
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(0);
    this->_detailInfo = make_label("", "muted");
    this->_detailInfo->setWordWrap(true);
    this->_detailInfo->setContentsMargins(16, 12, 16, 4);
    detailLayout->addWidget(this->_detailInfo);
    this->_permView = new QCheckBox("See the screen");
    this->_permControl = new QCheckBox("Control keyboard && mouse");
    this->_permTerminal = new QCheckBox("Open terminals");
    this->_permClipboard = new QCheckBox("Clipboard");
    perms->setContentsMargins(16, 8, 16, 12);
    perms->setSpacing(22);
    for (QCheckBox* box: {this->_permView, this->_permControl, this->_permTerminal, this->_permClipboard}) {
        perms->addWidget(box);
        QObject::connect(box, &QCheckBox::clicked, this, [this]() {
            host::Permissions p;
            p.view = this->_permView->isChecked();
            p.control = this->_permControl->isChecked();
            p.terminal = this->_permTerminal->isChecked();
            p.clipboard = this->_permClipboard->isChecked();
            this->_host.setPermissions(this->_selected, p);
        });
    }
    perms->addStretch();
    detailLayout->addLayout(perms);
    this->_activity = new QPlainTextEdit();
    this->_activity->setReadOnly(true);
    this->_activity->setMaximumBlockCount(2000);
    this->_activity->setFont(QFont(Theme::monospaceFamily(), 10));
    tabs->setDocumentMode(true);
    tabs->addTab(this->_activity, Theme::icon("control"), "Live activity");
    this->_mirrorTabs = new QTabWidget();
    this->_mirrorTabs->setDocumentMode(true);
    tabs->addTab(this->_mirrorTabs, Theme::icon("terminal"), "Terminals");
    detailLayout->addWidget(tabs, 1);
    this->_detail = detailCard;
    this->_detail->setEnabled(false);
    splitter->addWidget(detailCard);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);
    QObject::connect(kick, &QPushButton::clicked, this, [this]() {if (!this->_selected.isEmpty()) this->_host.kick(this->_selected);});
    QObject::connect(ban, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString reason = QInputDialog::getText(this, "Ban this device", "Reason (kept in the logs):", QLineEdit::Normal, "Banned by the host", &ok);
        if (ok && !this->_selected.isEmpty()) this->_host.ban(this->_selected, reason);
    });
    return page;
}

int xdesk::ui::gui::HostWindow::sessionRow_(const QString& id) const
{
    for (int row = 0; row < this->_sessions->rowCount(); ++row)
        if (this->_sessions->item(row, 0) && this->_sessions->item(row, 0)->data(Qt::UserRole).toString() == id) return row;
    return -1;
}

void xdesk::ui::gui::HostWindow::refreshSession_(host::Session* session)
{
    static const char* STATES[] = {"authenticating", "waiting approval", "active", "closed"};
    const host::Permissions& p = session->permissions();
    const host::Stats& st = session->stats();
    QStringList rights;
    int row = this->sessionRow_(session->id());
    QStringList cells;

    if (p.view) rights << "view";
    if (p.control) rights << "control";
    if (p.terminal) rights << "terminal";
    if (p.clipboard) rights << "clipboard";
    cells << session->name() << net::client_kind_name(session->kind()) << session->address() << STATES[static_cast<int>(session->state())]
        << session->mode() + " · " + this->_host.transport(session->id()) << rights.join(", ") << QString::number(st.fps) << (st.latency >= 0 ? QString::number(st.latency) + " ms" : "-")
        << QString::number(st.kbps) << since_text(session->since()) << session->lastActivity();
    if (row < 0) {
        row = this->_sessions->rowCount();
        this->_sessions->insertRow(row);
    }
    for (int col = 0; col < cells.size(); ++col) {
        QTableWidgetItem* item = this->_sessions->item(row, col);
        if (!item) {item = new QTableWidgetItem(); this->_sessions->setItem(row, col, item);}
        item->setText(cells[col]);
        item->setData(Qt::UserRole, session->id());
    }
    if (session->id() == this->_selected) this->refreshDetail_();
}

void xdesk::ui::gui::HostWindow::selectSession_(const QString& id)
{
    this->_selected = id;
    this->_activity->clear();
    while (this->_mirrorTabs->count() > 0)
        this->_mirrorTabs->removeTab(0);
    if (!id.isEmpty()) {
        this->_activity->setPlainText(this->_activityLog.value(id).join('\n'));
        for (QHash<QString, TerminalWidget*>::const_iterator it = this->_mirrors.cbegin(); it != this->_mirrors.cend(); ++it)
            if (it.key().startsWith(id + "/")) this->_mirrorTabs->addTab(it.value(), "Terminal " + it.key().section('/', 1));
    }
    this->refreshDetail_();
}

void xdesk::ui::gui::HostWindow::refreshDetail_(void)
{
    host::Session* s = this->_host.session(this->_selected);
    const bool locked = s && s->state() != host::State::Active;

    this->_detail->setEnabled(s != nullptr);
    if (!s) {this->_detailTitle->setText("Select a session"); this->_detailInfo->clear(); return;}
    this->_detailTitle->setText(s->name() + "  ·  " + net::client_kind_name(s->kind()) + "  ·  " + s->address());
    this->_detailInfo->setText("OS " + s->os() + "  ·  device " + (s->deviceId().isEmpty() ? "-" : s->deviceId()) + "  ·  " + QString::number(s->stats().inputs) + " inputs  ·  " + QString::number(s->stats().bytesOut / 1024) + " KiB sent" + (s->userAgent().isEmpty() ? "" : "\n" + s->userAgent()));
    for (QCheckBox* box: {this->_permView, this->_permControl, this->_permTerminal, this->_permClipboard})
        box->setEnabled(!locked);
    this->_permView->setChecked(s->permissions().view);
    this->_permControl->setChecked(s->permissions().control);
    this->_permTerminal->setChecked(s->permissions().terminal);
    this->_permClipboard->setChecked(s->permissions().clipboard);
}

void xdesk::ui::gui::HostWindow::pushActivity_(const QString& session, const QString& line)
{
    QStringList& lines = this->_activityLog[session];
    const QString stamped = QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "  " + line;

    // Mouse moves replace the previous move line (the feed stays readable)
    if (line.startsWith("mouse ") && !lines.isEmpty() && lines.back().mid(14).startsWith("mouse ")) lines.back() = stamped;
    else lines.append(stamped);
    if (lines.size() > 2000) lines.removeFirst();
    if (session != this->_selected) return;
    if (line.startsWith("mouse ")) this->_activity->setPlainText(lines.join('\n'));
    else this->_activity->appendPlainText(stamped);
    this->_activity->moveCursor(QTextCursor::End);
}

xdesk::ui::gui::TerminalWidget* xdesk::ui::gui::HostWindow::mirror_(const QString& session, const std::uint8_t id)
{
    const QString key = session + "/" + QString::number(id);
    TerminalWidget* widget = this->_mirrors.value(key);

    if (widget) return widget;
    widget = new TerminalWidget();
    widget->setReadOnly(true);
    this->_mirrors.insert(key, widget);
    if (session == this->_selected) this->_mirrorTabs->addTab(widget, "Terminal " + QString::number(id));
    return widget;
}

void xdesk::ui::gui::HostWindow::showApproval_(host::Session* session)
{
    QDialog* dialog = new QDialog(this);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* refuse = make_button("Refuse", "danger");
    QPushButton* accept = make_button("Accept", "");
    QPushButton* trust = make_button("Accept && trust", "primary", "shield");
    QLabel* countdown = make_label("", "muted");
    QTimer* timer = new QTimer(dialog);
    const QString id = session->id();
    int* left = new int(30);

    dialog->setWindowTitle("Incoming connection");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setMinimumWidth(460);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);
    layout->addWidget(make_label("Someone wants to connect", "title"));
    layout->addWidget(make_label("<b>" + session->name().toHtmlEscaped() + "</b><br>" + net::client_kind_name(session->kind()) + " · " + session->os().toHtmlEscaped() + " · " + session->address()));
    layout->addWidget(make_label("They typed the right code. Accept only if you expect this connection.", "muted"));
    layout->addWidget(countdown);
    buttons->addWidget(refuse);
    buttons->addStretch();
    buttons->addWidget(accept);
    buttons->addWidget(trust);
    layout->addLayout(buttons);
    QObject::connect(refuse, &QPushButton::clicked, dialog, [this, dialog, id]() {this->_host.approve(id, false); dialog->close();});
    QObject::connect(accept, &QPushButton::clicked, dialog, [this, dialog, id]() {this->_host.approve(id, true, false); dialog->close();});
    QObject::connect(trust, &QPushButton::clicked, dialog, [this, dialog, id]() {this->_host.approve(id, true, true); dialog->close();});
    QObject::connect(session, &host::Session::closed, dialog, &QDialog::close);
    QObject::connect(dialog, &QObject::destroyed, [left]() {delete left;});
    QObject::connect(timer, &QTimer::timeout, dialog, [this, dialog, countdown, left, id]() {
        countdown->setText("Refused automatically in " + QString::number(*left) + " s");
        if ((*left)-- > 0) return;
        this->_host.approve(id, false);
        dialog->close();
    });
    timer->start(1000);
    countdown->setText("Refused automatically in 30 s");
    if (this->_tray) this->_tray->showMessage("Incoming connection", session->name() + " (" + session->address() + ") wants to connect", QSystemTrayIcon::Warning, 8000);
    this->showNormal();
    this->raise();
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void xdesk::ui::gui::HostWindow::showTamper_(const QString& session, const QString& reason)
{
    host::Session* s = this->_host.session(session);
    const QString who = s ? s->name() + " (" + s->address() + ")" : "A session";

    this->_bannerText->setText(who + " tried to change or stop xdesk and was " + (this->_host.config().settings().banOnTamper ? "banned" : "disconnected") + ".\n" + reason);
    this->_banner->show();
    if (this->_tray) this->_tray->showMessage("Tampering attempt blocked", who + ": " + reason, QSystemTrayIcon::Critical, 10000);
}

//----------------------------------------------------------------//
// Devices

QWidget* xdesk::ui::gui::HostWindow::buildDevices_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QPushButton* trust = make_button("Trust", "", "shield");
    QPushButton* untrust = make_button("Untrust", "ghost");
    QPushButton* unban = make_button("Unban", "primary", "lock");
    QPushButton* forget = make_button("Forget", "danger");
    QPushButton* unbanAddress = make_button("Unban address", "primary", "lock");
    QWidget* devicesBody = nullptr;
    QWidget* addressesBody = nullptr;
    QFrame* devicesCard = make_panel("Known devices", devicesBody, {trust, untrust, forget, unban});
    QFrame* addressesCard = make_panel("Banned addresses", addressesBody, {unbanAddress});
    QVBoxLayout* devicesLayout = new QVBoxLayout(devicesBody);
    QVBoxLayout* addressesLayout = new QVBoxLayout(addressesBody);
    std::function<QString(QTableWidget*)> selected = [](QTableWidget* table) {
        const QList<QTableWidgetItem*> items = table->selectedItems();
        return items.isEmpty() ? QString() : items.front()->data(Qt::UserRole).toString();
    };

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(make_page_head("Devices", "Trusted devices reconnect without a code. Banned devices and addresses are refused until you unban them here, on this PC."));
    this->_lockInfo = make_label("", "");
    this->_lockInfo->setWordWrap(true);
    layout->addWidget(this->_lockInfo);
    devicesLayout->setContentsMargins(0, 0, 0, 0);
    this->_devices = make_table({"Device", "Client", "OS", "Last address", "State", "Sessions", "Last seen", "Reason"});
    devicesLayout->addWidget(this->_devices);
    layout->addWidget(devicesCard, 2);
    addressesLayout->setContentsMargins(0, 0, 0, 0);
    this->_addresses = make_table({"Address", "Reason", "Since", "Until"});
    addressesLayout->addWidget(this->_addresses);
    layout->addWidget(addressesCard, 1);
    this->_localButtons = {trust, untrust, unban, forget, unbanAddress};
    QObject::connect(trust, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty() && this->confirmLocal_("Trust this device? It will connect without a code (one code first when its key was erased by a ban: no approval asked).", false)) (void)this->_host.setTrusted(id, true);
    });
    QObject::connect(untrust, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty()) (void)this->_host.setTrusted(id, false);
    });
    QObject::connect(unban, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty() && this->confirmLocal_("Unban this device?", true)) (void)this->_host.unban(id);
    });
    QObject::connect(forget, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        const bool banned = this->_host.registry().isBanned(id);
        if (!id.isEmpty() && this->confirmLocal_(banned ? "Forget this banned device? Its ban is lifted too." : "Forget this device? It will need a code again.", banned)) (void)this->_host.forget(id);
    });
    QObject::connect(unbanAddress, &QPushButton::clicked, this, [this, selected]() {
        const QString address = selected(this->_addresses);
        if (!address.isEmpty() && this->confirmLocal_("Unban the address " + address + "?", true)) (void)this->_host.unbanAddress(address);
    });
    return page;
}

void xdesk::ui::gui::HostWindow::refreshDevices_(void)
{
    const QList<security::Device>& devices = this->_host.registry().devices();
    const QList<security::AddressBan>& addresses = this->_host.registry().addresses();

    this->_devices->setRowCount(static_cast<int>(devices.size()));
    for (int i = 0; i < devices.size(); ++i) {
        const security::Device& d = devices[i];
        const QStringList cells = {d.name, d.kind, d.os, d.address, d.banned ? "banned" : (d.trusted ? (d.secret.isEmpty() ? "trusted · code once" : "trusted") : "known"), QString::number(d.sessions), QDateTime::fromMSecsSinceEpoch(d.seen).toString("yyyy-MM-dd HH:mm"), d.banReason};
        for (int col = 0; col < cells.size(); ++col) {
            QTableWidgetItem* item = new QTableWidgetItem(cells[col]);
            item->setData(Qt::UserRole, d.id);
            if (col == 4) item->setForeground(d.banned ? Theme::palette().danger : (d.trusted ? Theme::palette().success : Theme::palette().muted));
            this->_devices->setItem(i, col, item);
        }
    }
    this->_addresses->setRowCount(static_cast<int>(addresses.size()));
    for (int i = 0; i < addresses.size(); ++i) {
        const security::AddressBan& a = addresses[i];
        const QStringList cells = {a.address, a.reason, QDateTime::fromMSecsSinceEpoch(a.since).toString("yyyy-MM-dd HH:mm"), a.until == 0 ? "local unban" : QDateTime::fromMSecsSinceEpoch(a.until).toString("yyyy-MM-dd HH:mm")};
        for (int col = 0; col < cells.size(); ++col) {
            QTableWidgetItem* item = new QTableWidgetItem(cells[col]);
            item->setData(Qt::UserRole, a.address);
            this->_addresses->setItem(i, col, item);
        }
    }
    this->refreshLock_();
}

void xdesk::ui::gui::HostWindow::refreshLock_(void)
{
    const bool locked = this->_host.localActionsLocked();

    for (QPushButton* button: this->_localButtons)
        button->setEnabled(!locked);
    if (this->_settingsSave) this->_settingsSave->setEnabled(!locked);
    this->_lockInfo->setText(locked ? "Locked: a remote session controls this PC or has a terminal. Trust, unban and settings need the person in front of the PC: stop the remote control first." : "");
    set_tone(this->_lockInfo, locked ? "warning" : "");
}

bool xdesk::ui::gui::HostWindow::confirmLocal_(const QString& question, const bool unban)
{
    bool ok = false;
    QString typed;

    if (this->_host.localActionsLocked()) {
        QMessageBox::warning(this, "Locked", "A remote session controls this PC or has a terminal: this action needs the person in front of the PC.");
        return false;
    }
    if (!unban) return QMessageBox::question(this, "xdesk", question) == QMessageBox::Yes && !this->_host.localActionsLocked();

    // Lifting a ban: the word typed (a click alone could be a mistake)
    typed = QInputDialog::getText(this, "Unban", question + "\n\nType " + XDESK_UNBAN_WORD + " to confirm:", QLineEdit::Normal, "", &ok);
    if (!ok) return false;
    if (typed.trimmed().compare(XDESK_UNBAN_WORD, Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, "Unban", "Not confirmed: nothing was changed.");
        return false;
    }
    return !this->_host.localActionsLocked();
}

//----------------------------------------------------------------//
// Logs

QWidget* xdesk::ui::gui::HostWindow::buildLogs_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QPushButton* open = make_button("Open the folder", "ghost", "logs");
    QWidget* body = nullptr;
    QFrame* card = nullptr;
    QVBoxLayout* bodyLayout = nullptr;

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(make_page_head("Logs", "Audit trail of every connection, decision, input and terminal command (jsonl, one file per day). Double click an entry for its details."));
    this->_logLevel = new QComboBox();
    this->_logLevel->addItems({"all levels", "info", "warn", "alert"});
    this->_logCategory = new QComboBox();
    this->_logCategory->addItems({"all categories", "server", "auth", "session", "input", "terminal", "tamper", "device", "settings"});
    this->_logSearch = new QLineEdit();
    this->_logSearch->setPlaceholderText("Search (session, address, text)...");
    this->_logSearch->setClearButtonEnabled(true);
    this->_logSearch->setMinimumWidth(240);
    card = make_panel("Entries", body, {this->_logLevel, this->_logCategory, this->_logSearch, open});
    bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    this->_logs = new QTreeWidget();
    this->_logs->setHeaderLabels({"TIME", "LEVEL", "CATEGORY", "SESSION", "MESSAGE"});
    this->_logs->setRootIsDecorated(false);
    this->_logs->setAlternatingRowColors(true);
    this->_logs->setUniformRowHeights(true);
    this->_logs->setFrameShape(QFrame::NoFrame);
    this->_logs->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->_logs->header()->setStretchLastSection(true);
    this->_logs->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    Theme::decorate(this->_logs);
    bodyLayout->addWidget(this->_logs);
    layout->addWidget(card, 1);
    QObject::connect(this->_logLevel, &QComboBox::currentIndexChanged, this, &HostWindow::filterLogs_);
    QObject::connect(this->_logCategory, &QComboBox::currentIndexChanged, this, &HostWindow::filterLogs_);
    QObject::connect(this->_logSearch, &QLineEdit::textChanged, this, &HostWindow::filterLogs_);
    QObject::connect(open, &QPushButton::clicked, this, []() {QDesktopServices::openUrl(QUrl::fromLocalFile(core::Config::logDir()));});
    QObject::connect(this->_logs, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, const int) {
        QMessageBox::information(this, "Log entry", QString::fromUtf8(QJsonDocument(QJsonDocument::fromJson(item->data(0, Qt::UserRole).toByteArray()).object()).toJson(QJsonDocument::Indented)));
    });
    return page;
}

void xdesk::ui::gui::HostWindow::addLog_(const QJsonObject& entry)
{
    QTreeWidgetItem* item = new QTreeWidgetItem({
        QDateTime::fromMSecsSinceEpoch(entry.value("ts").toInteger()).toString("MM-dd HH:mm:ss"), entry.value("level").toString(),
        entry.value("cat").toString(), entry.value("session").toString(), entry.value("msg").toString()});
    const QString level = entry.value("level").toString();

    item->setData(0, Qt::UserRole, QJsonDocument(entry).toJson(QJsonDocument::Compact));
    if (level == "alert") item->setForeground(1, Theme::palette().danger);
    else if (level == "warn") item->setForeground(1, Theme::palette().warning);
    this->_logs->insertTopLevelItem(0, item);
    while (this->_logs->topLevelItemCount() > 3000)
        delete this->_logs->takeTopLevelItem(this->_logs->topLevelItemCount() - 1);
    item->setHidden(false);
    if (this->_logLevel->currentIndex() > 0 || this->_logCategory->currentIndex() > 0 || !this->_logSearch->text().isEmpty()) this->filterLogs_();
}

void xdesk::ui::gui::HostWindow::filterLogs_(void)
{
    const QString level = this->_logLevel->currentIndex() > 0 ? this->_logLevel->currentText() : QString();
    const QString category = this->_logCategory->currentIndex() > 0 ? this->_logCategory->currentText() : QString();
    const QString search = this->_logSearch->text();

    for (int i = 0; i < this->_logs->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = this->_logs->topLevelItem(i);
        const bool visible = (level.isEmpty() || item->text(1) == level) && (category.isEmpty() || item->text(2) == category)
            && (search.isEmpty() || QString::fromUtf8(item->data(0, Qt::UserRole).toByteArray()).contains(search, Qt::CaseInsensitive));
        item->setHidden(!visible);
    }
}

//----------------------------------------------------------------//
// Settings

QWidget* xdesk::ui::gui::HostWindow::buildSettings_(void)
{
    QScrollArea* scroll = new QScrollArea();
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QBoxLayout* columns = new QBoxLayout(QBoxLayout::LeftToRight);
    QWidget* networkBody = nullptr;
    QWidget* accessBody = nullptr;
    QWidget* streamBody = nullptr;
    QFrame* networkCard = make_panel("Network", networkBody);
    QFrame* accessCard = make_panel("Access & security", accessBody);
    QFrame* streamCard = make_panel("Stream & interface", streamBody);
    QFormLayout* network = new QFormLayout(networkBody);
    QFormLayout* access = new QFormLayout(accessBody);
    QFormLayout* stream = new QFormLayout(streamBody);
    QVBoxLayout* left = new QVBoxLayout();
    QHBoxLayout* save = new QHBoxLayout();
    QLabel* streamHint = make_label("Lower = less bandwidth and CPU on this PC. Resolution caps the frames sent (a viewer can ask for less, never for more).", "muted");

    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->setContentsMargins(0, 0, 4, 0);
    layout->setSpacing(16);
    layout->addWidget(make_page_head("Settings", "Saved in config.json. Changing them needs the person in front of the PC (locked while a remote session has control)."));
    for (QFormLayout* form: {network, access, stream}) {
        form->setContentsMargins(18, 14, 18, 16);
        form->setVerticalSpacing(10);
        form->setHorizontalSpacing(16);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    }

    // Network
    this->_setPort = new QSpinBox();
    this->_setPort->setRange(1, 65535);
    this->_setBind = new QLineEdit();
    this->_setExposure = make_exposure_box();
    this->_setPublicHost = new QLineEdit();
    this->_setPublicHost->setPlaceholderText("my.domain.tld (manual exposure)");
    this->_setDiscovery = new QCheckBox("Announce on the LAN (xdesk scan)");
    this->_setUdp = new QCheckBox("Video over encrypted UDP for the native viewers and the Android app (faster on lossy links)");
    this->_setLanes = new QCheckBox("Extra sockets for the web viewers when one connection holds the video back");
    network->addRow(make_label("Port", "muted"), this->_setPort);
    network->addRow(make_label("Listen on", "muted"), this->_setBind);
    network->addRow(make_label("Exposure", "muted"), this->_setExposure);
    network->addRow(make_label("Public address", "muted"), this->_setPublicHost);
    network->addRow("", this->_setDiscovery);
    network->addRow("", this->_setUdp);
    network->addRow("", this->_setLanes);

    // Stream
    this->_setFps = new QSpinBox();
    this->_setFps->setRange(1, 120);
    this->_setFps->setSuffix(" fps");
    this->_setResolution = new QComboBox();
    for (const QPair<QString, int>& preset: QList<QPair<QString, int>>{{"Screen size (native)", 0}, {"1440p", 1440}, {"1080p - Full HD", 1080}, {"720p - HD", 720}, {"480p", 480}, {"360p", 360}, {"240p", 240}, {"144p", 144}})
        this->_setResolution->addItem(preset.first, preset.second);
    this->_setQuality = new QComboBox();
    for (const QPair<QString, int>& preset: QList<QPair<QString, int>>{{"Low - 40 (blurry text, very light)", 40}, {"Medium - 55", 55}, {"High - 70 (default)", 70}, {"Very high - 85", 85}, {"Maximum - 100 (heavy)", 100}})
        this->_setQuality->addItem(preset.first, preset.second);
    streamHint->setWordWrap(true);
    this->_setLanguage = new QComboBox();
    this->_setLanguage->addItems({"en", "fr"});
    this->_setMinimized = new QCheckBox("Start in the tray");
    stream->addRow(make_label("Max FPS", "muted"), this->_setFps);
    stream->addRow(make_label("Resolution", "muted"), this->_setResolution);
    stream->addRow(make_label("Image quality", "muted"), this->_setQuality);
    stream->addRow(streamHint);
    stream->addRow(make_label("Language", "muted"), this->_setLanguage);
    stream->addRow("", this->_setMinimized);
    left->setSpacing(16);
    left->addWidget(networkCard);
    left->addWidget(streamCard, 1); // both columns end on the same line

    // Access
    this->_setCodeLength = new QSpinBox();
    this->_setCodeLength->setRange(6, 24);
    this->_setCodeLifetime = new QSpinBox();
    this->_setCodeLifetime->setRange(0, 86400);
    this->_setCodeLifetime->setSuffix(" s");
    this->_setApproval = new QComboBox();
    this->_setApproval->addItems({"untrusted", "always", "never"});
    this->_setMaxSessions = new QSpinBox();
    this->_setMaxSessions->setRange(1, 64);
    this->_setMaxFailures = new QSpinBox();
    this->_setMaxFailures->setRange(1, 100);
    this->_setIdle = new QSpinBox();
    this->_setIdle->setRange(0, 86400);
    this->_setIdle->setSuffix(" s");
    this->_setAllowControl = new QCheckBox("Allow keyboard && mouse control");
    this->_setAllowTerminal = new QCheckBox("Allow remote terminals");
    this->_setAllowWeb = new QCheckBox("Allow browsers and the android app");
    this->_setViewOnly = new QCheckBox("New sessions start view only");
    this->_setBanOnTamper = new QCheckBox("Ban a session that touches xdesk");
    this->_setFocusRequest = new QCheckBox("Viewers may ask the control back (panel focused)");
    this->_setFocusRequest->setToolTip("The remote control pauses while this panel has the focus; a viewer may ask it back, granted after the delay unless refused here");
    this->_setFocusDelay = new QSpinBox();
    this->_setFocusDelay->setRange(5, 3600);
    this->_setFocusDelay->setSingleStep(5);
    this->_setFocusDelay->setSuffix(" s");
    this->_setShell = new QLineEdit();
    this->_setShell->setPlaceholderText("default shell");
    access->addRow(make_label("Code length", "muted"), this->_setCodeLength);
    access->addRow(make_label("Code lifetime", "muted"), this->_setCodeLifetime);
    access->addRow(make_label("Ask before accepting", "muted"), this->_setApproval);
    access->addRow(make_label("Max sessions", "muted"), this->_setMaxSessions);
    access->addRow(make_label("Failures before ban", "muted"), this->_setMaxFailures);
    access->addRow(make_label("Idle kick", "muted"), this->_setIdle);
    access->addRow("", this->_setAllowControl);
    access->addRow("", this->_setAllowTerminal);
    access->addRow("", this->_setAllowWeb);
    access->addRow("", this->_setViewOnly);
    access->addRow("", this->_setBanOnTamper);
    access->addRow("", this->_setFocusRequest);
    access->addRow(make_label("Control given back after", "muted"), this->_setFocusDelay);
    access->addRow(make_label("Terminal shell", "muted"), this->_setShell);
    columns->setSpacing(16);
    columns->addLayout(left, 1);
    columns->addWidget(accessCard, 1);
    layout->addLayout(columns);
    this->_settingsSave = make_button("Save", "primary");
    QPushButton* reset = make_button("Revert", "ghost");
    save->addStretch();
    save->addWidget(reset);
    save->addWidget(this->_settingsSave);
    layout->addLayout(save);
    layout->addStretch();
    QObject::connect(this->_settingsSave, &QPushButton::clicked, this, &HostWindow::saveSettings_);
    QObject::connect(reset, &QPushButton::clicked, this, &HostWindow::loadSettings_);

    // One width for the short fields (numbers, lists): the values form a column
    for (QWidget* field: page->findChildren<QWidget*>())
        if (qobject_cast<QAbstractSpinBox*>(field) || qobject_cast<QComboBox*>(field)) field->setMinimumWidth(200);
    scroll->setWidget(page);
    this->_settingsScroll = scroll;
    this->_settingsColumns = columns;
    scroll->viewport()->installEventFilter(this);
    return scroll;
}

void xdesk::ui::gui::HostWindow::loadSettings_(void)
{
    const core::Settings& s = this->_host.config().settings();

    this->_setPort->setValue(s.port);
    this->_setBind->setText(s.bind);
    this->_setExposure->setCurrentIndex(std::max(0, this->_setExposure->findData(core::exposure_name(s.exposure))));
    this->_setPublicHost->setText(s.publicHost);
    this->_setDiscovery->setChecked(s.discovery);
    this->_setUdp->setChecked(s.udp);
    this->_setLanes->setChecked(s.lanes);
    this->_setCodeLength->setValue(s.codeLength);
    this->_setCodeLifetime->setValue(s.codeLifetime);
    this->_setApproval->setCurrentText(core::approval_name(s.approval));
    this->_setMaxSessions->setValue(s.maxSessions);
    this->_setMaxFailures->setValue(s.maxAuthFailures);
    this->_setIdle->setValue(s.idleTimeout);
    this->_setAllowControl->setChecked(s.allowControl);
    this->_setAllowTerminal->setChecked(s.allowTerminal);
    this->_setAllowWeb->setChecked(s.allowWeb);
    this->_setViewOnly->setChecked(s.viewOnlyDefault);
    this->_setBanOnTamper->setChecked(s.banOnTamper);
    this->_setFocusRequest->setChecked(s.focusRequest);
    this->_setFocusDelay->setValue(s.focusRequestDelay);
    this->_setShell->setText(s.shell);
    this->_setFps->setValue(s.fps);
    this->_setResolution->setCurrentIndex(std::max(0, this->_setResolution->findData(s.maxHeight)));
    if (this->_setResolution->findData(s.maxHeight) < 0) {
        this->_setResolution->addItem(QString::number(s.maxHeight) + "p (config.json)", s.maxHeight);
        this->_setResolution->setCurrentIndex(this->_setResolution->count() - 1);
    }
    if (this->_setQuality->findData(s.quality) < 0) this->_setQuality->addItem("Custom - " + QString::number(s.quality) + " (config.json)", s.quality);
    this->_setQuality->setCurrentIndex(this->_setQuality->findData(s.quality));
    this->_setLanguage->setCurrentText(s.language);
    this->_setMinimized->setChecked(s.startMinimized);
}

void xdesk::ui::gui::HostWindow::saveSettings_(void)
{
    core::Settings s = this->_host.config().settings();

    s.port = static_cast<std::uint16_t>(this->_setPort->value());
    s.bind = this->_setBind->text().trimmed();
    s.exposure = core::exposure_from(this->_setExposure->currentData().toString());
    s.publicHost = this->_setPublicHost->text().trimmed();
    s.discovery = this->_setDiscovery->isChecked();
    s.udp = this->_setUdp->isChecked();
    s.lanes = this->_setLanes->isChecked();
    s.codeLength = this->_setCodeLength->value();
    s.codeLifetime = this->_setCodeLifetime->value();
    s.approval = core::approval_from(this->_setApproval->currentText());
    s.maxSessions = this->_setMaxSessions->value();
    s.maxAuthFailures = this->_setMaxFailures->value();
    s.idleTimeout = this->_setIdle->value();
    s.allowControl = this->_setAllowControl->isChecked();
    s.allowTerminal = this->_setAllowTerminal->isChecked();
    s.allowWeb = this->_setAllowWeb->isChecked();
    s.viewOnlyDefault = this->_setViewOnly->isChecked();
    s.banOnTamper = this->_setBanOnTamper->isChecked();
    s.focusRequest = this->_setFocusRequest->isChecked();
    s.focusRequestDelay = this->_setFocusDelay->value();
    s.shell = this->_setShell->text().trimmed();
    s.fps = this->_setFps->value();
    s.quality = this->_setQuality->currentData().toInt();
    s.maxHeight = this->_setResolution->currentData().toInt();
    s.language = this->_setLanguage->currentText();
    s.startMinimized = this->_setMinimized->isChecked();
    try {
        if (!this->_host.applySettings(s)) {QMessageBox::warning(this, "Locked", "A remote session controls this PC: settings unchanged."); return;}
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Settings", QString("Could not apply the settings: ") + e.what());
        return;
    }
    QMessageBox::information(this, "Settings", "Settings saved.");
}
