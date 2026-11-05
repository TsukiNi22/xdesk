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
##  @file ViewerWindow.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/gui/ViewerWindow.hpp"
#include "xdesk/platform/Keymap.hpp"
#include "xdesk/net/Exposure.hpp"
#include "xdesk/ui/gui/Theme.hpp"
#include <QJsonDocument>
#include <QApplication>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QWheelEvent>
#include <QStatusBar>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QThread>
#include <QAction>
#include <QFile>

//----------------------------------------------------------------//
// Screen view

xdesk::ui::gui::ScreenView::ScreenView(client::Client& client, QWidget* parent)
: QWidget(parent), _client{client}
{
    this->setFocusPolicy(Qt::StrongFocus);
    this->setMouseTracking(true);
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    this->setAttribute(Qt::WA_InputMethodEnabled, false);
    QObject::connect(&client, &client::Client::frameUpdated, this, [this](const QRect&) {this->update();});
    QObject::connect(&client, &client::Client::cursorMoved, this, &ScreenView::setCursor);
}

void xdesk::ui::gui::ScreenView::setFit(const bool fit)
{
    this->_fit = fit;
    this->setMinimumSize(fit ? QSize(160, 90) : this->_client.canvas().size());
    this->update();
}

void xdesk::ui::gui::ScreenView::setControl(const bool control)
{
    this->_control = control;
    this->setCursor(this->_cursor, this->_cursorVisible);
    QWidget::setCursor(control ? Qt::BlankCursor : Qt::ArrowCursor);
}

void xdesk::ui::gui::ScreenView::setCursor(const QPoint& position, const bool visible)
{
    this->_cursor = position;
    this->_cursorVisible = visible;
    this->update();
}

QRect xdesk::ui::gui::ScreenView::target_(void) const
{
    const QSize canvas = this->_client.canvas().size();
    QSize scaled = canvas;

    if (canvas.isEmpty()) return {};
    if (this->_fit) scaled = canvas.scaled(this->size(), Qt::KeepAspectRatio);
    return QRect(QPoint((this->width() - scaled.width()) / 2, (this->height() - scaled.height()) / 2), scaled);
}

QPoint xdesk::ui::gui::ScreenView::toFrame_(const QPointF& pos) const
{
    const QRect target = this->target_();
    const QSize canvas = this->_client.canvas().size();

    if (target.isEmpty()) return {0, 0};
    return QPoint(std::clamp(static_cast<int>((pos.x() - target.x()) * canvas.width() / target.width()), 0, canvas.width() - 1),
        std::clamp(static_cast<int>((pos.y() - target.y()) * canvas.height() / target.height()), 0, canvas.height() - 1));
}

void xdesk::ui::gui::ScreenView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const QRect target = this->target_();
    const QImage& canvas = this->_client.canvas();

    painter.fillRect(this->rect(), QColor(12, 13, 16));
    if (canvas.isNull()) {
        painter.setPen(QColor(150, 150, 160));
        painter.drawText(this->rect(), Qt::AlignCenter, "Waiting for the first frame...");
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, target.size() != canvas.size());
    painter.drawImage(target, canvas);

    // Remote cursor (drawn here: the frames do not contain it)
    if (this->_cursorVisible && this->_cursor.x() >= 0) {
        const QPointF p(target.x() + this->_cursor.x() * static_cast<double>(target.width()) / canvas.width(),
            target.y() + this->_cursor.y() * static_cast<double>(target.height()) / canvas.height());
        QPolygonF arrow({p, p + QPointF(0, 17), p + QPointF(4.5, 13), p + QPointF(8, 20), p + QPointF(10.5, 19), p + QPointF(7.5, 12), p + QPointF(13, 12)});
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::black, 1.2));
        painter.setBrush(this->_control ? Qt::white : QColor(255, 255, 255, 170));
        painter.drawPolygon(arrow);
    }
}

void xdesk::ui::gui::ScreenView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint p = this->toFrame_(event->position());

    if (this->_control) this->_client.mouseMove(p.x(), p.y());
}

static xdesk::net::MouseButton to_button(const Qt::MouseButton button)
{
    switch (button) {
        case Qt::RightButton:   return xdesk::net::MouseButton::Right;
        case Qt::MiddleButton:  return xdesk::net::MouseButton::Middle;
        case Qt::BackButton:    return xdesk::net::MouseButton::Back;
        case Qt::ForwardButton: return xdesk::net::MouseButton::Forward;
        default:                return xdesk::net::MouseButton::Left;
    }
}

