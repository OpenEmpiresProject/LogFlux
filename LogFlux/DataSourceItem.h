#pragma once

#include <QWidget>
#include "ui_DataSourceItem.h"

class DataSourceItem : public QWidget
{
	Q_OBJECT

public:
	DataSourceItem(QWidget *parent = nullptr);
	~DataSourceItem();

	Ui::DataSourceItemClass ui;
};

