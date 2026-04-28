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
	void onHeader(DataSource* source, QStringList headers);
	void onNewLine(DataSource* source, QList<QStandardItem*> cells);
	void onStatusChange(DataSource* source, bool online);

public slots:
	virtual void startProcessing() = 0;
	virtual void refresh() = 0;
	virtual void startTailing() = 0;
	virtual void stopTailing() = 0;
};