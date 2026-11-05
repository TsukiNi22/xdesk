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
##  @file AuditLog.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/core/AuditLog.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QString>
#include <QFile>
#include <mutex>
#include <QDir>

xdesk::core::AuditLog::AuditLog(const QString& dir, QObject* parent)
: QObject(parent), _dir{dir}
{
    QDir().mkpath(dir);
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

void xdesk::core::AuditLog::write(const QString& level, const QString& category, const QString& session, const QString& message, const QJsonObject& data)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString day = now.toString("yyyy-MM-dd");
    QJsonObject entry = data;

    entry["ts"] = now.toMSecsSinceEpoch();
    entry["level"] = level;
    entry["cat"] = category;
    entry["session"] = session;
    entry["msg"] = message;
    {
        std::lock_guard lock(this->_lock);

        // One file per day, append only
        if (day != this->_day || !this->_file.isOpen()) {
            this->_file.close();
            this->_day = day;
            this->_file.setFileName(this->_dir + "/audit-" + day + ".jsonl");
            if (this->_file.open(QIODevice::Append | QIODevice::Text))
                this->_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        }
        if (this->_file.isOpen()) {
            this->_file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact));
            this->_file.write("\n");
            this->_file.flush();
        }
        this->_recent.append(entry);
        if (this->_recent.size() > 2000) this->_recent.remove(0, this->_recent.size() - 2000);
    }
    emit this->entry(entry);
}

QList<QJsonObject> xdesk::core::AuditLog::recent(const int count) const
{
    std::lock_guard lock(this->_lock);
    const qsizetype size = this->_recent.size();

    return this->_recent.mid(std::max<qsizetype>(0, size - count));
}

QString xdesk::core::AuditLog::currentFile(void) const
{
    return this->_dir + "/audit-" + QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".jsonl";
}
