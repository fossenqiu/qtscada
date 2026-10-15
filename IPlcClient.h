#pragma once

#ifndef IPLCCLIENT_H
#define IPLCCLIENT_H

#include <QObject>
#include <QString>
#include <QVariant>

class IPlcClient : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~IPlcClient() override = default;

    virtual bool connectToPlc() = 0;
    virtual void disconnectFromPlc() = 0;
    virtual bool isConnected() const = 0;
    virtual bool readTag(const QString &tag, QVariant &value) = 0;
    virtual bool writeTag(const QString &tag, const QVariant &value) = 0;

signals:
    void connectionChanged(bool connected);
    void tagUpdated(const QString &tag, const QVariant &value);
    void communicationError(const QString &message);
};

#endif // IPLCCLIENT_H
