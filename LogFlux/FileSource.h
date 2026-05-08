#pragma once

#include "DataSource.h"

#include <QFileSystemWatcher>
#include <QString>

class FileSource : public DataSource
{
  public:
    explicit FileSource(const QString& filePath);
    ~FileSource() override = default;

    QString description() const override;

  private:
    QString m_filePath;
    QFileSystemWatcher watcher;
    qint64 m_lastFilePos = 0;

  public slots:
    void startProcessing() override;
    void refresh() override;
    void onFileChanged(const QString& path);
};