#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QVector>
#include <QTimer>
#include "HistoryStore.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QComboBox;
class QFrame;
class IPlcClient;
class MotorWidget;

struct EventRow {
    QDateTime time;
    QString level;
    QString source;
    QString message;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void startMotor();
    void stopMotor();
    void updateMotor(bool running);
    void updateConnection(bool connected);
    void renderEvents();

private:
    void setupUi();
    QFrame *createMetricCard(const QString &title, QLabel **valueLabel,
                             const QString &unit = QString());
    void addEvent(const QString &level, const QString &source,
                  const QString &message);
    void updateMetrics(bool running);
    QString levelLabel(const QString &level) const;
    void exportCsv();

    IPlcClient *m_plc = nullptr;
    QLabel *m_connectionChip = nullptr;
    QLabel *m_modeChip = nullptr;
    QLabel *m_motorChip = nullptr;
    QLabel *m_motorValue = nullptr;
    QLabel *m_currentValue = nullptr;
    QLabel *m_speedValue = nullptr;
    QLabel *m_countValue = nullptr;
    QLabel *m_runTag = nullptr;
    QLabel *m_modeValue = nullptr;
    QLabel *m_interlockValue = nullptr;
    QLabel *m_lastActionValue = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QTableWidget *m_eventTable = nullptr;
    QComboBox *m_filter = nullptr;
    QLabel *m_eventCount = nullptr;
    MotorWidget *m_motorWidget = nullptr;

    QVector<EventRow> m_events;
    int m_startCount = 0;
    bool m_running = false;

    // SQLite 历史数据存储
    HistoryStore m_historyStore;

    // 周期采样定时器
    QTimer *m_historyTimer = nullptr;

    // 历史查询窗口
    void showHistoryDialog();

    // 每秒采集并保存一次数据
    void saveTelemetrySnapshot();
};

#endif // MAINWINDOW_H
