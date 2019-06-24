#include "pbrview.h"
#include "ui_pbrview.h"

#include <QWebEngineView>
#include <QDir>
#include <QWebEngineProfile>

#include "processmanager.h"
#include "substancemanagerapp.h"



PBRView::PBRView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PBRView)
{
    ui->setupUi(this);
    m_textureSize = QSize(255,255);

    QWebEngineProfile::defaultProfile()->setHttpCacheType(QWebEngineProfile::NoCache);
    view = new QWebEngineView();

    view->setUrl(QUrl::fromLocalFile( QDir::currentPath() + "/pbrview/index.html"));
    view->page()->setBackgroundColor(Qt::transparent);

    // Chromium DevTools Open
    //auto dev = new QWebEngineView();
    //view->page()->setDevToolsPage(dev->page());
    //view->page()->triggerAction(QWebEnginePage::InspectElement);
    //dev->setWindowTitle("SBSAR Library Manager PBRView DevTools");
    //dev->show();

    connect((QObject*)ProcessManager::get(), SIGNAL(PresetRenderFinished(PresetRender*)), this, SLOT(PresetRenderFinished(PresetRender*)));

    ui->frame->layout()->addWidget(view);
}

PBRView::~PBRView()
{
    delete ui;
}


void PBRView::themeChanged()
{
    //view->defaultFrameGraph()->setClearColor(palette().color(QPalette::Window));
}

void PBRView::SetPreset(SBSPreset *p)
{
    m_preset = p;
    ProcessManager::SchedulePresetRender(p, m_textureSize, "", false);
}

void PBRView::hideEvent(QHideEvent *event)
{
    view->page()->runJavaScript("setCubeVisible(false)");
}

void PBRView::PresetRenderFinished(PresetRender *p)
{
    // ProcessManager spams with these notifications,
    // we must manually filter and only process the proper one!
    if(!isVisible()) return;
    if(p->preset != m_preset || p->size != m_textureSize) return;

    auto dir = SubstanceManagerApp::instance->GetPresetCacheDir(m_preset, p->size, p->suffix);

    auto outBase      = m_preset->GetDefaultPreviewOutput();
    auto outNormal    = m_preset->GetChannelOutput("normal");
    auto outMetal     = m_preset->GetChannelOutput("metallic");
    auto outRough     = m_preset->GetChannelOutput("roughness");
    auto outOcclusion = m_preset->GetChannelOutput("ambientOcclusion");

    QString js = "setCubeVisible(true);";
    js.append(outBase?      "SetTexture(\"map\", \""+          QUrl::fromLocalFile(dir+"/"+outBase->identifier+".png").toString()+"\");"      : "material.map = null;");
    js.append(outNormal?    "SetTexture(\"normalMap\", \""+    QUrl::fromLocalFile(dir+"/"+outNormal->identifier+".png").toString()+"\");"    : "material.normalMap = null;");
    js.append(outMetal?     "SetTexture(\"metalnessMap\", \""+ QUrl::fromLocalFile(dir+"/"+outMetal->identifier+".png").toString()+"\");"     : "material.metalnessMap = null;");
    js.append(outRough?     "SetTexture(\"roughnessMap\", \""+ QUrl::fromLocalFile(dir+"/"+outRough->identifier+".png").toString()+"\");"     : "material.roughnessMap = null;");
    js.append(outOcclusion? "SetTexture(\"aoMap\", \""+        QUrl::fromLocalFile(dir+"/"+outOcclusion->identifier+".png").toString()+"\");" : "material.aoMap = null;");
    view->page()->runJavaScript(js);

    // now it's the proper one!
    qDebug() << "PBR VIEW updates...";
}

void PBRView::on_comboBox_activated(int index)
{
    int items[] = {127,255,511,1023};
    m_textureSize = QSize(items[index],items[index]);
    ProcessManager::SchedulePresetRender(m_preset, m_textureSize, "", false);
}
