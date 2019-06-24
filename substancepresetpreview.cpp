#include "substancepresetpreview.h"
#include "substancemanagerapp.h"
#include "logger.h"
#include "globals.h"
#include "processmanager.h"
#include <QDebug>
#include <QDir>
#include <QDir>
#include <QPainter>
#include <QProcess>
#include <QGraphicsDropShadowEffect>
#include <algorithm>
#include <cmath>
#include <QTimer>

QCache<QString, QImage> SubstancePresetPreview::imageCache(1000);
QImage SubstancePresetPreview::LoadImageThroughCache(QString fileName, bool useCache)
{
    if(!useCache) imageCache.remove(fileName);
    auto obj = imageCache.object(fileName);
    if(obj)
    {
        //qDebug() << "LoadImageThroughCache: RAM CACHED";
        return *obj;
    }
    else
    {
        auto i = new QImage(fileName);
        if(i->isNull())
        {
            //qDebug() << "LoadImageThroughCache: NULL IMAGE";
            return *i;
        }
        else
        {
            //qDebug() << "LoadImageThroughCache: HDD/SSD CACHED";
            imageCache.insert(fileName, i);
            return *i;
        }
    }
}

void SubstancePresetPreview::ClearCacheForDir(QString prefix)
{
    auto keys = imageCache.keys();
    for(auto k: keys)
        if(k.startsWith(prefix))
            imageCache.remove(k);
}


SubstancePresetPreview::SubstancePresetPreview()
{
    m_size = QSize(128,128);
    lgx = 8;
    lgy = 8;
    m_preset = nullptr;
    m_isLoading = true;
    m_label = nullptr;
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    messageColor = Qt::transparent;
    connect((QObject*)ProcessManager::get(), SIGNAL(PresetRenderFinished(PresetRender*)), this, SLOT(OnSBSRenderCompleted(PresetRender*)));
}

SubstancePresetPreview::~SubstancePresetPreview()
{

}

void SubstancePresetPreview::SetPresetAndRenderPreview(SBSPreset *preset)
{
    m_preset = preset;
    if(m_label) m_label->setText(preset->label);

    // render with cache
    RenderPreview(true);
}

void SubstancePresetPreview::SetPreviewSize(QSize size)
{
    m_size = size;
    lgx = 1+(log2(size.width()));
    lgy = 1+(log2(size.height()));
}

void SubstancePresetPreview::ForceReRender(bool displayLoader)
{
    // TODO: if cache present will directly use/load it!
    // Each UI Regen will load all textures from cache,
    // this is inefficient due to disk access.
    // eventually Implement two levels of cache - 1. disk, 2. ram - for performance
    if(displayLoader) m_isLoading = true;

    RenderPreview(false);
}

void SubstancePresetPreview::OnSBSRenderCompleted(PresetRender* p)
{
    if(!p->preset || p->preset != m_preset || p->size != m_size) return;
    if(!m_preset) return;

    PostRenderPreview();    // recursion may get infinite if I/O issues
    update();
}

bool SubstancePresetPreview::isLoading() const
{
    return m_isLoading;
}

QPixmap SubstancePresetPreview::texture() const
{
    return m_texture;
}

SBSPreset *SubstancePresetPreview::preset() const
{
    return m_preset;
}

