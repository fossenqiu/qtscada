#include "MotorWidget.h"

#include <QPainter>
#include <QPaintEvent>

MotorWidget::MotorWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(180, 150);
}

void MotorWidget::setRunning(bool running)
{
    if (m_running == running)
        return;

    m_running = running;
    update();
}

void MotorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF body(
        width() * 0.25,
        height() * 0.20,
        width() * 0.50,
        height() * 0.48
        );

    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(
        m_running ? QColor("#F2C94C")
                  : QColor("#A0A0A0")
        );

    painter.drawRoundedRect(body, 12, 12);

    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(body.adjusted(
        body.width() * 0.25,
        body.height() * 0.20,
        -body.width() * 0.25,
        -body.height() * 0.20
        ));

    painter.drawLine(
        QPointF(width() * 0.50, height() * 0.68),
        QPointF(width() * 0.50, height() * 0.80)
        );

    painter.drawLine(
        QPointF(width() * 0.35, height() * 0.80),
        QPointF(width() * 0.65, height() * 0.80)
        );

    painter.setPen(Qt::black);
    painter.drawText(
        QRect(0, height() * 0.82, width(), 25),
        Qt::AlignCenter,
        m_running ? "电机运行" : "电机停止"
        );
}
