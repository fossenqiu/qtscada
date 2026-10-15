#include "mainwindow.h"

#include "MockPlcClient.h"
#include "IPlcClient.h"
#include "MotorWidget.h"
#include "HistoryStore.h"

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QStatusBar>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QTimer>
#include <QDebug>
#include <QDialog>
#include <QTabWidget>
#include <QTableWidgetItem>

namespace {
const char *kStyle = R"(
QMainWindow { background: #F4F6F8; }
QWidget { color: #0F172A; font-family: "Microsoft YaHei", "Noto Sans CJK SC", sans-serif; font-size: 13px; }
QFrame#appHeader, QFrame#card, QFrame#metricCard, QFrame#eventsPanel {
    background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 10px;
}
QLabel#appTitle { font-size: 17px; font-weight: 600; }
QLabel#subTitle { color: #64748B; font-size: 11px; }
QLabel#sectionTitle { font-size: 13px; font-weight: 600; }
QLabel#metricCaption { color: #64748B; font-size: 12px; }
QLabel#metricValue { font-size: 21px; font-weight: 600; }
QLabel#chip { padding: 5px 10px; border-radius: 14px; background: #F8FAFC; border: 1px solid #E2E8F0; color: #475569; }
QLabel#chipOk { padding: 5px 10px; border-radius: 14px; background: #E8F6EC; border: 1px solid #BFE3CB; color: #14532D; }
QLabel#chipBlue { padding: 5px 10px; border-radius: 14px; background: #E8F1FE; border: 1px solid #C6DBFB; color: #1E40AF; }
QPushButton { min-height: 34px; padding: 4px 12px; border-radius: 7px; border: 1px solid #CBD5E1; background: white; }
QPushButton:hover { border-color: #94A3B8; }
QPushButton:disabled { background: #F8FAFC; color: #94A3B8; border-color: #E2E8F0; }
QPushButton#startButton { color: white; background: #16A34A; border-color: #15803D; font-size: 14px; font-weight: 600; min-height: 54px; }
QPushButton#startButton:hover { background: #15803D; }
QPushButton#stopButton { color: white; background: #DC2626; border-color: #B91C1C; font-size: 14px; font-weight: 600; min-height: 54px; }
QPushButton#stopButton:hover { background: #B91C1C; }
QComboBox { min-height: 28px; padding: 2px 8px; background: white; border: 1px solid #CBD5E1; border-radius: 6px; }
QTableWidget { background: white; border: 1px solid #E2E8F0; border-radius: 7px; gridline-color: #E2E8F0; }
QHeaderView::section { background: #F8FAFC; color: #64748B; border: none; border-bottom: 1px solid #E2E8F0; padding: 8px; }
QTableWidget::item { padding: 5px; }
QStatusBar { background: #FFFFFF; color: #64748B; border-top: 1px solid #E2E8F0; }
)";
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setStyleSheet(QString::fromUtf8(kStyle));
    setupUi();

    m_plc = new MockPlcClient(this);
    connect(m_plc, &IPlcClient::connectionChanged,
            this, &MainWindow::updateConnection);
    connect(m_plc, &IPlcClient::tagUpdated, this,
            [this](const QString &tag, const QVariant &value) {
                if (tag == "motor")
                    updateMotor(value.toBool());
            });
    connect(m_plc, &IPlcClient::communicationError, this,
            [this](const QString &message) {
                addEvent("err", "PLC", message);
            });

    m_plc->connectToPlc();
    addEvent("info", "PLC", "模拟 PLC 已连接；当前为模拟模式");
    statusBar()->showMessage("就绪 · 当前数据来自模拟 PLC");

    // SQLite 数据库在 setupUi() 中已经尝试初始化。
    // 只有初始化成功时才启动采样。
    if (m_historyStore.isOpen()) {
        m_historyTimer = new QTimer(this);

        connect(m_historyTimer, &QTimer::timeout,
                this, &MainWindow::saveTelemetrySnapshot);

        // 启动后先保存一条，再每秒采样一次。
        saveTelemetrySnapshot();
        m_historyTimer->start(1000);

        statusBar()->showMessage(
            "就绪 · 模拟 PLC · SQLite 历史记录已启用"
            );
    } else {
        qWarning() << "SQLite 历史记录未启用："
                   << m_historyStore.lastError();

        statusBar()->showMessage(
            "就绪 · 模拟 PLC · 历史数据库不可用"
            );
    }
}

QFrame *MainWindow::createMetricCard(const QString &title, QLabel **valueLabel,
                                     const QString &unit)
{
    auto *card = new QFrame;
    card->setObjectName("metricCard");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 11);
    layout->setSpacing(3);

    auto *caption = new QLabel(title);
    caption->setObjectName("metricCaption");
    auto *line = new QHBoxLayout;
    *valueLabel = new QLabel("--");
    (*valueLabel)->setObjectName("metricValue");
    line->addWidget(*valueLabel);
    if (!unit.isEmpty()) {
        auto *u = new QLabel(unit);
        u->setStyleSheet("color:#64748B; padding-top:7px;");
        line->addWidget(u);
    }
    line->addStretch();
    layout->addWidget(caption);
    layout->addLayout(line);
    return card;
}

void MainWindow::setupUi()
{
    setWindowTitle("工业设备监控系统 · 电机启停实验");
    resize(1180, 820);
    setMinimumSize(900, 650);

    auto *central = new QWidget;
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 16, 18, 12);
    root->setSpacing(12);

    // Header
    auto *header = new QFrame;
    header->setObjectName("appHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    headerLayout->setSpacing(12);

    auto *logo = new QLabel("SCADA");
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(58, 42);
    logo->setStyleSheet("background:#1F6FEB;color:white;border-radius:8px;font-size:13px;font-weight:700;");

    auto *titles = new QVBoxLayout;
    auto *title = new QLabel("工业设备监控系统  /  电机启停实验");
    title->setObjectName("appTitle");
    auto *subtitle = new QLabel("PLC 仿真实验站  ·  起保停回路  ·  当前为软件模拟模式");
    subtitle->setObjectName("subTitle");
    titles->addWidget(title);
    titles->addWidget(subtitle);

    m_connectionChip = new QLabel("● 通信已连接（模拟）");
    m_connectionChip->setObjectName("chipOk");
    m_modeChip = new QLabel("模式：手动");
    m_modeChip->setObjectName("chip");
    m_motorChip = new QLabel("电机：停止");
    m_motorChip->setObjectName("chip");

    headerLayout->addWidget(logo);
    headerLayout->addLayout(titles, 1);
    headerLayout->addWidget(m_connectionChip);
    headerLayout->addWidget(m_modeChip);
    headerLayout->addWidget(m_motorChip);
    root->addWidget(header);

    // Metric cards
    auto *metrics = new QGridLayout;
    metrics->setSpacing(12);
    metrics->addWidget(createMetricCard("电机状态", &m_motorValue), 0, 0);
    metrics->addWidget(createMetricCard("运行电流（模拟值）", &m_currentValue, "A"), 0, 1);
    metrics->addWidget(createMetricCard("转速（模拟值）", &m_speedValue, "rpm"), 0, 2);
    metrics->addWidget(createMetricCard("今日启动次数", &m_countValue, "次"), 0, 3);
    root->addLayout(metrics);

    // Main content: equipment and control panel
    auto *mainGrid = new QGridLayout;
    mainGrid->setSpacing(12);

    auto *deviceCard = new QFrame;
    deviceCard->setObjectName("card");
    auto *deviceLayout = new QVBoxLayout(deviceCard);
    deviceLayout->setContentsMargins(0, 0, 0, 0);
    auto *deviceHeader = new QHBoxLayout;
    deviceHeader->setContentsMargins(15, 12, 15, 10);
    auto *deviceTitle = new QLabel("设备监控");
    deviceTitle->setObjectName("sectionTitle");
    m_runTag = new QLabel("IDLE");
    m_runTag->setStyleSheet("color:#64748B;background:#F8FAFC;border:1px solid #E2E8F0;border-radius:10px;padding:2px 9px;font-size:11px;");
    deviceHeader->addWidget(deviceTitle);
    deviceHeader->addStretch();
    deviceHeader->addWidget(m_runTag);
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color:#E2E8F0;");
    deviceLayout->addLayout(deviceHeader);
    deviceLayout->addWidget(separator);
    auto *stage = new QWidget;
    stage->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #FFFFFF,stop:1 #F8FAFC);");
    auto *stageLayout = new QVBoxLayout(stage);
    stageLayout->setContentsMargins(12, 8, 12, 8);
    m_motorWidget = new MotorWidget;
    stageLayout->addWidget(m_motorWidget, 1);
    auto *hint = new QLabel("控制逻辑：启动自锁 / 停止优先    ·    设备图形为示意图");
    hint->setStyleSheet("color:#94A3B8;font-size:11px;padding:0 4px 5px;");
    stageLayout->addWidget(hint);
    deviceLayout->addWidget(stage, 1);

    auto *controlCard = new QFrame;
    controlCard->setObjectName("card");
    auto *controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(15, 12, 15, 15);
    controlLayout->setSpacing(12);

    auto *controlTitle = new QLabel("控制台");
    controlTitle->setObjectName("sectionTitle");
    auto *lockTag = new QLabel("联锁状态：已投入");
    lockTag->setStyleSheet("color:#14532D;background:#E8F6EC;border-radius:10px;padding:3px 8px;font-size:11px;");
    auto *controlTitleRow = new QHBoxLayout;
    controlTitleRow->addWidget(controlTitle);
    controlTitleRow->addStretch();
    controlTitleRow->addWidget(lockTag);
    controlLayout->addLayout(controlTitleRow);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(10);
    m_startButton = new QPushButton("▶  启动电机");
    m_startButton->setObjectName("startButton");
    m_stopButton = new QPushButton("■  停止电机");
    m_stopButton->setObjectName("stopButton");
    m_stopButton->setEnabled(false);
    buttonRow->addWidget(m_startButton);
    buttonRow->addWidget(m_stopButton);
    controlLayout->addLayout(buttonRow);

    auto addDetailRow = [controlLayout](const QString &name, QLabel **value, const QString &initial) {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(name);
        label->setStyleSheet("color:#64748B;");
        *value = new QLabel(initial);
        (*value)->setStyleSheet("font-weight:600;");
        row->addWidget(label);
        row->addStretch();
        row->addWidget(*value);
        controlLayout->addLayout(row);
        auto *line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("color:#E2E8F0;");
        controlLayout->addWidget(line);
    };
    addDetailRow("输出点", &m_modeValue, "模拟输出 / KM1");
    addDetailRow("故障联锁", &m_interlockValue, "已投入");
    addDetailRow("最近动作", &m_lastActionValue, "无");
    controlLayout->addStretch();

    mainGrid->addWidget(deviceCard, 0, 0);
    mainGrid->addWidget(controlCard, 0, 1);
    mainGrid->setColumnStretch(0, 7);
    mainGrid->setColumnStretch(1, 4);
    mainGrid->setRowStretch(0, 1);
    root->addLayout(mainGrid, 5);

    // Event log panel
    auto *eventsPanel = new QFrame;
    eventsPanel->setObjectName("eventsPanel");
    auto *eventsLayout = new QVBoxLayout(eventsPanel);
    eventsLayout->setContentsMargins(14, 12, 14, 14);
    eventsLayout->setSpacing(9);

    auto *toolbar = new QHBoxLayout;
    auto *eventsTitle = new QLabel("报警与事件记录");
    eventsTitle->setObjectName("sectionTitle");
    m_filter = new QComboBox;
    m_filter->addItem("全部级别", "all");
    m_filter->addItem("事件", "info");
    m_filter->addItem("警告", "warn");
    m_filter->addItem("报警", "err");
    auto *clearButton = new QPushButton("清空记录");
    auto *exportButton = new QPushButton("导出 CSV");
    auto *historyButton = new QPushButton("历史查询");
    m_eventCount = new QLabel("0 条");
    m_eventCount->setStyleSheet("color:#64748B;");
    toolbar->addWidget(eventsTitle);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_filter);
    toolbar->addWidget(clearButton);
    toolbar->addWidget(exportButton);
    toolbar->addStretch();
    toolbar->addWidget(m_eventCount);
    eventsLayout->addLayout(toolbar);
    toolbar->addWidget(historyButton);

    m_eventTable = new QTableWidget(0, 4);
    m_eventTable->setHorizontalHeaderLabels({"时间", "级别", "来源", "事件内容"});
    m_eventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_eventTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_eventTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_eventTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_eventTable->verticalHeader()->setVisible(false);
    m_eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventTable->setAlternatingRowColors(true);
    m_eventTable->setMinimumHeight(165);
    eventsLayout->addWidget(m_eventTable);
    root->addWidget(eventsPanel, 3);

    setCentralWidget(central);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startMotor);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopMotor);
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this]() { renderEvents(); });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_events.clear();
        renderEvents();
        addEvent("warn", "操作员", "事件记录已清空");
    });
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportCsv);
    connect(historyButton, &QPushButton::clicked, this, &MainWindow::showHistoryDialog);

    if (!m_historyStore.open()) {
        qWarning() << "SQLite 初始化失败:"
                   << m_historyStore.lastError();
    } else {
        qInfo() << "SQLite 数据库路径:"
                << m_historyStore.databasePath();

        m_historyStore.appendEvent(
            "info",
            "系统",
            "SCADA 历史数据库初始化成功"
            );
    }
}