void xdesk::ui::gui::ScreenView::mousePressEvent(QMouseEvent* event)
{
    const QPoint p = this->toFrame_(event->position());

    this->setFocus();
    if (!this->_control) return;
    this->_client.mouseMove(p.x(), p.y());
    this->_client.mouseButton(to_button(event->button()), true);
}

void xdesk::ui::gui::ScreenView::mouseReleaseEvent(QMouseEvent* event)
{
    if (this->_control) this->_client.mouseButton(to_button(event->button()), false);
}

void xdesk::ui::gui::ScreenView::wheelEvent(QWheelEvent* event)
{
    if (this->_control) this->_client.mouseWheel(event->angleDelta().x(), event->angleDelta().y());
    event->accept();
}

QString xdesk::ui::gui::ScreenView::keyCode(const QKeyEvent* event)
{
    const quint32 scan = event->nativeScanCode();
    QString code;

    // Physical key first (the remote layout applies, like a real keyboard)
    #if defined(Q_OS_WIN)
        if (scan != 0) code = platform::code_from_scancode(static_cast<unsigned short>(scan & 0xFF), (scan & 0x100) != 0);
    #elif defined(Q_OS_LINUX)
        if (scan > 8) code = platform::code_from_evdev(static_cast<int>(scan) - 8);
    #endif
    if (!code.isEmpty()) return code;

    // Fallback on the Qt key (macOS, unknown scancodes)
    const int key = event->key();
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return "Key" + QString(QChar('A' + (key - Qt::Key_A)));
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return ((event->modifiers() & Qt::KeypadModifier) ? "Numpad" : "Digit") + QString::number(key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return "F" + QString::number(key - Qt::Key_F1 + 1);
    static const QHash<int, QString> KEYS = {
        {Qt::Key_Escape, "Escape"}, {Qt::Key_Tab, "Tab"}, {Qt::Key_Backtab, "Tab"}, {Qt::Key_Backspace, "Backspace"},
        {Qt::Key_Return, "Enter"}, {Qt::Key_Enter, "NumpadEnter"}, {Qt::Key_Insert, "Insert"}, {Qt::Key_Delete, "Delete"},
        {Qt::Key_Pause, "Pause"}, {Qt::Key_Print, "PrintScreen"}, {Qt::Key_Home, "Home"}, {Qt::Key_End, "End"},
        {Qt::Key_Left, "ArrowLeft"}, {Qt::Key_Up, "ArrowUp"}, {Qt::Key_Right, "ArrowRight"}, {Qt::Key_Down, "ArrowDown"},
        {Qt::Key_PageUp, "PageUp"}, {Qt::Key_PageDown, "PageDown"}, {Qt::Key_Shift, "ShiftLeft"}, {Qt::Key_Control, "ControlLeft"},
        {Qt::Key_Meta, "MetaLeft"}, {Qt::Key_Alt, "AltLeft"}, {Qt::Key_AltGr, "AltRight"}, {Qt::Key_CapsLock, "CapsLock"},
        {Qt::Key_NumLock, "NumLock"}, {Qt::Key_ScrollLock, "ScrollLock"}, {Qt::Key_Menu, "ContextMenu"}, {Qt::Key_Space, "Space"},
        {Qt::Key_Minus, "Minus"}, {Qt::Key_Equal, "Equal"}, {Qt::Key_BracketLeft, "BracketLeft"}, {Qt::Key_BracketRight, "BracketRight"},
        {Qt::Key_Backslash, "Backslash"}, {Qt::Key_Semicolon, "Semicolon"}, {Qt::Key_Apostrophe, "Quote"}, {Qt::Key_QuoteLeft, "Backquote"},
        {Qt::Key_Comma, "Comma"}, {Qt::Key_Period, "Period"}, {Qt::Key_Slash, "Slash"},
    };
    return KEYS.value(key);
}

void xdesk::ui::gui::ScreenView::keyPressEvent(QKeyEvent* event)
{
    const QString code = keyCode(event);

    if (!this->_control || code.isEmpty()) {QWidget::keyPressEvent(event); return;}
    this->_pressed.insert(code);
    this->_client.key(code, true);
    event->accept();
}

void xdesk::ui::gui::ScreenView::keyReleaseEvent(QKeyEvent* event)
{
    const QString code = keyCode(event);

    if (!this->_control || code.isEmpty() || event->isAutoRepeat()) {event->accept(); return;}
    this->_pressed.remove(code);
    this->_client.key(code, false);
    event->accept();
}

void xdesk::ui::gui::ScreenView::focusOutEvent(QFocusEvent* event)
{
    // No stuck key on the remote when the window loses the focus (alt+tab)
    for (const QString& code: this->_pressed)
        this->_client.key(code, false);
    this->_pressed.clear();
    QWidget::focusOutEvent(event);
}

bool xdesk::ui::gui::ScreenView::focusNextPrevChild(const bool)
{
    return false;
}

bool xdesk::ui::gui::ScreenView::event(QEvent* event)
{
    // Tab / shortcuts go to the remote while controlling
    if (event->type() == QEvent::ShortcutOverride && this->_control) {event->accept(); return true;}
    return QWidget::event(event);
}

//----------------------------------------------------------------//
// Window

xdesk::ui::gui::ViewerWindow::ViewerWindow(QWidget* parent)
: QMainWindow(parent)
{
    QWidget* waiting = new QWidget();
    QVBoxLayout* waitingLayout = new QVBoxLayout(waiting);
    QWidget* closed = new QWidget();
    QVBoxLayout* closedLayout = new QVBoxLayout(closed);
    QPushButton* back = new QPushButton("Back");
    QPushButton* cancel = new QPushButton("Cancel");

    this->setWindowTitle("xdesk viewer");
    this->setWindowIcon(QIcon(Theme::logo(64)));
    this->resize(1280, 800);
    this->_pages = new QStackedWidget(this);
    this->_pages->addWidget(this->buildConnect_());

    // Waiting (connecting / approval)
    waitingLayout->addStretch();
    this->_waiting = new QLabel("Connecting...");
    this->_waiting->setObjectName("title");
    this->_waiting->setAlignment(Qt::AlignCenter);
    waitingLayout->addWidget(this->_waiting);
    waitingLayout->addWidget(cancel, 0, Qt::AlignCenter);
    waitingLayout->addStretch();
    this->_pages->addWidget(waiting);
    this->_pages->addWidget(this->buildViewer_());

    // Closed (kicked, banned, denied)
    closedLayout->addStretch();
    this->_closedTitle = new QLabel();
    this->_closedTitle->setObjectName("title");
    this->_closedTitle->setAlignment(Qt::AlignCenter);
    this->_closedText = new QLabel();
    this->_closedText->setObjectName("muted");
    this->_closedText->setAlignment(Qt::AlignCenter);
    this->_closedText->setWordWrap(true);
    closedLayout->addWidget(this->_closedTitle);
    closedLayout->addWidget(this->_closedText);
    back->setProperty("kind", "primary");
    closedLayout->addWidget(back, 0, Qt::AlignCenter);
    closedLayout->addStretch();
    this->_pages->addWidget(closed);
    this->setCentralWidget(this->_pages);
    QObject::connect(back, &QPushButton::clicked, this, [this]() {this->_pages->setCurrentIndex(0); this->loadRecent_();});
    QObject::connect(cancel, &QPushButton::clicked, this, [this]() {this->_client.disconnectFromHost(); this->_pages->setCurrentIndex(0);});

    // Client
    QObject::connect(&this->_client, &client::Client::stateChanged, this, &ViewerWindow::onState_);
    QObject::connect(&this->_client, &client::Client::fingerprintCheck, this, [this](const QString& fingerprint, const bool changed) {
        QMessageBox box(this);
        box.setWindowTitle(changed ? "Host identity CHANGED" : "New host");
        box.setIcon(changed ? QMessageBox::Critical : QMessageBox::Question);
        box.setText(changed ? "The identity of this host is not the one seen before. Someone may be intercepting the connection." : "First connection to this host. Check that the host panel shows the same identity:");
        box.setInformativeText("<pre>" + fingerprint.left(23) + "</pre><small>" + fingerprint + "</small>");
        box.addButton(changed ? "Connect anyway" : "It matches, connect", QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        this->_client.acceptFingerprint(box.exec() == 0 && box.clickedButton() != box.button(QMessageBox::Cancel), true);
    });
    QObject::connect(&this->_client, &client::Client::message, this, [this](const QString& text) {this->_waiting->setText(text); this->statusBar()->showMessage(text, 5000);});
    QObject::connect(&this->_client, &client::Client::closed, this, [this](const QString& reason, const bool banned) {
        this->_closedTitle->setText(banned ? "Banned by the host" : "Disconnected");
        this->_closedText->setText(reason + (banned ? "\nOnly the person in front of the host can unban this device." : ""));
        this->_pages->setCurrentIndex(3);
        for (TerminalWidget* t: this->_terminals) {
            this->_tabs->removeTab(this->_tabs->indexOf(t));
            t->deleteLater();
        }
        this->_terminals.clear();
        if (this->isFullScreen()) this->showNormal();
    });
    QObject::connect(&this->_client, &client::Client::latencyChanged, this, [this](const int ms) {this->_latency->setText(QString::number(ms) + " ms");});
    QObject::connect(&this->_client, &client::Client::permissionsChanged, this, [this](const QJsonObject& perms) {
        const bool control = perms.value("control").toBool() && !this->_client.options().viewOnly;
        this->_controlAction->setEnabled(control);
        if (!control) this->_controlAction->setChecked(false);
        else if (!this->_controlAction->isChecked() && this->_client.state() == client::State::Active) this->_controlAction->setChecked(true);
        this->_screen->setControl(this->_controlAction->isChecked());
    });
    QObject::connect(&this->_client, &client::Client::terminalOutput, this, [this](const std::uint8_t id, const QByteArray& data) {
        if (this->_terminals.contains(id)) this->_terminals.value(id)->feed(data);
    });
    QObject::connect(&this->_client, &client::Client::terminalClosed, this, [this](const std::uint8_t id, const int code) {
        TerminalWidget* t = this->_terminals.value(id);
        if (t) t->feed(QString("\r\n\x1b[2m[terminal closed, code %1]\x1b[0m\r\n").arg(code).toUtf8());
    });
    this->loadRecent_();
}

QWidget* xdesk::ui::gui::ViewerWindow::buildConnect_(void)
{
    QWidget* page = new QWidget();
    QHBoxLayout* outer = new QHBoxLayout(page);
    QFrame* card = new QFrame();
    QVBoxLayout* layout = new QVBoxLayout(card);
    QFormLayout* form = new QFormLayout();
    QLabel* logo = new QLabel();
    QLabel* title = new QLabel("Connect to a PC");
    QLabel* hint = new QLabel("Ask the person in front of the PC for its address and its code (xdesk panel, Overview).");
    QHBoxLayout* foundHead = new QHBoxLayout();
    QPushButton* scan = new QPushButton(Theme::icon("scan"), "Scan the LAN");

    card->setObjectName("card");
    card->setMaximumWidth(520);
    layout->setContentsMargins(30, 26, 30, 26);
    layout->setSpacing(12);
    logo->setPixmap(Theme::logo(44));
    title->setObjectName("title");
    hint->setObjectName("muted");
    hint->setWordWrap(true);
    layout->addWidget(logo);
    layout->addWidget(title);
    layout->addWidget(hint);
    this->_host = new QLineEdit();
    this->_host->setPlaceholderText("192.168.1.20, my-pc.tailnet.ts.net, bore.pub...");
    this->_port = new QSpinBox();
    this->_port->setRange(1, 65535);
    this->_port->setValue(XDESK_DEFAULT_PORT);
    this->_code = new QLineEdit();
    this->_code->setPlaceholderText("K7F-29Q-XM4 (empty for a trusted device)");
    this->_code->setObjectName("codeInput");
    this->_remember = new QCheckBox("Remember this device (the host decides to trust it)");
    this->_remember->setChecked(true);
    this->_viewOnly = new QCheckBox("View only (never send keyboard / mouse)");
    form->addRow("Address", this->_host);
    form->addRow("Port", this->_port);
    form->addRow("Code", this->_code);
    layout->addLayout(form);
    layout->addWidget(this->_remember);
    layout->addWidget(this->_viewOnly);
    this->_connectError = new QLabel();
    this->_connectError->setProperty("tone", "danger");
    this->_connectError->setWordWrap(true);
    layout->addWidget(this->_connectError);
    this->_connect = new QPushButton(Theme::icon("screen", Theme::palette().accentText), "Connect");
    this->_connect->setProperty("kind", "primary");
    layout->addWidget(this->_connect);
    foundHead->addWidget(new QLabel("Hosts"));
    foundHead->addStretch();
    foundHead->addWidget(scan);
    layout->addLayout(foundHead);
    this->_found = new QListWidget();
    this->_found->setMaximumHeight(150);
    layout->addWidget(this->_found);
    QVBoxLayout* column = new QVBoxLayout();
    column->addStretch();
    column->addWidget(card);
    column->addStretch(2);
    outer->addStretch();
    outer->addLayout(column);
    outer->addStretch();
    QObject::connect(this->_connect, &QPushButton::clicked, this, &ViewerWindow::startConnection_);
    QObject::connect(this->_code, &QLineEdit::returnPressed, this, &ViewerWindow::startConnection_);
    QObject::connect(scan, &QPushButton::clicked, this, &ViewerWindow::scan_);
    QObject::connect(this->_found, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        this->_host->setText(item->data(Qt::UserRole).toString());
        this->_port->setValue(item->data(Qt::UserRole + 1).toInt());
        this->_code->setFocus();
    });
    return page;
}

void xdesk::ui::gui::ViewerWindow::loadRecent_(void)
{
    QFile file(client::Client::knownHostsPath());
    const QJsonObject hosts = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();

    this->_found->clear();
    for (QJsonObject::const_iterator it = hosts.constBegin(); it != hosts.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        QListWidgetItem* item = new QListWidgetItem(Theme::icon(entry.contains("secret") ? "shield" : "screen"), entry.value("name").toString(it.key()) + "   " + it.key() + (entry.contains("secret") ? "   (trusted device)" : ""));
        item->setData(Qt::UserRole, it.key().section(':', 0, -2));
        item->setData(Qt::UserRole + 1, it.key().section(':', -1).toInt());
        this->_found->addItem(item);
    }
}

void xdesk::ui::gui::ViewerWindow::scan_(void)
{
    QPointer<ViewerWindow> self = this;
    QThread* thread = QThread::create([self]() {
        const QList<net::Discovered> found = net::Exposure::scan(2500);

        // Back to the ui thread
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, found]() {
            if (!self) return;
            for (const net::Discovered& d: found) {
                QListWidgetItem* item = new QListWidgetItem(Theme::icon("scan"), d.name + "   " + d.host + ":" + QString::number(d.port) + "   id " + d.fingerprint);
                item->setData(Qt::UserRole, d.host);
                item->setData(Qt::UserRole + 1, d.port);
                self->_found->insertItem(0, item);
            }
            self->statusBar()->showMessage(QString::number(found.size()) + " host(s) found on the LAN", 4000);
        }, Qt::QueuedConnection);
    });

    this->statusBar()->showMessage("Scanning the LAN...", 3000);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QWidget* xdesk::ui::gui::ViewerWindow::buildViewer_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QAction* fit = nullptr;
    QAction* fullscreen = nullptr;
    QLabel* qualityLabel = new QLabel("  Quality ");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    this->_toolbar = new QToolBar();
    this->_toolbar->setIconSize(QSize(18, 18));
    this->_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    this->_controlAction = this->_toolbar->addAction(Theme::icon("control"), "Control");
    this->_controlAction->setCheckable(true);
    fit = this->_toolbar->addAction(Theme::icon("fit"), "Fit");
    fit->setCheckable(true);
    fit->setChecked(true);
    fullscreen = this->_toolbar->addAction(Theme::icon("fullscreen"), "Full screen");
    this->_toolbar->addAction(Theme::icon("keyboard"), "Ctrl+Alt+Del", this, [this]() {
        for (const QString& code: {"ControlLeft", "AltLeft", "Delete"})
            this->_client.key(code, true);
        for (const QString& code: {"Delete", "AltLeft", "ControlLeft"})
            this->_client.key(code, false);
    });
    this->_toolbar->addAction(Theme::icon("terminal"), "Terminal", this, &ViewerWindow::openTerminal_);
    this->_toolbar->addWidget(qualityLabel);
    this->_quality = new QSlider(Qt::Horizontal);
    this->_quality->setRange(20, 95);
    this->_quality->setValue(70);
    this->_quality->setMaximumWidth(140);
    this->_toolbar->addWidget(this->_quality);
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    this->_toolbar->addWidget(spacer);
    this->_latency = new QLabel("- ms");
    this->_latency->setObjectName("badge");
    this->_toolbar->addWidget(this->_latency);
    this->_toolbar->addAction(Theme::icon("power"), "Disconnect", this, [this]() {
        this->_client.disconnectFromHost();
        this->_closedTitle->setText("Disconnected");
        this->_closedText->setText("You left the session.");
        this->_pages->setCurrentIndex(3);
    });
    layout->addWidget(this->_toolbar);
    this->_tabs = new QTabWidget();
    this->_tabs->setDocumentMode(true);
    this->_tabs->setTabsClosable(true);
    this->_screen = new ScreenView(this->_client);
    this->_tabs->addTab(this->_screen, Theme::icon("screen"), "Screen");
    this->_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    layout->addWidget(this->_tabs, 1);
    QObject::connect(this->_controlAction, &QAction::toggled, this, [this](const bool on) {
        this->_screen->setControl(on);
        this->_client.requestMode(on ? "control" : "view");
        this->_screen->setFocus();
    });
    QObject::connect(fit, &QAction::toggled, this->_screen, &ScreenView::setFit);
    QObject::connect(fullscreen, &QAction::triggered, this, [this]() {
        if (this->isFullScreen()) this->showNormal();
        else this->showFullScreen();
        this->_screen->setFocus();
    });
    QObject::connect(this->_quality, &QSlider::sliderReleased, this, [this]() {this->_client.setQuality(this->_quality->value(), 1.0, 60);});
    QObject::connect(this->_tabs, &QTabWidget::tabCloseRequested, this, [this](const int index) {
        for (QHash<std::uint8_t, TerminalWidget*>::iterator it = this->_terminals.begin(); it != this->_terminals.end(); ++it) {
            if (this->_tabs->widget(index) != it.value()) continue;
            this->_client.closeTerminal(it.key());
            this->_tabs->removeTab(index);
            it.value()->deleteLater();
            this->_terminals.erase(it);
            return;
        }
    });
    return page;
}

