#pragma once

#ifndef MOTORWIDGET_H
#define MOTORWIDGET_H

#pragma once

#include <QWidget>

class MotorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorWidget(QWidget *parent = nullptr);

    void setRunning(bool running);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_running = false;
};

#endif // MOTORWIDGET_H
