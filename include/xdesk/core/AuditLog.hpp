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
##  @file AuditLog.hpp

File Description:
##  Audit log of the host: every connection, decision and input, jsonl per day + live feed
\**************************************************************/

#ifndef AUDITLOG_H
    #define AUDITLOG_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QJsonObject>      // QJsonObject
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <QFile>            // QFile
    #include <QList>            // QList
    #include <mutex>            // std::mutex

namespace xdesk::core { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * One entry = {"ts": ms, "level": "info|warn|alert", "cat": "<category>", "session": "<id>", ...data}
 * Categories: server, auth, session, input, terminal, tamper, settings, device
*/
class AuditLog: public QObject {
    Q_OBJECT

    private:
        QString _dir; // folder of the jsonl files
        QString _day; // yyyy-MM-dd of the open file
        QFile _file; // audit-<day>.jsonl
        QList<QJsonObject> _recent; // last entries kept in memory for the UIs
        mutable std::mutex _lock; // written from the capture / terminal threads too

    public:
        // ---------- Pre-Function -------- //
        _hot void write(const QString& level, const QString& category, const QString& session, const QString& message, const QJsonObject& data = {}); // append + emit entry
        _cold _nodiscard QList<QJsonObject> recent(const int count = 500) const; // last entries (oldest first)
        _cold _nodiscard QString currentFile(void) const; // path of the jsonl of today

        // ------------ Function ---------- //
        /* shortcut */
        _hot inline void info(const QString& cat, const QString& session, const QString& msg, const QJsonObject& data = {})  {this->write("info", cat, session, msg, data);};
        _hot inline void warn(const QString& cat, const QString& session, const QString& msg, const QJsonObject& data = {})  {this->write("warn", cat, session, msg, data);};
        _hot inline void alert(const QString& cat, const QString& session, const QString& msg, const QJsonObject& data = {}) {this->write("alert", cat, session, msg, data);};

        // ---------- Constructor --------- //
        explicit AuditLog(const QString& dir, QObject* parent = nullptr); // dir created (0700)

        // ----------- Destructor --------- //
        ~AuditLog() override = default;

    signals:
        void entry(const QJsonObject& entry); // every written entry (queued to the UI thread)
};

} // namespace end
#endif /* AUDITLOG_H */
