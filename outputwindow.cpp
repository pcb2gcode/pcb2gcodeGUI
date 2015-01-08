#include "outputwindow.h"
#include "ui_outputwindow.h"

outputWindow::outputWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::outputWindow)
{
    ui->setupUi(this);
}

outputWindow::~outputWindow()
{
    delete ui;
}

QPushButton *outputWindow::getPushButton()
{
    return ui->killClosePushButton;
}

QPlainTextEdit *outputWindow::getPlainTextEdit()
{
    return ui->outputPlainTextEdit;
}
