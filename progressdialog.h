#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QWidget>

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = 0);
    ~ProgressDialog();
    void setMax(int n);
    void setProgress(int p);
    void setText(QString text);
    bool usePercents;

public slots:
    void advance(int n);
    void advanceOne();

private:
    Ui::ProgressDialog *ui;
};

#endif // PROGRESSDIALOG_H