void xdesk::ui::gui::ViewerWindow::openTerminal_(void)
{
    TerminalWidget* terminal = new TerminalWidget();
    std::uint8_t id = 0;

    if (!this->_client.permissions().value("terminal").toBool()) {
        QMessageBox::information(this, "Terminal", "The host did not allow terminals for this session.");
        delete terminal;
        return;
    }
    id = this->_client.openTerminal(80, 24);
    this->_terminals.insert(id, terminal);
    this->_tabs->addTab(terminal, Theme::icon("terminal"), "Terminal " + QString::number(id));
    this->_tabs->setCurrentWidget(terminal);
    QObject::connect(terminal, &TerminalWidget::input, this, [this, id](const QByteArray& data) {this->_client.terminalInput(id, data);});
    QObject::connect(terminal, &TerminalWidget::sizeChanged, this, [this, id](const int cols, const int rows) {this->_client.resizeTerminal(id, cols, rows);});
    QObject::connect(terminal, &TerminalWidget::titleChanged, this, [this, terminal](const QString& title) {this->_tabs->setTabText(this->_tabs->indexOf(terminal), title.left(30));});
    terminal->setFocus();
}

void xdesk::ui::gui::ViewerWindow::startConnection_(void)
{
    client::Options options;

    options.host = this->_host->text().trimmed();
    options.port = static_cast<quint16>(this->_port->value());
    options.code = this->_code->text().trimmed();
    options.remember = this->_remember->isChecked();
    options.viewOnly = this->_viewOnly->isChecked();
    options.kind = net::ClientKind::Gui;
    if (options.host.isEmpty()) {this->_connectError->setText("Type the address of the host."); return;}
    this->_connectError->clear();
    this->connectNow(options);
}

void xdesk::ui::gui::ViewerWindow::connectNow(const client::Options& options)
{
    this->_host->setText(options.host);
    this->_port->setValue(options.port);
    this->_viewOnly->setChecked(options.viewOnly);
    this->_waiting->setText("Connecting to " + options.host + "...");
    this->_pages->setCurrentIndex(1);
    this->_client.connectToHost(options);
}

void xdesk::ui::gui::ViewerWindow::onState_(const client::State state)
{
    switch (state) {
        case client::State::Connecting:     this->_waiting->setText("Connecting (TLS 1.3, post-quantum key exchange)..."); this->_pages->setCurrentIndex(1); break;
        case client::State::Verifying:      this->_waiting->setText("Checking the identity of the host..."); break;
        case client::State::Authenticating: this->_waiting->setText("Authenticating (Argon2id proof of the code)..."); break;
        case client::State::Pending:        this->_waiting->setText("Waiting for the person in front of the PC to accept..."); break;
        case client::State::Active:
            this->setWindowTitle("xdesk - " + this->_client.hostName());
            this->_pages->setCurrentIndex(2);
            this->_screen->setFocus();
            break;
        default: break;
    }
}
