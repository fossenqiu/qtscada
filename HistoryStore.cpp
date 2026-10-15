
#include "HistoryStore.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>
#include <QTimeZone>

HistoryStore::HistoryStore()
{
    m_connectionName =
        "scada_history_" +
        QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QString dataDir =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation
            );

    m_databasePath =
        QDir(dataDir).filePath("scada_history.sqlite");
}

HistoryStore::~HistoryStore()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool HistoryStore::open()
{
    if (m_db.isOpen()) {
        return true;
    }

    const QFileInfo info(m_databasePath);

    if (!QDir().mkpath(info.absolutePath())) {
        m_lastError = "无法创建数据库目录：" + info.absolutePath();
        return false;
    }

    m_db = QSqlDatabase::addDatabase(
        "QSQLITE",
        m_connectionName
        );

    m_db.setDatabaseName(m_databasePath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    if (!executeSql("PRAGMA busy_timeout = 3000;")) {
        return false;
    }

    if (!executeSql("PRAGMA foreign_keys = ON;")) {
        return false;
    }

    return initializeSchema();
}

bool HistoryStore::isOpen() const
{
    return m_db.isOpen();
}

QString HistoryStore::databasePath() const
{
    return m_databasePath;
}

QString HistoryStore::lastError() const
{
    return m_lastError;
}

bool HistoryStore::executeSql(const QString& sql)
{
    QSqlQuery query(m_db);

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool HistoryStore::initializeSchema()
{
    if (!executeSql(R"(
        CREATE TABLE IF NOT EXISTS telemetry_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            motor_running INTEGER NOT NULL,
            current_a REAL NOT NULL,
            speed_rpm REAL NOT NULL,
            plc_connected INTEGER NOT NULL
        );
    )")) {
        return false;
    }

    if (!executeSql(R"(
        CREATE INDEX IF NOT EXISTS idx_telemetry_timestamp
        ON telemetry_history(timestamp);
    )")) {
        return false;
    }

    if (!executeSql(R"(
        CREATE TABLE IF NOT EXISTS event_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            level TEXT NOT NULL,
            category TEXT NOT NULL,
            message TEXT NOT NULL
        );
    )")) {
        return false;
    }

    if (!executeSql(R"(
        CREATE INDEX IF NOT EXISTS idx_event_timestamp
        ON event_history(timestamp);
    )")) {
        return false;
    }

    if (!executeSql(R"(
        CREATE INDEX IF NOT EXISTS idx_event_level_timestamp
        ON event_history(level, timestamp);
    )")) {
        return false;
    }

    if (!executeSql(R"(
        CREATE TABLE IF NOT EXISTS app_meta (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )")) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR IGNORE INTO app_meta(key, value)
        VALUES('schema_version', '1');
    )");

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool HistoryStore::appendTelemetry(
    const TelemetryRecord& record)
{
    if (!m_db.isOpen()) {
        m_lastError = "数据库尚未打开";
        return false;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        INSERT INTO telemetry_history (
            timestamp,
            motor_running,
            current_a,
            speed_rpm,
            plc_connected
        )
        VALUES (?, ?, ?, ?, ?);
    )");

    query.addBindValue(
        record.timestamp.toUTC().toString(Qt::ISODateWithMs)
        );
    query.addBindValue(record.motorRunning ? 1 : 0);
    query.addBindValue(record.currentA);
    query.addBindValue(record.speedRpm);
    query.addBindValue(record.plcConnected ? 1 : 0);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool HistoryStore::appendEvent(
    const QString& level,
    const QString& category,
    const QString& message)
{
    if (!m_db.isOpen()) {
        m_lastError = "数据库尚未打开";
        return false;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        INSERT INTO event_history (
            timestamp,
            level,
            category,
            message
        )
        VALUES (?, ?, ?, ?);
    )");

    query.addBindValue(
        QDateTime::currentDateTimeUtc()
            .toString(Qt::ISODateWithMs)
        );
    query.addBindValue(level);
    query.addBindValue(category);
    query.addBindValue(message);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

QVector<TelemetryRecord> HistoryStore::queryTelemetry(
    const QDateTime& from,
    const QDateTime& to,
    int limit)
{
    QVector<TelemetryRecord> records;

    if (!m_db.isOpen() || limit <= 0) {
        return records;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        SELECT timestamp, motor_running, current_a,
               speed_rpm, plc_connected
        FROM telemetry_history
        WHERE timestamp >= ? AND timestamp <= ?
        ORDER BY timestamp DESC
        LIMIT ?;
    )");

    query.addBindValue(
        from.toUTC().toString(Qt::ISODateWithMs)
        );
    query.addBindValue(
        to.toUTC().toString(Qt::ISODateWithMs)
        );
    query.addBindValue(qMin(limit, 100000));

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return records;
    }

    while (query.next()) {
        TelemetryRecord record;

        record.timestamp = QDateTime::fromString(
            query.value(0).toString(),
            Qt::ISODateWithMs
            );

        record.timestamp.setTimeZone(QTimeZone::utc());
        record.motorRunning = query.value(1).toInt() != 0;
        record.currentA = query.value(2).toDouble();
        record.speedRpm = query.value(3).toDouble();
        record.plcConnected = query.value(4).toInt() != 0;

        records.append(record);
    }

    return records;
}

QVector<HistoryEvent> HistoryStore::queryEvents(
    const QDateTime& from,
    const QDateTime& to,
    const QString& level,
    int limit)
{
    QVector<HistoryEvent> records;

    if (!m_db.isOpen() || limit <= 0) {
        return records;
    }

    QSqlQuery query(m_db);

    QString sql = R"(
        SELECT id, timestamp, level, category, message
        FROM event_history
        WHERE timestamp >= ? AND timestamp <= ?
    )";

    if (!level.isEmpty()) {
        sql += " AND level = ?";
    }

    sql += " ORDER BY timestamp DESC LIMIT ?";

    query.prepare(sql);

    query.addBindValue(
        from.toUTC().toString(Qt::ISODateWithMs)
        );
    query.addBindValue(
        to.toUTC().toString(Qt::ISODateWithMs)
        );

    if (!level.isEmpty()) {
        query.addBindValue(level);
    }

    query.addBindValue(qMin(limit, 100000));

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return records;
    }

    while (query.next()) {
        HistoryEvent event;

        event.id = query.value(0).toLongLong();
        event.timestamp = QDateTime::fromString(
            query.value(1).toString(),
            Qt::ISODateWithMs
            );
        event.timestamp.setTimeZone(QTimeZone::utc());
        event.level = query.value(2).toString();
        event.category = query.value(3).toString();
        event.message = query.value(4).toString();

        records.append(event);
    }

    return records;
}