void SubstancePresetPreview::PostRenderPreview()
{
    auto baseColorOutput = m_preset->GetDefaultPreviewOutput();
    auto normalOutput = m_preset->GetNormalOutput();
    QString dir = SubstanceManagerApp::instance->GetPresetCacheDir(m_preset, m_size);
    QString previewPNG = dir + "/" + m_preset->label + "-preview.png";
    if(!normalOutput)
    {
        QFile::copy(dir + "/" + baseColorOutput->identifier + ".png" , previewPNG);
        m_texture.load(previewPNG);
    }
    else
    {
        QString baseColorPNG = dir + "/" + baseColorOutput->identifier + ".png";
        QString normalPNG = dir + "/" + normalOutput->identifier + ".png";

        // combine them somehow
        QImage i = QImage(baseColorPNG).scaled(m_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QImage n = QImage(normalPNG).scaled(m_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        if(i.isNull())
        {
            SetLabelEnabled(true);
            m_label->setText("Null BaseColor");
            m_isLoading = false;
            return;
        }
        if(n.isNull())
        {
            m_texture = QPixmap::fromImage(i);
            m_isLoading = false;
            return;
        }

        // Normal shading
        for(int c=0;c<m_size.width();c++)
        for(int r=0;r<m_size.height();r++)
        {
            auto base = i.pixel(c,r);
            auto normal = n.pixel(c,r);

            //will only use the Normal Y for shading
            int ny = 255 - qGreen(normal); // 0...255
            ny -= 255/2; // -128...127

            auto R = qRed(base);
            auto G = qGreen(base);
            auto B = qBlue(base);
            R = clamp(R+ (ny), 255, 0);
            G = clamp(G+ (ny), 255, 0);
            B = clamp(B+ (ny), 255, 0);

            i.setPixel(c, r, qRgb(R,G,B));
        }

        i.save(previewPNG);
        m_texture = QPixmap::fromImage(i);
    }
    /**/

    m_isLoading = false;
}

void SubstancePresetPreview::RenderPreview(bool useCache, bool dontRequestRender)
{
    // requirements
    if(!m_preset) return;

    auto baseColorOutput = m_preset->GetDefaultPreviewOutput();
    if(!baseColorOutput)
    {
        messageColor = Qt::red;
        messageText = m_preset->isTemplatePreset ? "Invalid SBSAR" : "Invalid Preset";
        return;
    }

    QString dir = SubstanceManagerApp::instance->GetPresetCacheDir(m_preset, m_size);

    // Delete the cache if not using cache
    if(!useCache) gfRemoveFolder(dir);

    // Do we have the preview in cache?
    // if so, we will use it, otherwise we will run the SBSRender to grenerate basecolor & normal
    // then rebuild the final image
    QString previewPNG = dir + "/" + m_preset->label + "-preview.png";
    QImage preview = LoadImageThroughCache(previewPNG, m_preset->needsRerender ? false : useCache);

    if(!preview.isNull())
    {
        m_texture = QPixmap::fromImage(preview);
        m_isLoading = false;
    }
    else
    {
        // Recursion breaker
        if(dontRequestRender) return;

        // request SBSRender to generate them
        ProcessManager::SchedulePresetRender(m_preset, m_size, "", false);
    }
    m_preset->needsRerender = false;
}

void SubstancePresetPreview::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::HighQualityAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const int b = 2;
    if(m_isLoading)
    {
        QColor color;
        if(messageColor.alpha()==0)
        {
            color = palette().color(QPalette::WindowText);
            color.setAlpha(60);
        }
        else color = messageColor;
        p.setBrush(Qt::transparent);
        p.setPen(QPen(color, 1));
        p.drawRect(rect().adjusted(b,b,-b-1,-b-1));

        QTextOption opt;
        opt.setAlignment(Qt::AlignCenter);
        p.setPen(color);
        p.drawText(rect(), messageText, opt);
    }
    else
    {
        p.drawPixmap(rect().adjusted(b,b,-b,-b), m_texture);
    }
}

void SubstancePresetPreview::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "WTF?";
}

// something special
class MultiShadowGraphicsEffect: public QGraphicsDropShadowEffect
{
public:
    struct Shadow
    {
        int dx, dy;
        int blur;
        QColor color;
    };

    QList<Shadow> shadows;

    void draw(QPainter* p)
    {
        for(const auto& s: shadows)
        {
            QGraphicsDropShadowEffect::setOffset(s.dx, s.dy);
            QGraphicsDropShadowEffect::setColor(s.color);
            QGraphicsDropShadowEffect::setBlurRadius(s.blur);
            QGraphicsDropShadowEffect::draw(p);
        }
    }

};

void SubstancePresetPreview::SetLabelEnabled(bool enable)
{
    if(enable)
    {
        m_label = new QLabel();
        m_label->setParent(this);
        m_label->setGeometry(4,0,width(), height()-4);
        m_label->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        m_label->setText(m_preset ? m_preset->label : "[error]");
        auto ds = new MultiShadowGraphicsEffect();
        MultiShadowGraphicsEffect::Shadow s;
        s.dx = 0;
        s.dy = 0;
        s.color = Qt::black;
        s.blur = 10;
        ds->shadows.append(s);
        ds->shadows.append(s);
        ds->shadows.append(s);
        ds->shadows.append(s);

        m_label->setGraphicsEffect(ds);
        m_label->setStyleSheet("color:white");
    }
    else
    {
        delete m_label;
        m_label = nullptr;
    }
}
