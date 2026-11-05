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
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QApplication>
#include <QInputDialog>
#include <QHeaderView>
#include <QFormLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QClipboard>
#include <QDateTime>
#include <QHostInfo>
#include <QSplitter>
#include <QDialog>
#include <QAction>
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

static void set_tone(QLabel* label, const QString& tone)
{
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

static QTableWidget* make_table(const QStringList& headers)
{
    QTableWidget* table = new QTableWidget(0, static_cast<int>(headers.size()));

    QStringList upper;

    for (const QString& header: headers)
        upper.append(header.toUpper());
    table->setHorizontalHeaderLabels(upper);
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
    QHBoxLayout* bannerLayout = nullptr;
    QPushButton* bannerClose = nullptr;

    this->setWindowTitle("xdesk - remote access");
    this->setWindowIcon(QIcon(Theme::logo(64)));
    this->resize(1180, 760);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(210);
    sideLayout->setContentsMargins(14, 18, 14, 14);
    logo->setPixmap(Theme::logo(30));
    brand->addWidget(logo);
    brand->addWidget(make_label("xdesk", "brand"));
    brand->addStretch();
    brand->addWidget(make_label(QString("v") + XDESK_VERSION, "version"));
    sideLayout->addLayout(brand);
    sideLayout->addSpacing(14);
    this->_nav = new QListWidget(sidebar);
    this->_nav->setObjectName("nav");
    this->_nav->setFrameShape(QFrame::NoFrame);
    this->_nav->setIconSize(QSize(18, 18));
    for (const std::pair<QString, QString>& entry: {std::pair<QString, QString>{"Overview", "overview"}, {"Sessions", "sessions"}, {"Devices", "devices"}, {"Logs", "logs"}, {"Settings", "settings"}})
        this->_nav->addItem(new QListWidgetItem(Theme::icon(entry.second), entry.first));
    sideLayout->addWidget(this->_nav, 1);
    sideLayout->addWidget(make_label("Theme", "subtitle"));
    sideLayout->addWidget(Theme::switcher([this](const QString& theme) {this->_host.setTheme(theme);}, sidebar));
    sideLayout->addSpacing(10);
    this->_statusSessions = make_label("No session", "muted");
    sideLayout->addWidget(this->_statusSessions);
    QPushButton* stop = make_button("Stop sharing", "danger", "power");
    sideLayout->addWidget(stop);
    QObject::connect(stop, &QPushButton::clicked, this, &HostWindow::quit_);

    // Pages + tamper banner
    mainLayout->setContentsMargins(22, 18, 22, 18);
    this->_banner = new QWidget(main);
    this->_banner->setObjectName("banner");
    bannerLayout = new QHBoxLayout(this->_banner);
    this->_bannerText = new QLabel(this->_banner);
    this->_bannerText->setStyleSheet("color: white; font-weight: 600;");
    this->_bannerText->setWordWrap(true);
    bannerClose = make_button("Dismiss", "ghost");
    bannerClose->setStyleSheet("color: white;");
    bannerLayout->addWidget(Theme::icon("shield", Qt::white).pixmap(20, 20).isNull() ? new QLabel() : [&]() {QLabel* l = new QLabel(); l->setPixmap(Theme::icon("shield", Qt::white).pixmap(20, 20)); return l;}());
    bannerLayout->addWidget(this->_bannerText, 1);
    bannerLayout->addWidget(bannerClose);
    this->_banner->hide();
    QObject::connect(bannerClose, &QPushButton::clicked, this->_banner, &QWidget::hide);
    mainLayout->addWidget(this->_banner);
    this->_pages = new QStackedWidget(main);
    this->_pages->addWidget(this->buildOverview_());
    this->_pages->addWidget(this->buildSessions_());
    this->_pages->addWidget(this->buildDevices_());
    this->_pages->addWidget(this->buildLogs_());
    this->_pages->addWidget(this->buildSettings_());
    mainLayout->addWidget(this->_pages, 1);
    QObject::connect(this->_nav, &QListWidget::currentRowChanged, this->_pages, &QStackedWidget::setCurrentIndex);
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
    #ifdef _WIN32
        // The remote sessions never see the panel (Windows 10 2004+)
        SetWindowDisplayAffinity(reinterpret_cast<HWND>(this->winId()), 0x00000011); // WDA_EXCLUDEFROMCAPTURE
    #endif
}

void xdesk::ui::gui::HostWindow::changeEvent(QEvent* event)
{
    // Remote input is paused while the local user works in the panel
    if (event->type() == QEvent::ActivationChange) this->_host.setPanelFocused(this->isActiveWindow());
    QMainWindow::changeEvent(event);
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
    this->refreshOverview_();
    this->refreshDevices_();
}

void xdesk::ui::gui::HostWindow::closeEvent(QCloseEvent* event)
{
    if (this->_quitting || !this->_tray || !QSystemTrayIcon::isSystemTrayAvailable()) {
        if (!this->_quitting && !this->_host.sessions().isEmpty() && QMessageBox::question(this, "Stop sharing", "Sessions are connected. Stop sharing and quit xdesk?") != QMessageBox::Yes) {
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
        this->_tray->showMessage("xdesk is still sharing", "The panel is in the tray. Quit from the tray menu to stop sharing.", QSystemTrayIcon::Information, 4000);
        this->_trayHintShown = true;
    }
}

void xdesk::ui::gui::HostWindow::quit_(void)
{
    if (!this->_host.sessions().isEmpty() && QMessageBox::question(this, "Stop sharing", "Every session will be disconnected. Stop sharing and quit xdesk?") != QMessageBox::Yes) return;
    this->_quitting = true;
    this->_host.kickAll("The host stopped sharing");
    QApplication::quit();
}

void xdesk::ui::gui::HostWindow::buildTray_(void)
{
    QMenu* menu = new QMenu(this);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    this->_tray = new QSystemTrayIcon(QIcon(Theme::logo(64)), this);
    this->_tray->setToolTip("xdesk - sharing");
    menu->addAction("Open the panel", this, [this]() {this->showNormal(); this->raise(); this->activateWindow();});
    menu->addAction("Copy the code", this, [this]() {QGuiApplication::clipboard()->setText(this->_host.code().display());});
    menu->addAction("New code", this, [this]() {this->_host.rotateCode();});
    menu->addSeparator();
    menu->addAction("Disconnect everybody", this, [this]() {this->_host.kickAll("Disconnected by the host");});
    menu->addAction("Quit (stop sharing)", this, &HostWindow::quit_);
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
    QFrame* codeCard = make_card();
    QVBoxLayout* codeLayout = new QVBoxLayout(codeCard);
    QHBoxLayout* codeButtons = new QHBoxLayout();
    QPushButton* copy = make_button("Copy", "", "copy");
    QPushButton* rotate = make_button("New code", "ghost", "refresh");
    QFrame* statusCard = make_card();
    QFormLayout* statusLayout = new QFormLayout(statusCard);
    QFrame* endpointsCard = make_card();
    QVBoxLayout* endpointsLayout = new QVBoxLayout(endpointsCard);
    QHBoxLayout* exposureRow = new QHBoxLayout();

    layout->setSpacing(16);
    layout->addWidget(make_label("Overview", "title"));
    this->_identity = make_label("", "muted");
    layout->addWidget(this->_identity);

    // Code
    codeCard->setProperty("accent", true);
    codeLayout->setContentsMargins(22, 18, 22, 18);
    codeLayout->addWidget(make_label("Connection code", "subtitle"));
    codeLayout->addWidget(make_label("Give it to the person who connects: it works once, then changes", "muted"));
    this->_code = make_label("---", "code");
    codeLayout->addWidget(this->_code);
    this->_codeBar = new QProgressBar(codeCard);
    this->_codeBar->setTextVisible(false);
    this->_codeBar->setFixedHeight(4);
    codeLayout->addWidget(this->_codeBar);
    this->_codeTimer = make_label("", "muted");
    codeLayout->addWidget(this->_codeTimer);
    this->_fingerprint = make_label("", "muted");
    this->_fingerprint->setToolTip(security::fingerprint_text(this->_host.certificate().fingerprint()));
    codeLayout->addWidget(this->_fingerprint);
    codeButtons->addWidget(copy);
    codeButtons->addWidget(rotate);
    codeButtons->addStretch();
    codeLayout->addLayout(codeButtons);
    QObject::connect(copy, &QPushButton::clicked, this, [this]() {QGuiApplication::clipboard()->setText(this->_host.code().display());});
    QObject::connect(rotate, &QPushButton::clicked, this, [this]() {this->_host.rotateCode();});

    // Status
    statusLayout->setContentsMargins(20, 18, 20, 18);
    statusLayout->setVerticalSpacing(10);
    statusLayout->addRow(make_label("Status", "subtitle"));
    this->_statusCapture = make_label("-");
    this->_statusInput = make_label("-");
    this->_statusTls = make_label(security::tls_summary());
    this->_statusTls->setWordWrap(true);
    statusLayout->addRow("Screen", this->_statusCapture);
    statusLayout->addRow("Input", this->_statusInput);
    statusLayout->addRow("Encryption", this->_statusTls);
    top->addWidget(codeCard, 3);
    top->addWidget(statusCard, 2);
    layout->addLayout(top);

    // Endpoints
    endpointsLayout->setContentsMargins(20, 18, 20, 18);
    exposureRow->addWidget(make_label("Who can reach this PC", "subtitle"));
    exposureRow->addStretch();
    exposureRow->addWidget(make_label("Open to", "muted"));
    this->_exposure = make_exposure_box();
    exposureRow->addWidget(this->_exposure);
    endpointsLayout->addLayout(exposureRow);
    this->_exposureStatus = make_label("", "muted");
    this->_exposureStatus->setWordWrap(true);
    endpointsLayout->addWidget(this->_exposureStatus);
    this->_endpoints = make_table({"Path", "Who can use it", "Address"});
    this->_endpoints->horizontalHeader()->setStretchLastSection(false);
    this->_endpoints->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->_endpoints->setWordWrap(true);
    endpointsLayout->addWidget(this->_endpoints);
    QObject::connect(this->_endpoints, &QTableWidget::cellDoubleClicked, this, [this](const int row, const int column) {
        const QTableWidgetItem* item = this->_endpoints->item(row, 2);
        (void)column;
        QGuiApplication::clipboard()->setText(item->text());
        this->_exposureStatus->setText("Copied " + item->text() + "  (xdesk app: " + item->data(Qt::UserRole).toString() + ")");
    });
    QObject::connect(this->_exposure, &QComboBox::activated, this, [this](const int) {
        core::Settings s = this->_host.config().settings();
        s.exposure = core::exposure_from(this->_exposure->currentData().toString());
        if (!this->_host.applySettings(s)) QMessageBox::warning(this, "Locked", "Settings cannot change while a remote session controls this PC or has a terminal.");
        this->loadSettings_();
    });
    layout->addWidget(endpointsCard, 1);
    return page;
}

void xdesk::ui::gui::HostWindow::refreshOverview_(void)
{
    const security::PairingCode& code = this->_host.code();
    const int left = code.secondsLeft();
    const int lifetime = std::max(1, this->_host.config().settings().codeLifetime);
    const QList<net::Endpoint>& endpoints = this->_host.exposure().endpoints();
    int active = 0;

    this->_code->setText(code.display());
    this->_codeBar->setVisible(left >= 0);
    this->_codeBar->setRange(0, lifetime);
    this->_codeBar->setValue(std::max(0, left));
    this->_codeTimer->setText(left >= 0 ? "Changes in " + QString::number(left / 60) + ":" + QString::number(left % 60).rightJustified(2, '0') + ", and after each use" : "Changes after each use");
    this->_fingerprint->setText("Host identity " + security::fingerprint_short(this->_host.certificate().fingerprint()) + "  (the native clients show the same one)");
    this->_statusCapture->setText(this->_host.captureBackend().isEmpty() ? QString("idle, starts with the first viewer") : this->_host.captureBackend());
    set_tone(this->_statusCapture, this->_host.captureBackend().startsWith("none") ? "danger" : (this->_host.captureBackend().isEmpty() ? "muted" : "success"));
    this->_statusInput->setText(!this->_host.inputError().isEmpty() ? "unavailable" : (this->_host.captureBackend().isEmpty() ? QString("idle") : this->_host.inputBackend()));
    this->_statusInput->setToolTip(this->_host.inputError());
    set_tone(this->_statusInput, !this->_host.inputError().isEmpty() ? "warning" : (this->_host.inputBackend() == "none" ? "muted" : "success"));
    const net::Tailnet& tailnet = this->_host.exposure().tailnet();
    QString identity = "This PC is <b>" + QHostInfo::localHostName().toHtmlEscaped() + "</b>";
    if (this->_host.exposure().mode() == core::Exposure::Tailscale && tailnet.up && !tailnet.dns.isEmpty())
        identity += "  ·  on Tailscale it is <b>" + tailnet.dns.section('.', 0, 0).toHtmlEscaped() + "</b> (" + tailnet.dns.toHtmlEscaped() + ")";
    this->_identity->setText(identity);
    this->_exposure->setCurrentIndex(std::max(0, this->_exposure->findData(core::exposure_name(this->_host.exposure().mode()))));
    this->_exposureStatus->setText(this->_host.exposure().status() + ".  Open the address in a browser or a phone, or give it to the xdesk app. Double click to copy.");
    this->_endpoints->setRowCount(static_cast<int>(endpoints.size()));
    for (int i = 0; i < endpoints.size(); ++i) {
        const net::Endpoint& e = endpoints[i];
        const QString command = "xdesk connect " + e.host + (e.port != XDESK_DEFAULT_PORT ? " -p " + QString::number(e.port) : "");
        const QString tip = command + "\n" + (e.ip.isEmpty() ? QString() : "IP address " + e.ip + "\n") + (e.endToEnd ? "Encrypted end to end (TLS 1.3 + ML-KEM)" : "The relay decrypts the traffic: prefer another path for sensitive work");
        QTableWidgetItem* items[3] = {new QTableWidgetItem(e.label), new QTableWidgetItem(e.reach), new QTableWidgetItem(e.url)};
        items[2]->setData(Qt::UserRole, command);
        items[2]->setFont(QFont(Theme::monospaceFamily(), 9));
        for (int c = 0; c < 3; ++c) {
            items[c]->setToolTip(tip);
            this->_endpoints->setItem(i, c, items[c]);
        }
        if (!e.endToEnd) items[1]->setForeground(Theme::palette().warning);
    }
    for (const host::Session* s: this->_host.sessions())
        active += s->state() == host::State::Active;
    this->_statusSessions->setText(active == 0 ? "No session" : QString::number(active) + (active == 1 ? " session connected" : " sessions connected"));
    if (this->_tray) this->_tray->setToolTip("xdesk - " + this->_statusSessions->text() + " - code " + code.display());
}

//----------------------------------------------------------------//
// Sessions

QWidget* xdesk::ui::gui::HostWindow::buildSessions_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QSplitter* splitter = new QSplitter(Qt::Vertical, page);
    QFrame* detailCard = make_card();
    QVBoxLayout* detailLayout = new QVBoxLayout(detailCard);
    QHBoxLayout* head = new QHBoxLayout();
    QHBoxLayout* perms = new QHBoxLayout();
    QPushButton* kick = make_button("Disconnect", "", "power");
    QPushButton* ban = make_button("Ban", "danger", "lock");
    QTabWidget* tabs = new QTabWidget();

    layout->addWidget(make_label("Sessions", "title"));
    layout->addWidget(make_label("Everything each connected device does, live. Select a session to see its inputs and terminals.", "muted"));
    this->_sessions = make_table({"Device", "Client", "Address", "State", "Mode", "Rights", "FPS", "Latency", "Kbit/s", "Since", "Last activity"});
    splitter->addWidget(this->_sessions);
    QObject::connect(this->_sessions, &QTableWidget::itemSelectionChanged, this, [this]() {
        const QList<QTableWidgetItem*> items = this->_sessions->selectedItems();
        this->selectSession_(items.isEmpty() ? QString() : items.front()->data(Qt::UserRole).toString());
    });

    // Detail
    detailLayout->setContentsMargins(18, 14, 18, 14);
    this->_detailTitle = make_label("Select a session", "subtitle");
    head->addWidget(this->_detailTitle, 1);
    head->addWidget(kick);
    head->addWidget(ban);
    detailLayout->addLayout(head);
    this->_detailInfo = make_label("", "muted");
    this->_detailInfo->setWordWrap(true);
    detailLayout->addWidget(this->_detailInfo);
    this->_permView = new QCheckBox("See the screen");
    this->_permControl = new QCheckBox("Control keyboard & mouse");
    this->_permTerminal = new QCheckBox("Open terminals");
    this->_permClipboard = new QCheckBox("Clipboard");
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
        << session->mode() << rights.join(", ") << QString::number(st.fps) << (st.latency >= 0 ? QString::number(st.latency) + " ms" : "-")
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
    QPushButton* trust = make_button("Accept & trust", "primary", "shield");
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
    QHBoxLayout* deviceButtons = new QHBoxLayout();
    QHBoxLayout* addressButtons = new QHBoxLayout();
    QPushButton* trust = make_button("Trust", "", "shield");
    QPushButton* untrust = make_button("Untrust", "ghost");
    QPushButton* unban = make_button("Unban", "primary", "lock");
    QPushButton* forget = make_button("Forget", "danger");
    QPushButton* unbanAddress = make_button("Unban address", "primary", "lock");
    std::function<QString(QTableWidget*)> selected = [](QTableWidget* table) {
        const QList<QTableWidgetItem*> items = table->selectedItems();
        return items.isEmpty() ? QString() : items.front()->data(Qt::UserRole).toString();
    };

    layout->addWidget(make_label("Devices", "title"));
    layout->addWidget(make_label("Trusted devices reconnect without a code. Banned devices and addresses are refused until you unban them here, on this PC.", "muted"));
    this->_lockInfo = make_label("", "");
    this->_lockInfo->setWordWrap(true);
    layout->addWidget(this->_lockInfo);
    this->_devices = make_table({"Device", "Client", "OS", "Last address", "State", "Sessions", "Last seen", "Reason"});
    layout->addWidget(this->_devices, 2);
    deviceButtons->addWidget(trust);
    deviceButtons->addWidget(untrust);
    deviceButtons->addStretch();
    deviceButtons->addWidget(forget);
    deviceButtons->addWidget(unban);
    layout->addLayout(deviceButtons);
    layout->addWidget(make_label("Banned addresses", "subtitle"));
    this->_addresses = make_table({"Address", "Reason", "Since", "Until"});
    layout->addWidget(this->_addresses, 1);
    addressButtons->addStretch();
    addressButtons->addWidget(unbanAddress);
    layout->addLayout(addressButtons);
    this->_localButtons = {trust, untrust, unban, forget, unbanAddress};
    QObject::connect(trust, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty() && this->confirmPresence_("trust this device")) (void)this->_host.setTrusted(id, true);
    });
    QObject::connect(untrust, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty()) (void)this->_host.setTrusted(id, false);
    });
    QObject::connect(unban, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty() && this->confirmPresence_("unban this device")) (void)this->_host.unban(id);
    });
    QObject::connect(forget, &QPushButton::clicked, this, [this, selected]() {
        const QString id = selected(this->_devices);
        if (!id.isEmpty() && QMessageBox::question(this, "Forget", "Forget this device (and its ban)?") == QMessageBox::Yes && this->confirmPresence_("forget this device")) (void)this->_host.forget(id);
    });
    QObject::connect(unbanAddress, &QPushButton::clicked, this, [this, selected]() {
        const QString address = selected(this->_addresses);
        if (!address.isEmpty() && this->confirmPresence_("unban " + address)) (void)this->_host.unbanAddress(address);
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
        const QStringList cells = {d.name, d.kind, d.os, d.address, d.banned ? "banned" : (d.trusted ? "trusted" : "known"), QString::number(d.sessions), QDateTime::fromMSecsSinceEpoch(d.seen).toString("yyyy-MM-dd HH:mm"), d.banReason};
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

bool xdesk::ui::gui::HostWindow::confirmPresence_(const QString& action)
{
    const QString challenge = security::random_code(6).remove('-');
    bool ok = false;
    QString typed;

    if (this->_host.localActionsLocked()) {
        QMessageBox::warning(this, "Locked", "A remote session controls this PC or has a terminal: this action needs the person in front of the PC.");
        return false;
    }
    typed = QInputDialog::getText(this, "Confirm on this PC", "To " + action + ", type the code below (proof that you are in front of this PC):\n\n" + challenge, QLineEdit::Normal, "", &ok);
    if (!ok) return false;
    if (security::normalize_code(typed) != security::normalize_code(challenge)) {
        QMessageBox::warning(this, "Wrong code", "The code does not match: nothing was changed.");
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
    QHBoxLayout* filters = new QHBoxLayout();
    QPushButton* open = make_button("Open the log folder", "ghost", "logs");

    layout->addWidget(make_label("Logs", "title"));
    layout->addWidget(make_label("Audit trail of every connection, decision, input and terminal command (jsonl, one file per day).", "muted"));
    this->_logLevel = new QComboBox();
    this->_logLevel->addItems({"all levels", "info", "warn", "alert"});
    this->_logCategory = new QComboBox();
    this->_logCategory->addItems({"all categories", "server", "auth", "session", "input", "terminal", "tamper", "device", "settings"});
    this->_logSearch = new QLineEdit();
    this->_logSearch->setPlaceholderText("Search (session, address, text)...");
    this->_logSearch->setClearButtonEnabled(true);
    filters->addWidget(this->_logLevel);
    filters->addWidget(this->_logCategory);
    filters->addWidget(this->_logSearch, 1);
    filters->addWidget(open);
    layout->addLayout(filters);
    this->_logs = new QTreeWidget();
    this->_logs->setHeaderLabels({"Time", "Level", "Category", "Session", "Message"});
    this->_logs->setRootIsDecorated(false);
    this->_logs->setAlternatingRowColors(true);
    this->_logs->setUniformRowHeights(true);
    this->_logs->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->_logs->header()->setStretchLastSection(true);
    layout->addWidget(this->_logs, 1);
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
    QHBoxLayout* columns = new QHBoxLayout();
    QFrame* networkCard = make_card();
    QFrame* accessCard = make_card();
    QFrame* streamCard = make_card();
    QFormLayout* network = new QFormLayout(networkCard);
    QFormLayout* access = new QFormLayout(accessCard);
    QFormLayout* stream = new QFormLayout(streamCard);
    QVBoxLayout* left = new QVBoxLayout();
    QHBoxLayout* save = new QHBoxLayout();

    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->addWidget(make_label("Settings", "title"));
    layout->addWidget(make_label("Saved in config.json. Changing them needs the person in front of the PC (locked while a remote session has control).", "muted"));

    // Network
    network->setContentsMargins(20, 16, 20, 16);
    network->addRow(make_label("Network", "subtitle"));
    this->_setPort = new QSpinBox();
    this->_setPort->setRange(1, 65535);
    this->_setBind = new QLineEdit();
    this->_setExposure = make_exposure_box();
    this->_setPublicHost = new QLineEdit();
    this->_setPublicHost->setPlaceholderText("my.domain.tld (manual exposure)");
    this->_setDiscovery = new QCheckBox("Announce on the LAN (xdesk scan)");
    network->addRow("Port", this->_setPort);
    network->addRow("Listen on", this->_setBind);
    network->addRow("Exposure", this->_setExposure);
    network->addRow("Public address", this->_setPublicHost);
    network->addRow("", this->_setDiscovery);

    // Stream
    stream->setContentsMargins(20, 16, 20, 16);
    stream->addRow(make_label("Stream & interface", "subtitle"));
    this->_setFps = new QSpinBox();
    this->_setFps->setRange(1, 120);
    this->_setQuality = new QSlider(Qt::Horizontal);
    this->_setQuality->setRange(10, 100);
    this->_setScale = new QDoubleSpinBox();
    this->_setScale->setRange(0.25, 1.0);
    this->_setScale->setSingleStep(0.05);
    this->_setLanguage = new QComboBox();
    this->_setLanguage->addItems({"en", "fr"});
    this->_setMinimized = new QCheckBox("Start in the tray");
    stream->addRow("Max FPS", this->_setFps);
    stream->addRow("JPEG quality", this->_setQuality);
    stream->addRow("Scale", this->_setScale);
    stream->addRow("Language", this->_setLanguage);
    stream->addRow("", this->_setMinimized);
    left->addWidget(networkCard);
    left->addWidget(streamCard);
    left->addStretch();

    // Access
    access->setContentsMargins(20, 16, 20, 16);
    access->addRow(make_label("Access & security", "subtitle"));
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
    this->_setAllowControl = new QCheckBox("Allow keyboard & mouse control");
    this->_setAllowTerminal = new QCheckBox("Allow remote terminals");
    this->_setAllowWeb = new QCheckBox("Allow browsers and the android app");
    this->_setViewOnly = new QCheckBox("New sessions start view only");
    this->_setBanOnTamper = new QCheckBox("Ban a session that touches xdesk");
    this->_setShell = new QLineEdit();
    this->_setShell->setPlaceholderText("default shell");
    access->addRow("Code length", this->_setCodeLength);
    access->addRow("Code lifetime", this->_setCodeLifetime);
    access->addRow("Ask before accepting", this->_setApproval);
    access->addRow("Max sessions", this->_setMaxSessions);
    access->addRow("Failures before ban", this->_setMaxFailures);
    access->addRow("Idle kick", this->_setIdle);
    access->addRow("", this->_setAllowControl);
    access->addRow("", this->_setAllowTerminal);
    access->addRow("", this->_setAllowWeb);
    access->addRow("", this->_setViewOnly);
    access->addRow("", this->_setBanOnTamper);
    access->addRow("Terminal shell", this->_setShell);
    columns->addLayout(left, 1);
    columns->addWidget(accessCard, 1, Qt::AlignTop);
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
    scroll->setWidget(page);
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
    this->_setShell->setText(s.shell);
    this->_setFps->setValue(s.fps);
    this->_setQuality->setValue(s.quality);
    this->_setScale->setValue(s.scale);
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
    s.shell = this->_setShell->text().trimmed();
    s.fps = this->_setFps->value();
    s.quality = this->_setQuality->value();
    s.scale = this->_setScale->value();
    s.language = this->_setLanguage->currentText();
    s.startMinimized = this->_setMinimized->isChecked();
    if (!this->confirmPresence_("change the settings")) return;
    try {
        if (!this->_host.applySettings(s)) {QMessageBox::warning(this, "Locked", "A remote session controls this PC: settings unchanged."); return;}
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Settings", QString("Could not apply the settings: ") + e.what());
        return;
    }
    QMessageBox::information(this, "Settings", "Settings saved.");
}