void MainWindow::startMotor()
{
    if (!m_plc || !m_plc->isConnected()) {
        addEvent("err", "控制台", "启动失败：PLC 未连接");
        return;
    }
    if (m_running) {
        addEvent("warn", "控制台", "电机已经处于运行状态");
        return;
    }
    if (!m_plc->writeTag("start", true)) {
        addEvent("err", "控制台", "启动命令写入失败");
        return;
    }
    ++m_startCount;
    m_lastActionValue->setText("启动请求");
    addEvent("info", "操作员", "已发送电机启动请求（模拟）");
}

void MainWindow::stopMotor()
{
    if (!m_plc || !m_plc->isConnected()) {
        addEvent("err", "控制台", "停止失败：PLC 未连接");
        return;
    }
    if (!m_running) {
        addEvent("warn", "控制台", "电机已经处于停止状态");
        return;
    }
    if (!m_plc->writeTag("stop", true)) {
        addEvent("err", "控制台", "停止命令写入失败");
        return;
    }
    m_lastActionValue->setText("停止请求");
    addEvent("warn", "操作员", "已发送电机停止请求（模拟）");
}

void MainWindow::updateMotor(bool running)
{
    const bool changed = (m_running != running);

    m_running = running;

    // 每次都刷新界面，不能因状态未变化而跳过初始化。
    m_motorWidget->setRunning(running);

    m_motorValue->setText(running ? "运行" : "停止");
    m_motorValue->setStyleSheet(
        running
            ? "color:#16A34A;font-size:21px;font-weight:600;"
            : "color:#64748B;font-size:21px;font-weight:600;"
        );

    m_motorChip->setText(
        running ? "● 电机：运行" : "● 电机：停止"
        );

    m_motorChip->setObjectName(
        running ? "chipBlue" : "chip"
        );

    m_motorChip->setStyleSheet(
        running
            ? "padding:5px 10px;border-radius:14px;"
              "background:#E8F1FE;border:1px solid #C6DBFB;"
              "color:#1E40AF;"
            : "padding:5px 10px;border-radius:14px;"
              "background:#F8FAFC;border:1px solid #E2E8F0;"
              "color:#475569;"
        );

    m_runTag->setText(running ? "RUNNING" : "IDLE");

    m_runTag->setStyleSheet(
        running
            ? "color:#1E40AF;background:#E8F1FE;"
              "border:1px solid #C6DBFB;border-radius:10px;"
              "padding:2px 9px;font-size:11px;"
            : "color:#64748B;background:#F8FAFC;"
              "border:1px solid #E2E8F0;border-radius:10px;"
              "padding:2px 9px;font-size:11px;"
        );

    const bool connected = m_plc && m_plc->isConnected();

    m_startButton->setEnabled(connected && !running);
    m_stopButton->setEnabled(connected && running);

    updateMetrics(running);

    // 只有状态真正变化时，才生成一条反馈事件。
    if (changed) {
        addEvent(
            running ? "info" : "warn",
            "PLC 状态",
            running
                ? "电机运行反馈已置位"
                : "电机停止反馈已置位"
            );
    }
}

