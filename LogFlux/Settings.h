#pragma once

#include <QDialog>
#include "ui_Settings.h"

class Settings : public QDialog
{
	Q_OBJECT

public:
	Settings(QWidget *parent = nullptr);
	~Settings();

	Ui::SettingsClass ui;

};

