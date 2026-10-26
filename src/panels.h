#pragma once
#include "settings.h"
#include <QDialog>
#include <QImage>
class QListWidget;
class QLabel;
class QComboBox;
class QSpinBox;
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Settings& settings, QWidget* parent = nullptr);
};
class CollageDialog : public QDialog
{
    Q_OBJECT
public:
    CollageDialog(const QImage& current, QWidget* parent = nullptr);
    QImage resultImage() const;

private:
    QListWidget* list;
    QLabel* preview;
    QComboBox* direction;
    QSpinBox* gap;
    void refresh();
};