void MainWindow::updateMetrics(bool running)
{
    m_currentValue->setText(running ? "4.20" : "0.00");
    m_speedValue->setText(running ? "1450" : "0");
    m_countValue->setText(QString::number(m_startCount));
}

void MainWindow::saveTelemetrySnapshot()
{
    if (!m_historyStore.isOpen()) {
        return;
    }

    TelemetryRecord record;

    record.timestamp = QDateTime::currentDateTimeUtc();
    record.motorRunning = m_running;

    // 与当前界面 updateMetrics() 使用相同的模拟值。
    record.currentA = m_running ? 4.20 : 0.00;
    record.speedRpm = m_running ? 1450.0 : 0.0;

    // 使用 PLC 客户端的当前连接状态，不写死为 true。
    record.plcConnected = m_plc && m_plc->isConnected();

    if (!m_historyStore.appendTelemetry(record)) {
        qWarning() << "保存 SQLite 采样记录失败："
                   << m_historyStore.lastError();
    }
}

void MainWindow::updateConnection(bool connected)
{
    m_connectionChip->setText(connected ? "● 通信已连接（模拟）" : "● 通信中断");
    m_connectionChip->setObjectName(connected ? "chipOk" : "chip");
    m_connectionChip->setStyleSheet(connected
                                        ? "padding:5px 10px;border-radius:14px;background:#E8F6EC;border:1px solid #BFE3CB;color:#14532D;"
                                        : "padding:5px 10px;border-radius:14px;background:#FDECEC;border:1px solid #F5B7B7;color:#7F1D1D;");
    m_startButton->setEnabled(connected && !m_running);
    m_stopButton->setEnabled(connected && m_running);
    addEvent(connected ? "info" : "err", "通信", connected ? "模拟 PLC 已连接" : "PLC 通信中断");
}

