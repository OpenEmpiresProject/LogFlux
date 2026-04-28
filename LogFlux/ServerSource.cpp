#include "ServerSource.h"

#include <QStandardItemModel>
#include <QStandardItem>

ServerSource::ServerSource(const QString &host, int port)
    : m_host(host), m_port(port)
{
}

QString ServerSource::description() const
{
    return QStringLiteral("Server: %1:%2").arg(m_host).arg(m_port);
}

void ServerSource::startProcessing()
{

}

void ServerSource::refresh()
{
    startProcessing();
}

void ServerSource::startTailing()
{

}

void ServerSource::stopTailing()
{

}