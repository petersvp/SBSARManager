#include "logger.h"
#include <QApplication>
#include <iostream>
using std::cout;

Logger*  Logger::instance (new Logger());

void gfLog(QString text)
{
    cout << text.toStdString() << "\n";
    Logger::instance->emitTextLogged(text, QTextCharFormat());
}

void gfError(QString text)
{
    cout << "E: " << text.toStdString() << "\n";
    QTextCharFormat f;
    f.setForeground(QColor(255,0,0));
    //f.setTextOutline(QPen( QColor(255,170,170,80) , 3));
    Logger::instance->emitTextLogged(text,f);
}

void gfWarning(QString text)
{
    cout << "W: " << text.toStdString() << "\n";
    QTextCharFormat f;
    f.setForeground(QColor(255,128,0));
    Logger::instance->emitTextLogged(text,f);
}

void gfSuccess(QString text)
{
    cout << "S: " << text.toStdString() << "\n";
    QTextCharFormat f;
    f.setForeground(QColor(0,180,0));
    Logger::instance->emitTextLogged(text,f);
}

void gfInfo(QString text)
{
    cout << "I: " << text.toStdString() << "\n";
    QTextCharFormat f;
    f.setForeground(QColor(150,150,150));
    Logger::instance->emitTextLogged(text,f);
}


Logger::Logger(QObject *parent) : QObject(parent)
{
    // Does nothing, only fires signals
    instance = this;
}

void Logger::emitTextLogged(QString text, QTextCharFormat format)
{
    emit LogText(text, format);
    //entries.append(  QPair< QString, QTextCharFormat> (text, format)  );
}

void Logger::gflog(QString text)
{
    ::gfLog(text);
}

void Logger::gfError(QString text)
{
    ::gfError(text);
}

void Logger::gfWarning(QString text)
{
    ::gfWarning(text);
}

void Logger::gfSuccess(QString text)
{
    ::gfSuccess(text);
}

void Logger::gfInfo(QString text)
{
    ::gfInfo(text);
}
