#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

class IPlcClient;
class MotorWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startMotor();
    void stopMotor();
    void updateMotor(bool running);
    void updateConnection(bool connected);

private:
    void setupUi();
    void appendAlarm(const QString &message);

    IPlcClient *m_plc = nullptr;

    QLabel *m_connectionLabel = nullptr;
    QLabel *m_motorLabel = nullptr;

    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;

    MotorWidget *m_motorWidget = nullptr;
    QTableWidget *m_alarmTable = nullptr;

};
#endif // MAINWINDOW_H
