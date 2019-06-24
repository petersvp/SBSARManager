#pragma once

#include <QDomDocument>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include <QPixmap>
#include <QGraphicsEffect>
#include <mutex>

typedef std::lock_guard<std::mutex> MutexLocker;

bool gfRemoveFolder(QString path);
bool gfRemoveFiles(QString dir, QString mask);
QStringList findFilesRecursively ( QStringList paths, QStringList fileTypes );
QDomDocument* gfReadXml(QString fileData);
QJsonObject gfReadJson(QString filePath);
QString gfReadFile(QString path);
void gfSaveJson(QString file, QJsonObject val);
void gfSaveJson(QString file, QJsonArray val);
void gfSaveTextFile(QString file, QString text);
QString UuidFromMd5(QString source);

QString insertUniqueSuffixedNameInList(QStringList namesAlreadyThere, QString nameToInsert, QString suffixFormat = " (%1)");

template<class T>
const T& clamp(const T& x, const T& upper, const T& lower) {
    return std::min(upper, std::max(x, lower));
}

bool isSuitableForFileSystem(const QString& s);

QPixmap applyEffectToImage(QPixmap src, QGraphicsEffect *effect, int extent=0);
