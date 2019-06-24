#ifndef SUBSTANCEPRESETPREVIEW_H
#define SUBSTANCEPRESETPREVIEW_H

#include <QCache>
#include <QLabel>
#include <QProcess>
#include <QWidget>
#include "sbspreset.h"
#include "processmanager.h"

class SubstancePresetPreview: public QWidget
{
    Q_OBJECT

public:
    SubstancePresetPreview();
    ~SubstancePresetPreview();

    // required
    void SetPresetAndRenderPreview(SBSPreset* preset);
    void SetPreviewSize(QSize size);

    // forces re-render [update cache]
    // if displayLoader will immediately sgow loader
    void ForceReRender(bool displayLoader = false);

    // internal but exposed
    void RenderPreview(bool useCache = true, bool dontRequestRender = false);
    void PostRenderPreview();

    // Custom widget
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);

    // label enabled
    void SetLabelEnabled(bool enable);

    QString GetCacheDir();
    QProcess process;

    SBSPreset* preset() const;
    QPixmap texture() const;

    bool isLoading() const;

    QColor messageColor = QColor();
    QString messageText = "Loading...\n";

    static QCache<QString, QImage> imageCache;
    static QImage LoadImageThroughCache(QString cacheKey, bool useCache = true);
    static void ClearCacheForDir(QString prefix);

public slots:
    void OnSBSRenderCompleted(PresetRender* p);

private:
    SBSPreset* m_preset;
    bool m_isLoading;
    QSize m_size;
    int lgx, lgy;
    QPixmap m_texture;
    QLabel* m_label;
};

#endif // SUBSTANCEPRESETPREVIEW_H
