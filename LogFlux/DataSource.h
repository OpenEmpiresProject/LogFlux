#pragma once

#include <QStandardItemModel>
#include <QString>

class DataSource : public QObject
{
    Q_OBJECT
public:
    virtual ~DataSource() = default;
    virtual QString description() const = 0;

signals:
	void onNewLine(DataSource* source, const QString& line);
	void onStatusChange(DataSource* source, bool online);

public slots:
	virtual void startProcessing() = 0;
	virtual void refresh() = 0;
};