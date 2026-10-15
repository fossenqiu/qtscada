#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct TelemetryRecord {
    QDateTime timestamp;
    bool motorRunning = false;
    double currentA = 0.0;
    double speedRpm = 0.0;
    bool plcConnected = false;
};

struct HistoryEvent {
    qint64 id = 0;
    QDateTime timestamp;
    QString level;
    QString category;
    QString message;
};

class HistoryStore {
public:
    HistoryStore();
    ~HistoryStore();

    HistoryStore(const HistoryStore&) = delete;
    HistoryStore& operator=(const HistoryStore&) = delete;

    bool open();
    bool isOpen() const;

    QString databasePath() const;
    QString lastError() const;

    bool appendTelemetry(const TelemetryRecord& record);

    bool appendEvent(
        const QString& level,
        const QString& category,
        const QString& message
        );

    QVector<TelemetryRecord> queryTelemetry(
        const QDateTime& from,
        const QDateTime& to,
        int limit = 1000
        );

    QVector<HistoryEvent> queryEvents(
        const QDateTime& from,
        const QDateTime& to,
        const QString& level = QString(),
        int limit = 1000
        );

private:
    bool initializeSchema();
    bool executeSql(const QString& sql);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_db;
};

#endif // HISTORYSTORE_H
