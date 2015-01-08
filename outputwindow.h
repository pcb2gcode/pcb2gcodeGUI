#ifndef OUTPUTWINDOW_H
#define OUTPUTWINDOW_H

#include <QDialog>
#include <QPushButton>
#include <QPlainTextEdit>

namespace Ui {
class outputWindow;
}

class outputWindow : public QDialog
{
    Q_OBJECT

public:
    explicit outputWindow(QWidget *parent = 0);
    ~outputWindow();
    QPushButton *getPushButton();
    QPlainTextEdit *getPlainTextEdit();

private:
    Ui::outputWindow *ui;
};

#endif // OUTPUTWINDOW_H