QString MainWindow::levelLabel(const QString &level) const
{
    if (level == "err") return "报警";
    if (level == "warn") return "警告";
    return "事件";
}

void MainWindow::addEvent(
    const QString &level,
    const QString &source,
    const QString &message)
{
    const QDateTime now = QDateTime::currentDateTime();

    // 1. 更新现有界面的内存事件列表。
    m_events.prepend({now, level, source, message});

    while (m_events.size() > 200) {
        m_events.removeLast();
    }

    // 2. 同步保存到 SQLite。
    // 使用原项目的 info / warn / err 级别，保证现有筛选逻辑兼容。
    if (m_historyStore.isOpen()) {
        if (!m_historyStore.appendEvent(
                level, source, message)) {
            qWarning() << "保存 SQLite 事件失败："
                       << m_historyStore.lastError();
        }
    }

    // 3. 刷新原有事件表格。
    renderEvents();
}

void MainWindow::renderEvents()
{
    const QString filter = m_filter ? m_filter->currentData().toString() : "all";
    m_eventTable->setRowCount(0);

    for (const auto &event : m_events) {
        if (filter != "all" && event.level != filter)
            continue;

        const int row = m_eventTable->rowCount();
        m_eventTable->insertRow(row);
        auto *timeItem = new QTableWidgetItem(event.time.toString("HH:mm:ss"));
        timeItem->setForeground(QColor("#475569"));
        m_eventTable->setItem(row, 0, timeItem);

        auto *levelItem = new QTableWidgetItem(levelLabel(event.level));
        if (event.level == "err")
            levelItem->setForeground(QColor("#DC2626"));
        else if (event.level == "warn")
            levelItem->setForeground(QColor("#B45309"));
        else
            levelItem->setForeground(QColor("#0891B2"));
        m_eventTable->setItem(row, 1, levelItem);
        m_eventTable->setItem(row, 2, new QTableWidgetItem(event.source));
        m_eventTable->setItem(row, 3, new QTableWidgetItem(event.message));
    }

    m_eventCount->setText(QString("%1 条").arg(m_events.size()));
}

