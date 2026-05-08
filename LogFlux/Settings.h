#pragma once

#include "ui_Settings.h"

#include <QDialog>

class Settings : public QDialog
{
    Q_OBJECT

  public:
    Settings(QWidget* parent = nullptr);
    ~Settings();

    Ui::SettingsClass ui;
};
