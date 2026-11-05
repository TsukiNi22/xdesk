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
##  @file QtCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/QtCapture.hpp"
#include <QGuiApplication>
#include <QMetaObject>
#include <QSemaphore>
#include <QPixmap>
#include <QScreen>
#include <QCursor>
#include <QThread>
#include <memory>

//----------------------------------------------------------------//
/* DEFINE */

#define GRAB_TIMEOUT 500 // ms waited for the GUI thread

//----------------------------------------------------------------//
/* STRUCT */

// Shared between the capture thread and the GUI thread (kept alive by both until the last one is done)
struct GrabJob {
    QSemaphore done;
    QImage image;
    QPoint cursor = {-1, -1};
};

//----------------------------------------------------------------//
/* TOOLS */

// GUI thread: screen + cursor in frame pixels
static void grab_now(GrabJob& job)
{
    QScreen* screen = QGuiApplication::primaryScreen();

    if (!screen) return;
    const QPixmap pixmap = screen->grabWindow(0);
    const QRect geometry = screen->geometry();
    const qreal ratio = pixmap.devicePixelRatio() > 0 ? pixmap.devicePixelRatio() : screen->devicePixelRatio();
    const QPoint position = QCursor::pos(screen) - geometry.topLeft();

    job.image = pixmap.toImage();
    if (job.image.format() != QImage::Format_RGB32 && job.image.format() != QImage::Format_ARGB32)
        job.image = job.image.convertToFormat(QImage::Format_RGB32);
    if (geometry.contains(QCursor::pos(screen)))
        job.cursor = QPoint(static_cast<int>(position.x() * ratio), static_cast<int>(position.y() * ratio));
}

//----------------------------------------------------------------//
/* CLASS */

bool xdesk::platform::QtCapture::start(void)
{
    QImage first;

    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        this->_error = "no graphical application (QGuiApplication) to grab the screen";
        return false;
    }
    this->_started = true;
    first = this->grab();
    if (first.isNull() || first.width() <= 0) {
        this->_error = this->_error.isEmpty() ? "QScreen::grabWindow returned nothing (Wayland?)" : this->_error;
        this->_started = false;
        return false;
    }
    return true;
}

void xdesk::platform::QtCapture::stop(void)
{
    this->_started = false;
}

QImage xdesk::platform::QtCapture::grab(void)
{
    QCoreApplication* app = QCoreApplication::instance();
    std::shared_ptr<GrabJob> job = std::make_shared<GrabJob>();

    if (!this->_started || !qobject_cast<QGuiApplication*>(app)) return {};
    if (QThread::currentThread() == app->thread()) {
        grab_now(*job);
    } else {
        QMetaObject::invokeMethod(app, [job](void) {grab_now(*job); job->done.release();}, Qt::QueuedConnection);
        if (!job->done.tryAcquire(1, GRAB_TIMEOUT)) {
            this->_error = "GUI thread busy: frame skipped";
            return {};
        }
    }
    if (job->image.isNull()) return {};
    this->_size = job->image.size();
    this->_cursor = job->cursor;
    return job->image;
}
