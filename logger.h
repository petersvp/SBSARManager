#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextCharFormat>

void gfLog(QString text);
void gfError(QString text);
void gfWarning(QString text);
void gfSuccess(QString text);
void gfInfo(QString text);

class Logger : public QObject
{
    Q_OBJECT
public:
    explicit Logger(QObject *parent = 0);
    static Logger* instance;
    void emitTextLogged(QString text, QTextCharFormat format);

    QList<  QPair<QString,QTextCharFormat>  > entries;

signals:
    void LogText(QString text, QTextCharFormat format);

public slots:
    void gflog(QString text);
    void gfError(QString text);
    void gfWarning(QString text);
    void gfSuccess(QString text);
    void gfInfo(QString text);

public slots:
};

#endif // LOGGER_H
