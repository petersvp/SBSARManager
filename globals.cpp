#include "globals.h"

#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QJsonArray>


QStringList findFilesRecursively ( QStringList paths, QStringList fileTypes )
{
    if(fileTypes.isEmpty()) fileTypes << "*";
    QStringList result, more;
    QStringList::Iterator it;
    for ( uint i = 0 ; i < paths.size() ; i++ )
    {
        QDir dir( paths[i] );
        more = dir.entryList( fileTypes, QDir::Files );
        for ( it = more.begin() ; it != more.end() ; ++it )
            result.append( paths[i] + "/" + *it );
        more = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot );
        for ( it = more.begin() ; it != more.end() ; ++it )
            *it = paths[i] + "/" + *it;
        more = findFilesRecursively( more, fileTypes );
        for ( it = more.begin() ; it != more.end() ; ++it )
            result.append( *it );
    }
    return result; // yields absolute paths
}


bool gfRemoveFolder(QString path)
{
    bool result = true;
       QDir dir(path);

       if (dir.exists(path)) {
           Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
               if (info.isDir()) {
                   result = gfRemoveFolder(info.absoluteFilePath());
               }
               else {
                   result = QFile::remove(info.absoluteFilePath());
               }
               if (!result) {
                   return result;
               }
           }
           result = dir.rmdir(path);
       }
       return result;
}

bool gfRemoveFiles(QString path, QString mask)
{
    QDir d(path);
    QFileInfoList files = d.entryInfoList(QStringList()<<mask);
    foreach(QFileInfo i, files)
    {
        QFile file;
        file.setFileName(i.absoluteFilePath());
        file.remove();
    }
    return true;
}

QString UuidFromMd5(QString source)
{
    auto src = source.toUtf8();
    auto hash = QCryptographicHash::hash(src, QCryptographicHash::Md5);
    auto stringified = QString(hash.toHex());
    auto uuid = stringified;
    return uuid;
}

QString gfReadFile(QString path)
{
    QFile f(path);
    if(f.open(QIODevice::ReadOnly)) return f.readAll();
    return QString();
}


void gfSaveJson(QString file, QJsonObject val)
{
    QJsonDocument d; d.setObject(val);
    QFile f(file);
    if(f.open(QIODevice::WriteOnly)) f.write(d.toJson());
}

void gfSaveJson(QString file, QJsonArray val)
{
    QJsonDocument d; d.setArray(val);
    QFile f(file);
    if(f.open(QIODevice::WriteOnly)) f.write(d.toJson());
}

void gfSaveTextFile(QString file, QString text)
{
    QFile f(file);
    if(f.open(QIODevice::WriteOnly)) f.write(text.toUtf8());
}

QJsonObject gfReadJson(QString filePath)
{
    QFile f(filePath);
    if(f.open(QIODevice::ReadOnly))
    {
        return QJsonDocument::fromJson(f.readAll()).object();
    }
    return QJsonObject();
}

QPixmap applyEffectToImage(QPixmap src, QGraphicsEffect *effect, int extent)
{
    if(src.isNull()) return QPixmap();   //No need to do anything else!
    if(!effect) return src;              //No need to do anything else!
    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    item.setPixmap(src);
    item.setGraphicsEffect(effect);
    scene.addItem(&item);
    QImage res(src.size()+QSize(extent*2, extent*2), QImage::Format_ARGB32);
    res.fill(Qt::transparent);
    QPainter ptr(&res);
    scene.render(&ptr, QRectF(), QRectF( -extent, -extent, src.width()+extent*2, src.height()+extent*2 ) );
    return QPixmap::fromImage(res);
}

QString insertUniqueSuffixedNameInList(QStringList namesAlreadyThere, QString nameToInsert, QString suffixFormat)
{
    // obviously there
    if(!namesAlreadyThere.contains(nameToInsert)) return nameToInsert;

    // Format?
    bool isFormatValid = suffixFormat.contains("%1");
    assert(isFormatValid);
    if(!isFormatValid) suffixFormat = "%1";

    QString regex = QString(suffixFormat).replace("%1", "(\\d+)");

    // is there any number already prefixed to the nameToInsert?
    QRegExp rx(regex);
    int pos = rx.lastIndexIn(nameToInsert);
    int nextId = 1;
    if(pos != -1)
    {
        nextId = rx.cap(1).toInt();
        nameToInsert = nameToInsert.mid(0, pos);
    }

    QString newName;
    do
    {
        newName = nameToInsert + suffixFormat.arg(nextId);
        nextId++;
    }
    while(namesAlreadyThere.contains(newName));

    return newName;
}

bool isSuitableForFileSystem(const QString &s)
{
    QRegExp rx("[\\/<>|\":\?*]");
    return !s.contains(rx);
}
