#pragma once

#include "DataSource.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

class ServerSource : public DataSource
{
    Q_OBJECT
  public:
    ServerSource(const QString& host, int port);
    ~ServerSource() override;

  private:
    QString m_host;
    int m_port;

    QTcpServer* m_server = nullptr;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    bool m_listening = false;

    QString description() const override;
    void startProcessing() override;
    void refresh() override;

    // internal helpers
    void handleNewConnection();
    void handleReadyRead();
    void handleDisconnected();
};