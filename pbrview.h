#ifndef PBRVIEW_H
#define PBRVIEW_H

#include <QWidget>
#include "sbspreset.h"
#include "processmanager.h"
#include <QWebEngineView>

namespace Ui {
class PBRView;
}

class PBRView : public QWidget
{
    Q_OBJECT

public:
    explicit PBRView(QWidget *parent = nullptr);
    ~PBRView();

    void themeChanged();

    Ui::PBRView *ui;

    void SetPreset(SBSPreset* p);

    void hideEvent(QHideEvent* event);

    QSize m_textureSize;

private slots:
    void PresetRenderFinished(PresetRender* p);

    void on_comboBox_activated(int index);

private:
    SBSPreset* m_preset;
    QWebEngineView* view;
};

#endif // PBRVIEW_H
