#pragma once

#ifndef MOCKPLCCLIENT_H
#define MOCKPLCCLIENT_H

#include "IPlcClient.h"
#include <QHash>
#include <QTimer>

class MockPlcClient : public IPlcClient
{
    Q_OBJECT
public:
    explicit MockPlcClient(QObject *parent = nullptr);

    bool connectToPlc() override;
    void disconnectFromPlc() override;
    bool isConnected() const override;
    bool readTag(const QString &tag, QVariant &value) override;
    bool writeTag(const QString &tag, const QVariant &value) override;

private:
    bool m_connected = false;
    QHash<QString, QVariant> m_tags;
    QTimer m_timer;
};

#endif // MOCKPLCCLIENT_H
