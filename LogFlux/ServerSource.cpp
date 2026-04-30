#include "ServerSource.h"

#include <QStandardItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QHostAddress>
#include <QDebug>
#include <QJsonArray>

ServerSource::ServerSource(const QString &host, int port)
    : m_host(host)
    , m_port(port)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &ServerSource::handleNewConnection);
}

ServerSource::~ServerSource()
{
    if (m_server && m_server->isListening()) {
        m_server->close();
        emit onStatusChange(this, false);
    }

    // Ensure sockets are closed and deleted
    for (auto sock : m_buffers.keys()) {
        if (sock) {
            sock->disconnect(this);
            sock->close();
            sock->deleteLater();
        }
    }
    m_buffers.clear();
}

QString ServerSource::description() const
{
    return QStringLiteral("Server: %1:%2").arg(m_host).arg(m_port);
}

void ServerSource::startProcessing()
{
    if (m_listening)
        return;

    QHostAddress addr;
    if (m_host.isEmpty() || m_host == QStringLiteral("0.0.0.0") || m_host == QStringLiteral("*"))
        addr = QHostAddress::Any;
    else
        addr = QHostAddress(m_host);

    if (!m_server->listen(addr, static_cast<quint16>(m_port))) {
        qWarning() << "ServerSource: failed to listen on" << m_host << m_port << "error:" << m_server->errorString();
        m_listening = false;
        emit onStatusChange(this, false);
        return;
    }

    m_listening = true;
    emit onStatusChange(this, true);

    // If bound to port 0, update m_port to the actual port
    if (m_port == 0)
        m_port = m_server->serverPort();

    qDebug() << "ServerSource listening on" << m_server->serverAddress().toString() << ":" << m_port;
}

void ServerSource::refresh()
{
    // Restart listening if needed
    if (m_server->isListening())
        return;

    startProcessing();
}

void ServerSource::startTailing()
{
    // Not needed - server listens continuously
}

void ServerSource::stopTailing()
{
    // Not needed per requirement; keep server running
}

void ServerSource::handleNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        if (!sock)
            continue;

        // track buffer for the socket
        m_buffers.insert(sock, QByteArray());
        connect(sock, &QTcpSocket::readyRead, this, &ServerSource::handleReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &ServerSource::handleDisconnected);
        // Ensure socket is deleted later
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
        qDebug() << "ServerSource: new connection from" << sock->peerAddress().toString() << sock->peerPort();
    }
}

void ServerSource::handleReadyRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock || !m_buffers.contains(sock))
        return;

    QByteArray data = sock->readAll();
    if (data.isEmpty())
        return;

    m_buffers[sock].append(data);

    // Split into lines by '\n', keep remainder in buffer
    while (true) {
        int idx = m_buffers[sock].indexOf('\n');
        if (idx < 0)
            break;

        QByteArray lineBytes = m_buffers[sock].left(idx);
        // remove possible trailing '\r'
        if (!lineBytes.isEmpty() && lineBytes.endsWith('\r'))
            lineBytes.chop(1);

        QString line = QString::fromUtf8(lineBytes).trimmed();
        if (!line.isEmpty())
            processLine(line);

        // remove processed line + newline
        m_buffers[sock].remove(0, idx + 1);
    }
}

void ServerSource::handleDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;

    qDebug() << "ServerSource: disconnected" << sock->peerAddress().toString() << sock->peerPort();
    m_buffers.remove(sock);
    // socket will be deleted by deleteLater connected in handleNewConnection
}

static bool tryParseJsonLine(const QString& line, QJsonObject& outObj)
{
    const auto bytes = line.toUtf8();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError)
        return false;
    if (!doc.isObject())
        return false;
    outObj = doc.object();
    return true;
}

void ServerSource::processLine(const QString& line)
{
    QJsonObject obj;
    QSet<QString> keysThisLine;
    QJsonObject usedObj;

    if (tryParseJsonLine(line, obj)) {
        usedObj = obj;
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            keysThisLine.insert(it.key());
    } else {
        // fallback to raw
        usedObj.insert(QStringLiteral("raw"), QJsonValue(line));
        keysThisLine.insert(QStringLiteral("raw"));
    }

    // Update global keys/headers if needed
    updateHeadersIfNeeded(keysThisLine);

    // Build row according to current headers
    QList<QStandardItem*> rowItems;
    rowItems.reserve(m_headers.size());
    for (const QString& h : m_headers) {
        if (usedObj.contains(h)) {
            QJsonValue v = usedObj.value(h);
            QString text;
            if (v.isString()) text = v.toString();
            else if (v.isBool()) text = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
            else if (v.isDouble()) text = QString::number(v.toDouble());
            else if (v.isObject()) text = QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
            else if (v.isArray())
                text = QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
            else text = QString();
            rowItems.append(new QStandardItem(text));
        } else {
            rowItems.append(new QStandardItem(QString()));
        }
    }

    emit onNewLine(this, rowItems);
}

void ServerSource::updateHeadersIfNeeded(const QSet<QString>& keys)
{
    bool changed = false;
    for (const QString& k : keys) {
        if (!m_allKeys.contains(k)) {
            m_allKeys.insert(k);
            changed = true;
        }
    }

    if (!changed)
        return;

    // Rebuild headers deterministically (sorted)
    QStringList headers;
    for (const QString& k : m_allKeys)
        headers.append(k);
    std::sort(headers.begin(), headers.end(), [](const QString& a, const QString& b) { return a < b; });

    m_headers = headers;
    emit onHeader(this, m_headers);
}