#pragma once

#include "DataSource.h"
#include <QString>

class ServerSource : public DataSource
{
public:
    ServerSource(const QString &host, int port);
    ~ServerSource() override = default;

private:
    QString m_host;
    int m_port;

	QString description() const override;
	void startProcessing() override;
	void refresh() override;
	void startTailing() override;
	void stopTailing() override;
};