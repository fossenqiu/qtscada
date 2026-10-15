#include "mainwindow.h"

#include "MockPlcClient.h"
#include "MotorWidget.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QStatusBar>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    m_plc = new MockPlcClient(this);

    connect(
        m_plc, &IPlcClient::connectionChanged,
        this, &MainWindow::updateConnection
        );

    connect(
        m_plc, &IPlcClient::tagUpdated,
        this,
        [this](const QString &tag, const QVariant &value) {
            if (tag == "motor")
                updateMotor(value.toBool());
        }
        );

    connect(
        m_plc, &IPlcClient::communicationError,
        this, &MainWindow::appendAlarm
        );

    m_plc->connectToPlc();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    setWindowTitle("Qt SCADA - PLC 电机监控实验");
    resize(1000, 680);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *header = new QLabel("工业设备监控系统 | 电机起保停实验");
    header->setStyleSheet(
        "font-size:22px;font-weight:bold;padding:12px;"
        );

    m_connectionLabel = new QLabel("通信状态：未连接");
    m_motorLabel = new QLabel("电机状态：未知");

    auto *statusLayout = new QHBoxLayout;
    statusLayout->addWidget(m_connectionLabel);
    statusLayout->addWidget(m_motorLabel);
    statusLayout->addStretch();

    auto *deviceGroup = new QGroupBox("设备监控");
    auto *deviceLayout = new QHBoxLayout(deviceGroup);

    m_motorWidget = new MotorWidget;

    auto *operationLayout = new QVBoxLayout;

    m_startButton = new QPushButton("启动电机");
    m_stopButton = new QPushButton("停止电机");

    m_startButton->setMinimumHeight(48);
    m_stopButton->setMinimumHeight(48);

    m_startButton->setStyleSheet(
        "QPushButton { background:#D9EAD3; padding:10px; }"
        );

    m_stopButton->setStyleSheet(
        "QPushButton { background:#F4CCCC; padding:10px; }"
        );

    operationLayout->addWidget(m_startButton);
    operationLayout->addWidget(m_stopButton);
    operationLayout->addStretch();

    deviceLayout->addWidget(m_motorWidget, 2);
    deviceLayout->addLayout(operationLayout, 1);

    auto *alarmGroup = new QGroupBox("报警与事件记录");
    auto *alarmLayout = new QVBoxLayout(alarmGroup);

    m_alarmTable = new QTableWidget(0, 2);
    m_alarmTable->setHorizontalHeaderLabels(
        {"时间", "事件内容"}
        );

    m_alarmTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch
        );

    m_alarmTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers
        );

    alarmLayout->addWidget(m_alarmTable);

    mainLayout->addWidget(header);
    mainLayout->addLayout(statusLayout);
    mainLayout->addWidget(deviceGroup, 2);
    mainLayout->addWidget(alarmGroup, 1);

    setCentralWidget(central);

    connect(
        m_startButton, &QPushButton::clicked,
        this, &MainWindow::startMotor
        );

    connect(
        m_stopButton, &QPushButton::clicked,
        this, &MainWindow::stopMotor
        );
}

void MainWindow::startMotor()
{
    if (!m_plc || !m_plc->isConnected()) {
        appendAlarm("启动失败：PLC 未连接");
        return;
    }

    if (!m_plc->writeTag("start", true)) {
        appendAlarm("启动命令写入失败");
        return;
    }

    appendAlarm("已发送电机启动请求");
}

void MainWindow::stopMotor()
{
    if (!m_plc || !m_plc->isConnected()) {
        appendAlarm("停止失败：PLC 未连接");
        return;
    }

    if (!m_plc->writeTag("stop", true)) {
        appendAlarm("停止命令写入失败");
        return;
    }

    appendAlarm("已发送电机停止请求");
}

void MainWindow::updateMotor(bool running)
{
    m_motorWidget->setRunning(running);

    m_motorLabel->setText(
        running ? "电机状态：运行"
                : "电机状态：停止"
        );
}

void MainWindow::updateConnection(bool connected)
{
    m_connectionLabel->setText(
        connected ? "通信状态：已连接（模拟）"
                  : "通信状态：未连接"
        );

    m_startButton->setEnabled(connected);
    m_stopButton->setEnabled(connected);

    appendAlarm(
        connected ? "模拟 PLC 已连接"
                  : "模拟 PLC 已断开"
        );
}

void MainWindow::appendAlarm(const QString &message)
{
    const int row = m_alarmTable->rowCount();

    m_alarmTable->insertRow(row);

    m_alarmTable->setItem(
        row, 0,
        new QTableWidgetItem(
            QDateTime::currentDateTime().toString(
                "yyyy-MM-dd HH:mm:ss"
                )
            )
        );

    m_alarmTable->setItem(
        row, 1,
        new QTableWidgetItem(message)
        );

    m_alarmTable->scrollToBottom();
}
