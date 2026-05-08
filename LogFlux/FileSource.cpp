#include "FileSource.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStandardItem>
#include <QTextStream>
#include <algorithm>

FileSource::FileSource(const QString& filePath) : m_filePath(filePath)
{
    watcher.addPath(filePath);

    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, this, &FileSource::onFileChanged);
}

QString FileSource::description() const
{
    return QStringLiteral("File: %1").arg(m_filePath);
}

void FileSource::startProcessing()
{
    QFileInfo fi(m_filePath);
    if (fi.exists())
    {
        emit onStatusChange(this, true);
    }
    else
    {
        emit onStatusChange(this, false);
        return;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open file" << m_filePath;
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd())
    {
        auto line = in.readLine();
        emit onNewLine(this, line);
    }

    m_lastFilePos = file.pos();
}

void FileSource::refresh()
{
    startProcessing();
}

void FileSource::onFileChanged(const QString& path)
{
    QFileInfo fi(path);
    if (not fi.exists())
    {
        emit onStatusChange(this, false);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    // file truncated or rotated — reset so the new content is read from the start
    if (file.size() < m_lastFilePos)
    {
        m_lastFilePos = 0;
    }

    file.seek(m_lastFilePos);

    QTextStream in(&file);
    while (!in.atEnd())
    {
        auto line = in.readLine();
        emit onNewLine(this, line);
    }
    // Advance the cursor so the next change event only reads newly appended bytes.
    m_lastFilePos = file.pos();
}