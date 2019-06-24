#include "progressdialog.h"
#include "ui_progressdialog.h"

ProgressDialog::ProgressDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
    usePercents = false;
    setWindowModality(Qt::ApplicationModal);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

void ProgressDialog::setMax(int n)
{
    ui->progressBar->setMaximum(n);
}

void ProgressDialog::setProgress(int p)
{
    ui->progressBar->setValue(p);
    if(usePercents)
    {
        ui->percent->setText( QString::number(int(float(p)/float(ui->progressBar->maximum())*100)) +"%" );
    }
    else
    {
        ui->percent->setText(QString("%1 / %2").arg(p).arg(ui->progressBar->maximum()));
    }
}

void ProgressDialog::setText(QString text)
{
    ui->label->setText(text);
}

void ProgressDialog::advance(int n)
{
    setProgress(ui->progressBar->value()+n);
}

void ProgressDialog::advanceOne()
{
    advance(1);
}
