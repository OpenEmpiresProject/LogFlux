#pragma once

#include "DataSource.h"
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QSet>

class ServerSource : public DataSource
{
    Q_OBJECT
public:
    ServerSource(const QString &host, int port);
    ~ServerSource() override;

private:
    QString m_host;
    int m_port;

    QTcpServer* m_server = nullptr;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QSet<QString> m_allKeys;
    QStringList m_headers;
    bool m_listening = false;

    QString description() const override;
    void startProcessing() override;
    void refresh() override;
    void startTailing() override;
    void stopTailing() override;

    // internal helpers
    void handleNewConnection();
    void handleReadyRead();
    void handleDisconnected();
    void processLine(const QString& line);
    void updateHeadersIfNeeded(const QSet<QString>& keys);
};