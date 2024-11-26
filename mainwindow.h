#pragma once

#include <QMainWindow>

#include "bass.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_inputVolume_valueChanged(int value);

    void on_outputVolume_valueChanged(int value);

    void on_inputDevice_currentIndexChanged(int index);

    void on_outputDevice_currentIndexChanged(int index);

    void on_runButton_clicked(bool checked);

private:
    Ui::MainWindow *ui;

    HRECORD recordHandle;
    HSTREAM playHandle;
 };
