#include "MockPlcClient.h"

MockPlcClient::MockPlcClient(QObject *parent)
    : IPlcClient(parent)
{
    m_tags["start"] = false;
    m_tags["stop"] = false;
    m_tags["motor"] = false;

    m_timer.setInterval(200);

    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (m_connected) {
            emit tagUpdated("motor", m_tags["motor"]);
        }
    });
}

bool MockPlcClient::connectToPlc()
{
    m_connected = true;
    m_timer.start();

    emit connectionChanged(true);
    emit tagUpdated("motor", m_tags["motor"]);

    return true;
}

void MockPlcClient::disconnectFromPlc()
{
    m_timer.stop();
    m_connected = false;

    emit connectionChanged(false);
}

bool MockPlcClient::isConnected() const
{
    return m_connected;
}

bool MockPlcClient::readTag(
    const QString &tag,
    QVariant &value)
{
    if (!m_connected || !m_tags.contains(tag))
        return false;

    value = m_tags.value(tag);
    return true;
}

bool MockPlcClient::writeTag(
    const QString &tag,
    const QVariant &value)
{
    if (!m_connected || !m_tags.contains(tag))
        return false;

    if (tag == "start" && value.toBool()) {
        m_tags["motor"] = true;
        m_tags["start"] = false;
        m_tags["stop"] = false;
    } else if (tag == "stop" && value.toBool()) {
        m_tags["motor"] = false;
        m_tags["start"] = false;
        m_tags["stop"] = false;
    } else {
        m_tags[tag] = value;
    }

    emit tagUpdated("motor", m_tags["motor"]);
    return true;
}