void MainWindow::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "导出事件记录", "scada-events.csv", "CSV 文件 (*.csv)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入所选文件。");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF) << "时间,级别,来源,事件内容\r\n";

    auto csvCell = [](QString s) {
        s.replace("\"", "\"\"");
        return "\"" + s + "\"";
    };

    for (const auto &event : m_events) {
        out << csvCell(event.time.toString("yyyy-MM-dd HH:mm:ss")) << ","
            << csvCell(levelLabel(event.level)) << ","
            << csvCell(event.source) << ","
            << csvCell(event.message) << "\r\n";
    }
    file.close();
    statusBar()->showMessage("事件记录已导出：" + path, 5000);
}

void MainWindow::showHistoryDialog()
{
    if (!m_historyStore.isOpen()) {
        QMessageBox::warning(
            this,
            "历史查询",
            "SQLite 数据库未打开：\n" +
                m_historyStore.lastError()
            );
        return;
    }

    const QDateTime to = QDateTime::currentDateTimeUtc();
    const QDateTime from = to.addSecs(-24 * 60 * 60);

    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("SCADA 历史记录查询");
    dialog->resize(1000, 600);

    auto *layout = new QVBoxLayout(dialog);
    auto *tabs = new QTabWidget(dialog);

    // -----------------------------
    // 页签一：采样历史
    // -----------------------------
    auto *telemetryTable = new QTableWidget(dialog);
    telemetryTable->setColumnCount(5);
    telemetryTable->setHorizontalHeaderLabels({
        "采样时间",
        "电机状态",
        "电流（A）",
        "转速（rpm）",
        "PLC 通信"
    });

    telemetryTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers
        );
    telemetryTable->setSelectionBehavior(
        QAbstractItemView::SelectRows
        );
    telemetryTable->setAlternatingRowColors(true);
    telemetryTable->verticalHeader()->setVisible(false);
    telemetryTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch
        );

    const auto telemetryRecords =
        m_historyStore.queryTelemetry(from, to, 500);

    telemetryTable->setRowCount(telemetryRecords.size());

    for (int i = 0; i < telemetryRecords.size(); ++i) {
        const auto &record = telemetryRecords.at(i);

        const QString timestamp =
            record.timestamp.toLocalTime().toString(
                "yyyy-MM-dd HH:mm:ss"
                );

        telemetryTable->setItem(
            i, 0, new QTableWidgetItem(timestamp)
            );

        telemetryTable->setItem(
            i, 1,
            new QTableWidgetItem(
                record.motorRunning ? "运行" : "停止"
                )
            );

        telemetryTable->setItem(
            i, 2,
            new QTableWidgetItem(
                QString::number(record.currentA, 'f', 2)
                )
            );

        telemetryTable->setItem(
            i, 3,
            new QTableWidgetItem(
                QString::number(record.speedRpm, 'f', 0)
                )
            );

        telemetryTable->setItem(
            i, 4,
            new QTableWidgetItem(
                record.plcConnected ? "已连接" : "已断开"
                )
            );
    }

    tabs->addTab(
        telemetryTable,
        QString("采样历史（%1 条）")
            .arg(telemetryRecords.size())
        );

    // -----------------------------
    // 页签二：事件历史
    // -----------------------------
    auto *eventTable = new QTableWidget(dialog);
    eventTable->setColumnCount(4);
    eventTable->setHorizontalHeaderLabels({
        "事件时间",
        "级别",
        "来源",
        "事件内容"
    });

    eventTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers
        );
    eventTable->setSelectionBehavior(
        QAbstractItemView::SelectRows
        );
    eventTable->setAlternatingRowColors(true);
    eventTable->verticalHeader()->setVisible(false);
    eventTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents
        );
    eventTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents
        );
    eventTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents
        );
    eventTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch
        );

    const auto eventRecords =
        m_historyStore.queryEvents(from, to, QString(), 500);

    eventTable->setRowCount(eventRecords.size());

    for (int i = 0; i < eventRecords.size(); ++i) {
        const auto &event = eventRecords.at(i);

        eventTable->setItem(
            i, 0,
            new QTableWidgetItem(
                event.timestamp.toLocalTime().toString(
                    "yyyy-MM-dd HH:mm:ss"
                    )
                )
            );

        QString levelText;

        if (event.level == "err") {
            levelText = "报警";
        } else if (event.level == "warn") {
            levelText = "警告";
        } else {
            levelText = "事件";
        }

        eventTable->setItem(
            i, 1, new QTableWidgetItem(levelText)
            );

        eventTable->setItem(
            i, 2, new QTableWidgetItem(event.category)
            );

        eventTable->setItem(
            i, 3, new QTableWidgetItem(event.message)
            );
    }

    tabs->addTab(
        eventTable,
        QString("事件历史（%1 条）")
            .arg(eventRecords.size())
        );

    layout->addWidget(tabs);

    auto *closeButton = new QPushButton("关闭", dialog);
    closeButton->setFixedWidth(100);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    layout->addLayout(buttonLayout);

    connect(closeButton, &QPushButton::clicked,
            dialog, &QDialog::accept);

    dialog->exec();
    dialog->deleteLater();
}

