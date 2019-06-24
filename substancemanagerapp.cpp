#include "DarkStyle.h"
#include "progressdialog.h"
#include "substancemanagerapp.h"
#include "ui_substancemanagerapp.h"
#include "ui_pbrview.h"
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QScrollBar>
#include <QThread>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QInputDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QCheckBox>
#include "globals.h"
#include "logger.h"
#include "substancepresetpreview.h"
#include "processmanager.h"
#include "flowlayout.h"
#include <QUuid>
#include <Windows.h>
#include <QGraphicsDropShadowEffect>
#include "projectsettings.h"
#include <utility>

const char* cacheDirName = "/.SMCache/";

#define SIGNALPTR(PARAMTYPE,CLASS,FUNCNAME) static_cast<void(CLASS::*)(PARAMTYPE)>(&CLASS::FUNCNAME)

QString SubstanceManagerApp::sbsRenderExe;
QString SubstanceManagerApp::sbs7zExe;
SubstanceManagerApp* SubstanceManagerApp::instance = nullptr;

SubstanceManagerApp::SubstanceManagerApp(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SubstanceManagerApp)
{
    instance = this;
    defaultPalette = qApp->palette();
    m_currentCategory = nullptr;
    m_thumbSize = QSize(100,100);

    ui->setupUi(this);
    QCoreApplication::setOrganizationName("Pi-Dev");
    QCoreApplication::setOrganizationDomain("http://pi-dev.com");
    QCoreApplication::setApplicationName("SBSAR Library Manager");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    settings = new QSettings();

    // UI fix
    ui->mainToolBar->addWidget(ui->ThumbnailSize);
    connect(ui->pbrView->ui->Back, &QToolButton::clicked, [=]{
       ui->mainSW->setCurrentWidget(ui->pgManager);
    });

    // Logging
    //connect(Logger::instance, SIGNAL(LogText(QString, QTextCharFormat)), this, SLOT(OnTextLogged(QString,QTextCharFormat)));

    // Process Manager
    connect( (QObject*)ProcessManager::get(), SIGNAL(PresetRenderFinished(SBSPreset*)), this, SLOT(OnPresetRendered(SBSPreset*)));

    // External tools
    initSAT();
    init7z();

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    // Last opened project
    LoadProject(settings->value("LastProject").toString(), false);
    ReimportSubstances();
    UpdateLibraryUI();
    UpdateCategoryButtons();
    UpdatePresetsUI(m_currentCategory);
    //qDebug() << "\nSubstanceManagerApp ctor";

    // Process Manager setup
    ProcessManager::setMaxAllowedProcesses(QThread::idealThreadCount());

    // Auto rescan for SBSARs setup - needs better implementation, actually changing the app focus
    connect(qApp, &QApplication::focusChanged, [=](QWidget * old, QWidget * now){

        qDebug() << "&QApplication::focusChanged: old" << old << "new" << now;
        if(old == nullptr && now && now->window() == window())
        {
            if(m_projectFilePath == "") return;
            gfWarning("Rescanning Library");
            qDebug() << "Auto rescan SBSAR routine";

            qDebug() << "ReimportSubstances()";
            this->ReimportSubstances();
            qDebug() << "UpdateLibraryUI()";
            this->UpdateLibraryUI();
            qDebug() << "UpdatePresetsUI()";
            this->UpdatePresetsUI(m_currentCategory);
            //justFocused = true;
        }
    });

    // D&D
    qApp->installEventFilter(this);

    // Categories
    ui->allCategories->setProperty("CategoryButton", qVariantFromValue((void*)nullptr));
    ui->centralWidget->setMinimumWidth(300);
    ui->allCategories->setFocusPolicy(Qt::NoFocus);

    // TODO is Dark theme? Call that, persist
    //on_actionTest1_triggered();
    on_actionThemeDefault_triggered();
}

//void SubstanceManagerApp::OnTextLogged(QString text, QTextCharFormat format)
//{
//    ui->console->moveCursor (QTextCursor::End);
//    QTextCursor tc = ui->console->textCursor();
//    tc.setCharFormat(format);
//    tc.insertText(text);
//    tc.insertBlock();
//    QScrollBar* vscroll = ui->console->verticalScrollBar();
//    if(vscroll->value() == vscroll->maximum()) vscroll->setValue(vscroll->maximum());
//}

void SubstanceManagerApp::ReimportSubstances()
{
    if(m_projectFilePath == "") return;

    m_library.clear();

    ProgressDialog pd(this);
    pd.setWindowFlags(Qt::Dialog);
    pd.setWindowTitle("Progress");

    QStringList sbsars = findFilesRecursively( QStringList()<<m_projectDir, QStringList()<<"*.sbsar" );
    pd.setProgress(0);
    pd.setMax(sbsars.size());
    //pd.show();

    int i=0;

    // ref to SBSAR data
    auto oldLibrary = m_projectData["Library"].toObject();
    QJsonObject newLibrary;

    bool uiShown = false;


    // Iteration is based on SBSARs stored in the project file
    for(auto s: sbsars)
    {
        i++;

        // is Archive changed?
        auto SBSARPath = ConvertToRelativePath(s);
        auto SBSARCachedData = oldLibrary[SBSARPath];
        QFileInfo sbsarInfo(m_projectDir + "/" + SBSARPath);

        bool shouldUseCache = false;

        if(SBSARCachedData.isObject())
        {
            std::int64_t ts = SBSARCachedData.toObject()["ts"].toString().toLongLong();
            if(sbsarInfo.lastModified().toSecsSinceEpoch() <= ts)
                shouldUseCache = true;
        }

        if(shouldUseCache)
        {
            // load graphs from cache
            auto graphs = SBSARCachedData.toObject()["graphs"].toArray();
            for(auto graphObject: graphs)
            {
                auto preset = SBSPreset::FromJson(graphObject.toObject(), true);
                if(preset)
                {
                    // we have relativeSBSARPath and pkgurl
                    m_library.append(preset);
                    QJsonObject sbsar = newLibrary[SBSARPath].toObject();
                    auto graphs = sbsar["graphs"].toArray();
                    graphs.append(preset->toJson());
                    sbsar["graphs"] = graphs;
                    sbsar["ts"] = SBSARCachedData.toObject()["ts"].toString();
                    newLibrary[SBSARPath] = sbsar;
                }
            }

        }
        else
        {
            pd.show();
            pd.setText("Loading Substance Archive\n" + QFileInfo(s).fileName());
            pd.setProgress(i);
            uiShown = true;

            auto sbsar = new SBSARLoader(s);
            for(auto preset: sbsar->GetPresets())
            {
                // we have relativeSBSARPath and pkgurl
                m_library.append(preset);
                QJsonObject sbsar = newLibrary[SBSARPath].toObject();
                auto graphs = sbsar["graphs"].toArray();
                graphs.append(preset->toJson());
                sbsar["graphs"] = graphs;
                sbsar["ts"] = QString::number(sbsarInfo.lastModified().toSecsSinceEpoch());
                newLibrary[SBSARPath] = sbsar;
            }
        }

        //qDebug() << s;
        if(uiShown) qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // Assign the new library
    m_projectData["Library"] = newLibrary;

    SaveProject();
}

void SubstanceManagerApp::UpdateLibraryUI()
{
    if(m_projectFilePath == "") return;
    gfInfo("Update Library UI");

    int vscroll = ui->library->verticalScrollBar()->value();

    ui->library->clear();
    for(auto& graph: m_library)
    {
        auto item = new QListWidgetItem();
        item->setSizeHint(QSize(128,128));

        // Hacking the weird "size hint" again
        item->setText("\n" + graph->label + QString(50, ' '));
        //item->setTextColor(Qt::transparent);

        auto preview = new SubstancePresetPreview();
        //preview->SetLabelEnabled(true);
        preview->SetPresetAndRenderPreview(graph);

        ui->library->addItem(item);
        ui->library->setItemWidget(item, preview);
    }

    ui->library->verticalScrollBar()->setValue(vscroll);
}

void SubstanceManagerApp::UpdateCategoryButtons()
{
    for(auto c: categoryButtons) c->deleteLater();
    categoryButtons.clear();

    for(auto& c:m_categories)
    {
        auto cb = new QPushButton();
        categoryButtons.append(cb);
        cb->setParent(ui->Categories);
        cb->setFlat(true);
        cb->setText(c->name);
        cb->setProperty("CategoryButton", qVariantFromValue((void*)c));
        cb->setCheckable(true);
        cb->setFocusPolicy(Qt::NoFocus);
        cb->show();

        if(m_currentCategory == c)cb->setChecked(true);
    }
    ui->allCategories->setChecked(!m_currentCategory);

    CategoriesLayout(ui->Categories->size());
}

void SubstanceManagerApp::UpdatePresetsUI(Category *cat)
{
    //remember the scrollbar
    int scrollPos = ui->mainWorkArea->verticalScrollBar()->value();

    auto widgets = ui->MainWorkAreaWidgetContents->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly);
    for(auto c : widgets)
    {
        //qDebug() << "deleting Area" << c->objectName() << c;
        c->hide();
        c->deleteLater();
    }

    if(cat) BuildCategoryUI(cat);
    else for(auto c: m_categories) BuildCategoryUI(c);

    auto items = ui->MainWorkAreaWidgetContents->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly);
    if(items.size())
    {
        items.last()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    // fix the scrollbar
    ui->mainWorkArea->verticalScrollBar()->setValue(scrollPos);
}

void SubstanceManagerApp::BuildCategoryUI(Category *cat)
{
    // Checking for errors
    cat->CheckPresets();

    QFrame* titleFrame = new QFrame;
    QLabel* title = new QLabel;
    QWidget* content = new QFrame;

    titleFrame->setProperty("CategoryTitle", qVariantFromValue(content) );
    titleFrame->setProperty("CategoryLabel", true );
    titleFrame->setProperty("CategoryName", cat->name);
    titleFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    title->setText(cat->name);
    titleFrame->setLayout(new QHBoxLayout());
    titleFrame->layout()->addWidget(title);
    titleFrame->layout()->setContentsMargins(0,0,0,0);

    // we should have this set on our Content
    content->setAcceptDrops(true);
    content->setProperty("PresetView", true);
    content->setProperty("CategoryName", cat->name);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Fill in content
    if(cat->presets.size() == 0)
    {
        auto label = new QLabel(content);
        label->setProperty("EmptyPresetLabel", true);
        label->setText("Drop presets from Library here");
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        content->setLayout(new QVBoxLayout);
        content->layout()->addWidget(label);
    }
    else
    {
        new FlowLayout(content, 4, 0, 0);
        for(const auto& preset: cat->presets)
        {
            auto item = new SubstancePresetPreview();
            item->setParent(content);
            item->setFixedSize(m_thumbSize);
            item->SetPreviewSize(m_thumbSize);
            item->SetLabelEnabled(true);
            item->SetPresetAndRenderPreview(preset);
            item->setProperty("PresetPreview", true);
            item->setProperty("SBSPreset*", qVariantFromValue((void*)&preset));
            item->setFocusPolicy(Qt::NoFocus);
            content->layout()->addWidget(item);
        }
    }

    ui->MainWorkAreaWidgetContents->layout()->addWidget(titleFrame);
    content->setMinimumHeight(64);
    ui->MainWorkAreaWidgetContents->layout()->addWidget(content);
}

SubstanceManagerApp::WidgetPair SubstanceManagerApp::CreateWidgetForInput(SBSPreset::Input& input, QString value, bool isMulti, bool isTweaked)
{

    /* TYPES:

        Types are not documented but based on my analysis, they are as follows:
        OK 0 - float
        -- 1 - vec2f
        OK 2 - vec3f
        OK 3 - vec4f   (also color)
        OK 4 - int     (also used as bool)
        -- 5 - Image Input (Texture)
        OK 6 - string (default section)
        OK 8 - vec2i?  (for $outputsize)
        vec3i & vec4i = ?
        unknown will fall-back to string
    */

    auto label = new QLabel();
    label->setText(input.uiLabel.isEmpty() ? input.identifier : input.uiLabel);
    label->setAlignment(Qt::AlignCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto font = label->font();
    if(isTweaked) font.setBold(true);
    if(isMulti) font.setItalic(true);
    if(isTweaked || isMulti) label->setFont(font);

    label->setProperty("ResetProperty", input.identifier);

    bool isVisiblePassed = true;
    WidgetPair res;
    res.label = label;
    res.field = nullptr;
    // only basic forms:
    // 1: input["Advanced_parameters_Discolor"]==X
    // 2: input["Advanced_parameters_Discolor"]

    // 3: input["Advanced_parameters_Lichen_Splat"] == true
    //    || input["Advanced_parameters_Discolor"] == true
    //    || input["Advanced_parameters_Moss"] == true
    //    || input["Advanced_parameters_Leak"] == true
    //    || input["Advanced_parameters_Sandy_Surface"] == true
    //    || input["Advanced_parameters_Rust"] == true
    QString cond = input.uiVisibleIf;
    if(!cond.isEmpty())
    {
        // does it have multiple OR chains:
        bool hasOring = cond.contains("||");
        QStringList ors;
        bool orAccum = false;
        if(hasOring) ors = cond.split("||");
        else ors << cond;

        for(const auto& orPart: ors)
        {
            QRegExp case1("^ *input\\[\"([a-zA-Z0-9_]+)\"\\] *([><=!]+) *([0-9truefals]+)");
            QRegExp case2("^ *input\\[\"([a-zA-Z0-9_]+)\"\\] *$");
            if(case1.indexIn(orPart) != -1)
            {
                auto in = case1.cap(1);
                auto op = case1.cap(2);
                auto val = case1.cap(3);
                int ival = val.toInt();
                if(val=="true")ival = 1;
                // in --op--> val ?
                QString invals = getCurrentPresetProperty(in);
                int inval = invals.toInt();
                bool r = false;
                if(op == "==") r = inval == ival;
                else if(op == "!=") r = inval != ival;
                else if(op == "<") r = inval < ival;
                else if(op == ">") r = inval > ival;
                else if(op == "<=") r = inval <= ival;
                else if(op == ">=") r = inval >= ival;
                isVisiblePassed = r;
            }
            else if(case2.indexIn(orPart) != -1)
            {
                auto in = case2.cap(1);
                // is it "true" or nonzero?
                auto value = getCurrentPresetProperty(in);
                isVisiblePassed = (value.toLower() == "true" || value.toInt()!= 0);
            }
            orAccum =  orAccum || isVisiblePassed;
        }
    }


    if(!isVisiblePassed) return WidgetPair();

    switch(input.propertyType)
    {
    case 0: // float: slider
    {
        auto slider = new QSlider();
        slider->setMinimum(0);
        slider->setMaximum(1000);
        slider->setOrientation(Qt::Orientation::Horizontal);
        slider->setValue(value.toDouble()*1000.0);

        auto min = input.uiWidgetOptions.find("min");
        if(min != input.uiWidgetOptions.end()) slider->setMinimum(min.value().toDouble()*1000.0);
        auto max = input.uiWidgetOptions.find("max");
        if(max != input.uiWidgetOptions.end()) slider->setMaximum(max.value().toDouble()*1000.0);
        auto step = input.uiWidgetOptions.find("step");
        if(step  != input.uiWidgetOptions.end()) slider->setSingleStep(step.value().toDouble()*1000.0);

        QDoubleSpinBox* spinbox = new QDoubleSpinBox();
        spinbox->setValue(value.toDouble());

        QObject::connect(spinbox,  SIGNALPTR(void,QDoubleSpinBox,editingFinished), [=] {
            setCurrentPresetProperty(input.identifier, QString::number(spinbox->value()));
            slider->setValue(spinbox->value()*1000.0);
        });
        QObject::connect(slider,  SIGNALPTR(int,QSlider,valueChanged), [=] {
            setCurrentPresetProperty(input.identifier, QString::number(spinbox->value()));
            spinbox->setValue(slider->value()/1000.0);
        } );

        auto field = new QWidget;
        field->setLayout(new QHBoxLayout);
        field->layout()->setContentsMargins(0,0,0,0);
        field->layout()->addWidget(slider);
        field->layout()->addWidget(spinbox);

        res.field = (QWidget*) field;

        break;
    }
    case 2: // vec3f, color [no alpha]
    {
        // color
        auto button = new QPushButton;
        auto values = value.split(",");
        QColor color = Qt::white;
        if(values.size()>=3)
        {
            color.setRedF(values[0].toFloat());
            color.setGreenF(values[1].toFloat());
            color.setBlueF(values[2].toFloat());
        }
        QString name = color.name(QColor::NameFormat::HexRgb);
        float luminosity = (color.redF()+color.greenF()+color.blueF())/3.0;
        QString txcolor = luminosity>0.5 ? "black":"white";

        button->setStyleSheet("border:1px solid black; color:"+txcolor+"; background:"+name+";");
        button->setText(value);

        QObject::connect(button, &QPushButton::clicked, [=]{
            QColorDialog d;
            d.setCurrentColor(color);
            if(d.exec())
            {
                QColor c = d.currentColor();
                QString val = QString("%1,%2,%3").arg(c.redF()).arg(c.greenF()).arg(c.blueF());
                float luminosity = (c.redF()+c.greenF()+c.blueF())/3.0;
                QString txcolor = luminosity>0.5 ? "black":"white";
                button->setStyleSheet("border:1px solid black; color:"+txcolor+"; background:"+c.name(QColor::NameFormat::HexRgb)+";");
                button->setText(value);
                setCurrentPresetProperty(input.identifier, val);
            }
        });

        res.field = (QWidget*)button;

        break;
    }
    case 3: // vec4f, color
    {
        // color
        auto button = new QPushButton;
        auto values = value.split(",");
        QColor color;
        if(values.size()>=4)
        {
            color.setRedF(values[0].toFloat());
            color.setGreenF(values[1].toFloat());
            color.setBlueF(values[2].toFloat());
            color.setAlphaF(values[3].toFloat());
        }
        QString name = color.name(QColor::NameFormat::HexRgb);
        float luminosity = (color.redF()+color.greenF()+color.blueF())/3.0;
        QString txcolor = luminosity>0.5 ? "black":"white";

        button->setStyleSheet("border:1px solid black; color:"+txcolor+"; background:"+name+";");
        button->setText(value);

        QObject::connect(button, &QPushButton::clicked, [=]{
            QColorDialog d;
            d.setOption(QColorDialog::ColorDialogOption::ShowAlphaChannel, true);
            d.setCurrentColor(color);
            if(d.exec())
            {
                QColor c = d.currentColor();
                QString val = QString("%1,%2,%3,%4").arg(c.redF()).arg(c.greenF()).arg(c.blueF()).arg(c.alphaF());
                float luminosity = (c.redF()+c.greenF()+c.blueF())/3.0;
                QString txcolor = luminosity>0.5 ? "black":"white";
                button->setStyleSheet("border:1px solid black; color:"+txcolor+"; background:"+c.name(QColor::NameFormat::HexRgb)+";");
                button->setText(value);
                setCurrentPresetProperty(input.identifier, val);
            }
        });

        res.field = (QWidget*)button;

        break;
    }
    case 4: // int
    {
        // can be: int [],
        //         bool [togglebutton],
        //         slider [slider],
        //         combo box [guicombobox]

        auto widget = input.uiWidget;

        if(widget == "togglebutton")
        {
            auto button = new QPushButton;
            button->setCheckable(true);
            button->setChecked(value.toInt());
            button->setText(button->isChecked()?"Enabled":"Disabled");
            QObject::connect(button, &QPushButton::clicked, [=]{
                QString val = button->isChecked()?"1":"0";
                button->setText(button->isChecked()?"Enabled":"Disabled");
                setCurrentPresetProperty(input.identifier, val);
                BuildObjectInspectorUI();
            });
            res.field = button;
        }
        else if(widget == "slider")
        {
            auto slider = new QSlider();
            slider->setMinimum(0);
            slider->setMaximum(100);
            slider->setOrientation(Qt::Orientation::Horizontal);
            slider->setValue(value.toInt());

            auto min = input.uiWidgetOptions.find("min");
            if(min != input.uiWidgetOptions.end()) slider->setMinimum(min.value().toInt());
            auto max = input.uiWidgetOptions.find("max");
            if(max != input.uiWidgetOptions.end()) slider->setMaximum(max.value().toInt());
            auto step = input.uiWidgetOptions.find("step");
            if(step  != input.uiWidgetOptions.end()) slider->setSingleStep(step.value().toInt()*1000.0);

            QSpinBox* spinbox = new QSpinBox();
            spinbox->setValue(value.toInt());

            QObject::connect(spinbox, SIGNALPTR(void,QSpinBox,editingFinished), [=] {
                setCurrentPresetProperty(input.identifier, QString::number(spinbox->value()));
                slider->setValue(spinbox->value());
            } );
            QObject::connect(slider, SIGNALPTR(int,QSlider,valueChanged), [=] {
                setCurrentPresetProperty(input.identifier, QString::number(slider->value()));
                spinbox->setValue(slider->value());
            } );

            auto field = new QWidget;
            field->setLayout(new QHBoxLayout);
            field->layout()->setContentsMargins(0,0,0,0);
            field->layout()->addWidget(slider);
            field->layout()->addWidget(spinbox);

            res.field = field;
        }
        else if(widget == "combobox")
        {
            auto menu = new QComboBox();
            menu->setEditable(false);
            for(auto w = input.uiWidgetOptions.begin(); w!= input.uiWidgetOptions.end(); w++)
                menu->addItem(w.value(), w.key());

            QObject::connect(menu, SIGNALPTR(int,QComboBox,currentIndexChanged), [=]{
                setCurrentPresetProperty(input.identifier, menu->itemData(menu->currentIndex()).toString());
            });

            res.field = menu;
        }
        else // int field as in Random Seed
        {
            auto spinbox = new QSpinBox();
            spinbox->setMinimum(INT_MIN);
            spinbox->setMaximum(INT_MAX);
            spinbox->setSingleStep(1);
            QObject::connect(spinbox, SIGNALPTR(void,QSpinBox,editingFinished), [=] {
                setCurrentPresetProperty(input.identifier, QString::number(spinbox->value()));
            });
            res.field = spinbox;
        }

        break;
    }
    case 8: // vec2i
    {       
        auto values = value.split(",");
        if(value.size()<2) values = QStringList() << "0" << "0";

        QSpinBox* spinboxX = new QSpinBox();
        spinboxX->setValue(values[0].toInt());
        QSpinBox* spinboxY = new QSpinBox();
        spinboxY->setValue(values[1].toInt());

        QObject::connect(spinboxX, SIGNALPTR(void,QSpinBox,editingFinished), [=] {
            auto value = getCurrentPresetProperty(input.identifier);
            auto splits = value.split(',');
            if(splits.size()<2) return;
            splits[0] = QString::number(spinboxX->value());
            setCurrentPresetProperty(input.identifier, splits.join(','));
        });

        QObject::connect(spinboxY, SIGNALPTR(void,QSpinBox,editingFinished), [=] {
            auto value = getCurrentPresetProperty(input.identifier);
            auto splits = value.split(',');
            if(splits.size()<2) return;
            splits[1] = QString::number(spinboxX->value());
            setCurrentPresetProperty(input.identifier, splits.join(','));
        });

        auto field = new QWidget;
        field->setLayout(new QHBoxLayout);
        field->layout()->setContentsMargins(0,0,0,0);
        // $outputsize will have some special handling:
        if(input.identifier == "$outputsize")
        {
            int sizes[] = {64, 128, 256, 512, 1024};
            int sizev[] = { 6,   7,   8,   9,   10};
            int i=0;
            for(auto j: sizes)
            {
                auto b = new QToolButton();
                b->setAutoRaise(true);
                b->setText(QString::number(j));
                connect(b, &QToolButton::clicked, [=]{
                    spinboxX->setValue(sizev[i]);
                    spinboxY->setValue(sizev[i]);
                    setCurrentPresetProperty(input.identifier, QString("%1,%2").arg(sizev[i]).arg(sizev[i]));
                });
                i++;
                field->layout()->addWidget(b);
            }
        }
        field->layout()->addWidget(spinboxX);
        field->layout()->addWidget(spinboxY);

        res.field = (QWidget*) field;
        break;
    }
    default: // string
    {
        auto field = new QLineEdit();
        field->setText(value);
        QObject::connect(field, &QLineEdit::textChanged, [=] {
            setCurrentPresetProperty(input.identifier, field->text());
        });
        res.field = (QWidget*) field;
    }
    break;
    }
    return res;
}


void SubstanceManagerApp::BuildObjectInspectorUI()
{
    QSignalBlocker a(ui->inspector);

    // Remove old contents if any
    auto items = ui->inspector->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly);
    for(auto w: items)
    {
        ui->inspector->layout()->removeWidget(w);
        w->hide();
        w->deleteLater();
    }

    if(!m_selectedPresets.size()) return;

    // Will only support multi-edit of presets for same template

    //Commons with same name and type
    QMap<SBSPreset*, int> commons;
    for(auto p: m_selectedPresets) commons[p] ++;

    QMap<QString, QList<WidgetPair>> groupedProperties;

    auto layout = (QFormLayout*) ui->inspector->layout();

    if(commons.size()>1)
    {
        // invalid case
        auto error = new QLabel();
        error->setProperty("Light", true);
        error->setText("You can only edit\n"
                       " multiple presets\n"
                       "if they share same template");
        layout->addRow(error);
    }

    // will use this for presets
    SBSPreset* source = *m_selectedPresets.begin();
    if(!source) return;

    // First the name:
    auto name = new QLabel(); name->setText("Name:");
    auto field = new QLineEdit(); field->setProperty("input","");

    // This lambda here performs the renaming!
    // It must delete the old cache dir
    QObject::connect(field, &QLineEdit::textChanged, [=] {

        if(source->label == field->text()) return;

        QString dir = projectDir() + cacheDirName + "cat-" + source->label;
        SubstancePresetPreview::ClearCacheForDir(dir);
        gfRemoveFolder(dir);
        // i'm unsure it is that so I will debug!

        source->label = field->text();
        UpdatePresetsUI(m_currentCategory);
    });

    layout->addRow(name, field);

    auto inputs = source->GetInputDefinitions();
    for(auto& i: inputs)
    {
        // determine if they all have same value
        QSet<QString> values;
        for(auto v: m_selectedPresets) values.insert(v->getValue(i.identifier));
        QString value = values.size() == 1 ? *values.begin() : "";
        bool isMulti = values.size()>1;
        bool isValueTweaked = false;
        SBSPreset* v = *m_selectedPresets.begin();

        if(!isMulti) field->setText(v->label);
        else field->setEnabled(false);

        if(!isMulti) isValueTweaked = v->isValueTweaked(i.identifier);

        auto pair = CreateWidgetForInput(i, value, isMulti, isValueTweaked);

        if(pair.field != nullptr) groupedProperties[i.uiGroup].append(pair);
    }

    // Now create these groups
    for(auto i = groupedProperties.begin(); i!=groupedProperties.end(); ++i)
    {
        if(!i.key().isEmpty())
        {
            auto glabel = new QLabel();
            glabel->setText(i.key());
            glabel->setProperty("ObjectInspectorGroup", true);
            layout->addRow(glabel);
        }

        for(auto& wp: i.value())
        {
            layout->addRow(wp.label, wp.field);
        }

    }
}

void SubstanceManagerApp::OnPresetRendered(SBSPreset *p)
{
    qDebug() << "OnPresetRendered: " << p->label;

    auto allPrevs = ui->mainWorkArea->findChildren<SubstancePresetPreview*>();
    for(auto& pr: allPrevs)
        if(pr->preset() == p)
            pr->ForceReRender();
}

SubstanceManagerApp::~SubstanceManagerApp()
{
    delete ui;
    ProcessManager::ShutDown();
}

void SubstanceManagerApp::initSAT()
{
    bool first = true;
    gfLog("Checking if Automation Toolkit is installed...");

    // while he fails to feed proper exe (unless he quits)
    while(true)
    {
        QMessageBox::StandardButton res = QMessageBox::No;
        sbsRenderExe = settings->value("sbsrender").toString();

        // Not set or missing file?
        if(sbsRenderExe == "" || !QFile::exists(sbsRenderExe))
        {
            res = QMessageBox::question(nullptr, "Substance Automation Toolkit Location", "To use SBSAR Library Manager you must have Substance Automation Toolkit installed. Do you have it installed?");
        }
        else
        {
            gfLog("Checking if provided executable looks like the real sbsrender (Executing!)...");

            QProcess p(this);
            p.start(sbsRenderExe, QStringList() << "-h");
            p.waitForFinished();
            QString output = p.readAll();
            qDebug() << output;
            if(output.contains("substance archive file"))
            {
                // looks like valid, will trust it blindly.
                return;
            }
            else
            {
                res = QMessageBox::question(nullptr, "Substance Automation Toolkit Location", first
                                            ? "To use SBSAR Library Manager you must have Substance Automation Toolkit installed. Do you have it installed?"
                                            : "Feeded application does not look like the real sbsrender! I expect specific output Try Again?");
                first = false;
            }
        }

        if(res == QMessageBox::Yes)
        {
            QString fn = QFileDialog::getOpenFileName(nullptr, "Select sbsrender.exe from Substance Automation Toolkit",QString(), "sbsrender.exe");
            if(fn.size() == 0) ::exit(1);
            settings->setValue("sbsrender", fn);
        }
        else ::exit(1);
    }

}

void SubstanceManagerApp::init7z()
{
    sbs7zExe = "7zr.exe";
    if(!QFile::exists(sbs7zExe)) sbs7zExe = QDir::currentPath() + "/7zr.exe";
    if(!QFile::exists(sbs7zExe)) sbs7zExe = QApplication::applicationDirPath() + "/7zr.exe";
    if(!QFile::exists(sbs7zExe))
    {
        QMessageBox::critical(nullptr,  "Missing DLL", "Can't find 7zr.exe! Get 7zr.exe from LZMA SDK's bin folder and place it next to the SubstanceManager.exe");
        ::exit(1);
    }
}

void SubstanceManagerApp::NewProject(QString path)
{
    // create if NOT exist otherwise rewrite!
    LoadProject(path, true);
}

void SubstanceManagerApp::LoadProject(QString path, bool createNewProject)
{
    if(path == "") return;
    settings->setValue("LastProject", path);
    CloseProject();
    gfLog("Loading project from " + path);
    m_projectFilePath = path;
    m_projectDir = QFileInfo(path).absolutePath();


    // Do we have substance description cache?
    if(!QFile::exists(m_projectDir+"/.SMCache"))
    {
        QDir d(m_projectDir);
        d.mkdir(".SMCache");
    }

    // Load the project file as JSON:
    m_projectData = gfReadJson(m_projectFilePath);

    // Library is in cache, ReimportSubstance takes care of it to keep in sync

    // But categories and presets are to be manually handled:
    auto cats = m_projectData["Categories"].toArray();
    for(auto cat: cats)
    {
        m_categories.append(new Category);
        auto c = m_categories.last();
        c->name = cat.toObject()["name"].toString();
        auto presets = cat.toObject()["presets"].toArray();
        for(auto p: presets) c->presets.push_back( SBSPreset::FromJson(p.toObject(), false) );
    }

    SaveProject();

    gfLog("Job Done!");
}

void SubstanceManagerApp::CloseProject()
{
    SaveProject();
    m_library.clear();
    m_categories.clear();
    m_currentCategory = nullptr;
    m_selectedPresets.clear();
    UpdateLibraryUI();
    UpdateCategoryButtons();
    UpdatePresetsUI(nullptr);
}

void SubstanceManagerApp::SaveProject()
{
    // Library is already populated by the ReimportSubstances routine,
    // but categories must be reserialized on each save?
    m_projectData["Categories"] = QJsonValue();
    QJsonArray categoriesData;

    // SBSPreset To Json Array
    for(auto& c: m_categories)
    {
       QJsonObject cat;
       QJsonArray presets;
       for(auto p: c->presets) presets.append(p->toJson());
       cat["name"] = c->name;
       cat["presets"] = presets;
       categoriesData.append(cat);
    }

    // Then assign
    m_projectData["Categories"] = categoriesData;
    gfSaveJson(m_projectFilePath, m_projectData);
}

void SubstanceManagerApp::closeEvent(QCloseEvent* e)
{
    // save project
    SaveProject();

    //save state
    settings->setValue("Geometry",saveGeometry());
    settings->setValue("State",saveState());
    settings->sync();
}

void SubstanceManagerApp::showEvent(QShowEvent *)
{
    restoreGeometry(settings->value("Geometry").toByteArray());
    restoreState(settings->value("State").toByteArray());
}

Category *SubstanceManagerApp::GetCategoryByName(QString name)
{
    for(auto c: m_categories)
        if(c->name == name)
            return c;
    return nullptr;
}

void SubstanceManagerApp::on_actionCreate_Variation_triggered()
{

}

void SubstanceManagerApp::on_actionNew_Project_triggered()
{
    QString fn = QFileDialog::getSaveFileName(this, "Create SBSAR Library Manager Project...",QString(), k_formatStrings);
    if(fn!="") NewProject(fn);
}

void SubstanceManagerApp::on_actionOpen_Project_triggered()
{
    QString fn = QFileDialog::getOpenFileName(this, "Open SBSAR Library Manager Project...",QString(), k_formatStrings);
    if(fn!="") LoadProject(fn, false);
}

void SubstanceManagerApp::on_actionSave_Project_triggered()
{
    SaveProject();
}

void SubstanceManagerApp::on_actionThemeDefault_triggered()
{
    qApp->setPalette(defaultPalette);
    qApp->setStyleSheet("");
    qApp->setStyle("windowsvista");
    QString qss = gfReadFile("default.qss");
    qApp->setStyleSheet(qss);
    ui->pbrView->themeChanged();
}

void SubstanceManagerApp::on_actionThemeDark_triggered()
{
    qApp->setStyle("Fusion");
    auto palette = qApp->palette();
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(127, 127, 127));
    palette.setColor(QPalette::Base, QColor(42, 42, 42));
    palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Dark, QColor(35, 35, 35));
    palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(127, 127, 127));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     QColor(127, 127, 127));

    qApp->setPalette(palette);


    QFile qfDarkstyle(QStringLiteral(":/darkstyle/darkstyle.qss"));
    if (qfDarkstyle.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // set stylesheet
        QString qsStylesheet = QString::fromLatin1(qfDarkstyle.readAll());
        qApp->setStyleSheet(qsStylesheet);
        qfDarkstyle.close();
    }

    ui->pbrView->themeChanged();
    /**/
}

void SubstanceManagerApp::on_actionFusion_triggered()
{
    qApp->setPalette(defaultPalette);
    qApp->setStyleSheet("");
    qApp->setStyle("Fusion");
    ui->pbrView->themeChanged();
}

void SubstanceManagerApp::on_actionTest1_triggered()
{
    qApp->setPalette(defaultPalette);
    qApp->setStyle("windowsvista");
    QString qss = gfReadFile("designer.qss");
    qApp->setStyleSheet(qss);
    ui->pbrView->themeChanged();
}

void SubstanceManagerApp::on_actionTest2_triggered()
{
    qApp->setPalette(defaultPalette);
    qApp->setStyle("windowsvista");
    QString qss = gfReadFile("default.qss");
    qApp->setStyleSheet(qss);
    ui->pbrView->themeChanged();
}

void SubstanceManagerApp::on_actionTest3_triggered()
{
    qApp->setPalette(defaultPalette);
    qApp->setStyle("windowsvista");
    QString qss = gfReadFile("gui.qss");
    qApp->setStyleSheet(qss);
    ui->pbrView->themeChanged();
}

QString SubstanceManagerApp::ConvertToRelativePath(QString path)
{
    if(path.startsWith(m_projectDir)) return path.mid(m_projectDir.size()+1);
    return path;
}

void SubstanceManagerApp::on_reimport_clicked()
{
    m_projectData["Library"] = QJsonValue();
    ReimportSubstances();
}

QString SubstanceManagerApp::projectFilePath() const
{
    return m_projectFilePath;
}

void SubstanceManagerApp::setProjectFilePath(const QString &projectFilePath)
{
    m_projectFilePath = projectFilePath;
}

bool SubstanceManagerApp::eventFilter(QObject *watched, QEvent *event)
{
    // case 1: Category Button Double Clicks
    // case 2: DragMove/DragDrop on DropSite
    // case 3: Library Drag Begins
    // case 4: Layouting the Categories

    // DEL key
    if(watched == this && (event->type() == QEvent::KeyRelease || event->type() == QEvent::KeyPress))
    {
        return KeyboardEvents((QKeyEvent*)event);
    }
    else if(watched == ui->library->viewport())
    {
        return LibraryDragEvent(event);
    }
    else if(watched == ui->Categories && event->type() == QEvent::Resize)
    {
        auto ev = (QResizeEvent*) event;
        CategoriesLayout(ev->size());
    }
    else if(watched->property("PresetView").isValid())
    {
        return PresetViewEvent( (QWidget*)watched, event);
    }
    else if(watched->property("CategoryButton").isValid())
    {
        return CategoryButtonEvent( (QPushButton*)watched, event);
    }
    else if(watched->property("PresetPreview").isValid())
    {
        return PresetEvent( (SubstancePresetPreview*) watched, event);
    }
    else if(watched->property("ResetProperty").isValid() && event->type() == QEvent::MouseButtonRelease)
    {
        return InspectorLabelResetEvent( (QLabel*) watched, (QMouseEvent*) event );
    }
    return false;
}

bool SubstanceManagerApp::KeyboardEvents(QKeyEvent *e)
{
    if(e->type() == QEvent::KeyPress && e->key() == Qt::Key_Delete)
    {
        // Clear categories
        for(auto c: m_categories)
            for(auto s: m_selectedPresets)
                c->presets.removeAll(s);

        // free / delete definitions AND delete theirs cache dirs
        for(auto s: m_selectedPresets)
        {
            auto dir = GetPresetCacheRootDir(s, "");
            SubstancePresetPreview::ClearCacheForDir(dir);
            gfRemoveFolder(dir);
            delete s;
        }

        m_selectedPresets.clear();
        UpdatePresetsUI(m_currentCategory);
        return false;
    }
}

bool isDraggingPreset;
bool SubstanceManagerApp::LibraryDragEvent(QEvent *event)
{
    static QPoint dragStartPosition;
    auto e = static_cast<QMouseEvent*>(event);
    if(! (e->buttons() & Qt::LeftButton)) return false;

    if(event->type() == QEvent::MouseButtonPress)
    {
        dragStartPosition = e->pos();
    }
    else if(event->type() == QEvent::MouseMove)
    {
        if ((e->pos() - dragStartPosition).manhattanLength() < QApplication::startDragDistance()) return false;

        auto item = ui->library->itemAt(dragStartPosition);
        if(item)
        {
            auto widget = (SubstancePresetPreview*) ui->library->itemWidget(item);
            QDrag d(this);
            QMimeData* data = new QMimeData();
            intptr_t ptr = (intptr_t) widget->preset();
            QByteArray ptrByteArray =  QByteArray::fromRawData((char*)&ptr, sizeof(intptr_t));
            data->setData("SBSPreset*", ptrByteArray);
            d.setMimeData(data);

            QPixmap pm;
            if(widget->isLoading())
            {
                pm = ui->library->grab( ui->library->visualItemRect( item ).adjusted(4,4,0,0) );
            }
            else
            {
                pm = widget->texture().scaled(100,100,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            }

            QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect;
            e->setColor(pm.scaled(1,1,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).toImage().pixelColor(0,0).darker(130));
            e->setOffset(0,3);
            e->setBlurRadius(20);

            d.setPixmap( applyEffectToImage(pm, e, 40) );
            d.setHotSpot(QPoint(82,82));

            // isDraggingPreset is for paint events
            qDebug() << d.exec();
        }
    }
    return false;
}

bool SubstanceManagerApp::CategoryButtonEvent(QPushButton* button, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress)
    {
        Category* ref = (Category*) button->property("CategoryButton").value<void*>();
        m_currentCategory = ref;
        UpdateCategoryButtons();
        //qDebug() << "\nCategoryButtonEvent - MouseButtonPress";
        UpdatePresetsUI(m_currentCategory);
        return true; // eat event
    }
    if(event->type() == QEvent::MouseButtonDblClick)
    {
        bool ok = true;
        Category* ref = (Category*) button->property("CategoryButton").value<void*>();
        if(!ref) return true;
        QString name = QInputDialog::getText(this, "Edit category", "Category name:", QLineEdit::Normal, ref->name, &ok);
        if(!ok) return true;

        for(auto c: m_categories) if(c!=ref && c->name == name)
        {
            QMessageBox::warning(this,  "Cannot edit category", "Category with same name already exists");
            UpdateCategoryButtons();
            return false;
        }

        ref->name = name;
        UpdateCategoryButtons();
        //qDebug() << "\nCategoryButtonEvent - MouseButtonDblClick";
        UpdatePresetsUI(m_currentCategory);
        return true;
    }
    return false;
}

bool SubstanceManagerApp::PresetEvent(SubstancePresetPreview *watched, QEvent *event)
{
    if(event->type() == QEvent::Paint)
    {
        watched->paintEvent( (QPaintEvent*)event);
        QPainter p(watched);
        if(!watched->preset()->isPresetValid)
        {
            QColor selection = Qt::red;
            selection.setAlpha(50);
            p.fillRect(watched->rect(), QBrush(selection, Qt::SolidPattern));
            p.fillRect(watched->rect(), QBrush(selection, Qt::DiagCrossPattern));
            selection.setAlpha(255);
            p.setPen(QPen(selection, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            //p.drawRect(watched->rect());
            QTextOption opt;
            opt.setAlignment(Qt::AlignCenter);
            auto f = watched->font();
            f.setPixelSize(14);
            f.setBold(true);
            p.setFont(f);
            p.setPen( QPen(QColor(255,255,255), 3) );
            //p.setBrush(Qt::green);
            p.drawText(watched->rect(), watched->preset()->presetInvalidErrorText, opt);
        }

        if(m_selectedPresets.contains(watched->preset()))
        {
            QColor selection = palette().color(QPalette::Highlight);
            selection.setAlpha(100);
            p.fillRect(watched->rect(), QBrush(selection, Qt::SolidPattern));
            selection.setAlpha(255);
            p.setPen(QPen(selection, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawRect(watched->rect());
        }
        return true;
    }


    //qDebug() << watched << event;

    return false;
}

bool SubstanceManagerApp::isKnownPresetPtr(SBSPreset* ptr)
{
    for(auto c: m_categories)
        for(auto p: c->presets)
            if(p == ptr) return true;
    for(auto p: m_library)
        if(p == ptr) return true;
    return false;
}

Category *SubstanceManagerApp::GetPresetCategory(SBSPreset *ptr)
{
    if(ptr->isTemplatePreset) return nullptr;
    for(auto c: m_categories)
        for(auto p: c->presets)
            if(p == ptr) return c;
    return nullptr;
}

QString SubstanceManagerApp::GetCategoryCacheDir(Category *cat)
{
    return projectDir() + cacheDirName
            + (cat? QString("cat-"+cat->name).trimmed() : "library");
}

QString SubstanceManagerApp::GetPresetCacheDir(SBSPreset *preset, QSize size, QString suffix)
{
    auto cat = GetPresetCategory(preset);
    return projectDir() + cacheDirName
            + (cat? QString("cat-"+cat->name).trimmed() : "library") + "/"
            + preset->label.trimmed() + "/"
            + QString("%1x%2").arg(size.width()).arg(size.height())
            + (!suffix.isEmpty() ? "/" + suffix : QString());
}

QString SubstanceManagerApp::GetPresetCacheRootDir(SBSPreset *preset, QString suffix)
{
    auto cat = GetPresetCategory(preset);
    return projectDir() + cacheDirName
            + (cat? QString("cat-"+cat->name).trimmed() : "library") + "/"
            + preset->label
            + (!suffix.isEmpty() ? "/" + suffix : QString());
}



QProcess *SubstanceManagerApp::CreateProcessForSbsPresetRender(SBSPreset *preset, QSize size, QString suffix, bool useRootDir, QStringList* outputsToBake)
{
    //TODO: Settings for EXPORT and PREVIEW and PRODUCTION
    int lgx = 1+(log2(size.width()));
    int lgy = 1+(log2(size.height()));
    QSize outputSize(lgx,lgy); // 8 = 256, 9 = 512, 10 = 1024

    auto p = new QProcess();
    QString dir = useRootDir ? GetPresetCacheRootDir(preset, suffix) :
                               GetPresetCacheDir(preset, size, suffix);
    QDir().mkpath(dir);

    QStringList args;
    QStringList outputsArgs;
    QStringList previewOutputs = {"", "normal", "metallic", "roughness", "ambientOcclusion"};
    if(!outputsToBake) outputsToBake = &previewOutputs;

    for(const auto& outputName: *outputsToBake)
    {
        const SBSPreset::Output* out;
        if(outputName == "") out = preset->GetDefaultPreviewOutput();
        else out = preset->GetChannelOutput(outputName);
        if(out) outputsArgs << "--input-graph-output" << out->identifier;
    }

    // setup what we render and where
    args << "render" << projectDir() + "/" + preset->relativeSBSARPath;
    args << "--output-path" << dir;
    args << "--output-name" << "{outputNodeName}";

    // selected output and graph
    args << "--input-graph" << preset->pkgurl;

    args << outputsArgs;

    //args << "--set-value" <<  QString("$outputsize@%1,%2").arg(outputSize.width()).arg(outputSize.height());

    // Only tweaked inputs
    for(auto i = preset->tweakedValues.begin(); i != preset->tweakedValues.end(); i++)
        args << "--set-value" << QString("%1@%2").arg(i.key()).arg(i.value());


    // engine
    args << "--memory-budget" << "1024";
    args << "--engine" << "d3d10pc";

    p->setArguments(args);
    p->setProgram(sbsRenderExe);
    return p;
}

bool SubstanceManagerApp::PresetViewEvent(QWidget *watched, QEvent *event)
{
    static QPoint lastTrackedMousePos;
    static QPoint dragStartPosition;
    // Selecting items??
    if(event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent* e = (QMouseEvent*) event;
        bool selectedSomething = false;
        m_selectedPresets.clear();
        for(auto w: watched->findChildren<SubstancePresetPreview*>())
            if(w->geometry().contains(e->pos()))
            {
                if(!qApp->keyboardModifiers().testFlag(Qt::ControlModifier)) m_selectedPresets.clear();
                m_selectedPresets.insert(w->preset());
                BuildObjectInspectorUI();
                selectedSomething = true;
                ui->mainSW->setCurrentWidget(ui->pbrView);
                ui->pbrView->SetPreset(w->preset());
                return false;
            }
    }
    else if(event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* e = (QMouseEvent*) event;
        dragStartPosition = e->pos();

        bool selectedSomething = false;
        for(auto w: watched->findChildren<SubstancePresetPreview*>())
            if(w->geometry().contains(e->pos()))
            {
                if(!qApp->keyboardModifiers().testFlag(Qt::ControlModifier)) m_selectedPresets.clear();
                m_selectedPresets.insert(w->preset());
                selectedSomething = true;
            }

        if(!qApp->keyboardModifiers().testFlag(Qt::ControlModifier) && !selectedSomething)
            m_selectedPresets.clear();

        // all must be updated
        for(auto w: findChildren<SubstancePresetPreview*>()) w->update();
        return false;
    }
    else if(event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* e = (QMouseEvent*) event;
        if(dragStartPosition == e->pos())
        {
            BuildObjectInspectorUI();
        }
    }
    else if(event->type() == QEvent::MouseMove)
    {
        QMouseEvent* e = (QMouseEvent*) event;
        if(! (e->buttons() & Qt::LeftButton)) return false;

        if ((e->pos() - dragStartPosition).manhattanLength() < QApplication::startDragDistance()) return false;
        // Rearanging presets here - should implement category local rearrange or between-category rearrange
        lastTrackedMousePos = e->pos();

        // Trying to DRAG something
        auto listPresets = watched->findChildren<SubstancePresetPreview*>();
        SubstancePresetPreview* widget = nullptr;
        for(auto w: listPresets)
            if(w->geometry().contains(lastTrackedMousePos))
                widget = w;

        if(widget)
        {
            QDrag d(this);
            QMimeData* data = new QMimeData();
            intptr_t ptr = (intptr_t) widget->preset();
            QByteArray ptrByteArray =  QByteArray::fromRawData((char*)&ptr, sizeof(intptr_t));
            data->setData("SBSPreset*", ptrByteArray);
            d.setMimeData(data);

            QPixmap pm;
            if(widget->isLoading())pm = widget->grab();
            else pm = widget->texture().scaled(100,100,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

            QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect;
            e->setColor(pm.scaled(1,1,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).toImage().pixelColor(0,0).darker(130));
            e->setOffset(0,3);
            e->setBlurRadius(20);

            d.setPixmap( applyEffectToImage(pm, e, 40) );
            d.setHotSpot(QPoint(82,82));

            // isDraggingPreset is for paint events
            qDebug() << d.exec();
            BuildObjectInspectorUI();
        }
    }

    // Drag&Drop
    else if(event->type() == QEvent::DragEnter)
    {
        auto e = (QDragEnterEvent*) event;
        SBSPreset* val;
        QByteArray ptrByteArray = e->mimeData()->data("SBSPreset*");
        memcpy(&val, ptrByteArray.data(), sizeof(intptr_t));
        if(isKnownPresetPtr(val))
        {
            e->accept();
            isDraggingPreset = true;
            watched->update();
        }
        else e->ignore();
    }
    else if(event->type() == QEvent::DragMove)
    {
        auto e = (QDragMoveEvent*) event;
        SBSPreset* val;
        QByteArray ptrByteArray = e->mimeData()->data("SBSPreset*");
        memcpy(&val, ptrByteArray.data(), sizeof(intptr_t));
        if(isKnownPresetPtr(val))
        {
            lastTrackedMousePos = e->pos();
            e->accept();
            isDraggingPreset = true;
            watched->update();
        }
        else
        {
            e->setAccepted(false);
        }
    }
    else if(event->type() == QEvent::DragLeave)
    {
        isDraggingPreset = false;
        watched->update();
    }
    else if(event->type() == QEvent::Drop)
    {
        auto e = (QDropEvent*) event;
        qDebug() << event;
        isDraggingPreset = false;

        // DROP! Does it comes from Library, or from other place?

        // Get the pointer from the mime data!
        // This pointer is dangerous! It may come from outside, or another instance.
        // We should not dereference it unless it's known!
        // We will check it against every known SBSPreset* in this project
        SBSPreset* val;
        QByteArray ptrByteArray = e->mimeData()->data("SBSPreset*");
        memcpy(&val, ptrByteArray.data(), sizeof(intptr_t));
        if(isKnownPresetPtr(val))
        {
            //e->accept();
            // Create Empty preset referencing this as template
            bool copy = qApp->keyboardModifiers() & Qt::ControlModifier;
            if(val->isTemplatePreset) copy = true;

            int dropIndex = -1; // at end
            auto listPresets = watched->findChildren<SubstancePresetPreview*>();
            SBSPreset* overPreset = nullptr;

            auto sourceCategory = GetPresetCategory(val);
            auto destinationCategory = GetCategoryByName(watched->property("CategoryName").toString());

            for(auto w: listPresets)
                if(w->geometry().contains(lastTrackedMousePos))
                    overPreset = w->preset();

            // Move:
            if(!copy && sourceCategory == destinationCategory)
            {
                int current = sourceCategory->presets.indexOf(val);
                int target = destinationCategory->presets.indexOf(overPreset);
                if(target == -1) target = sourceCategory->presets.length()-1;
                sourceCategory->presets.move(current, target);
            }
            else if(!copy)
            {
                sourceCategory->presets.removeAll(val);
                dropIndex = destinationCategory->presets.indexOf(overPreset);
                if(dropIndex != -1) destinationCategory->presets.insert(dropIndex, val);
                else destinationCategory->presets.append(val);
            }


            if(copy)
            {
                SBSPreset* p = new SBSPreset();
                p->isTemplatePreset = false;               
                p->sourceGraphPtr = val->isTemplatePreset ? val : val->sourceGraphPtr;
                p->relativeSBSARPath = val->relativeSBSARPath;
                p->pkgurl = val->pkgurl;
                p->tweakedValues = val->tweakedValues;

                // destinationCategory is where we place it
                QStringList names;
                for(auto& pr: destinationCategory->presets) names << pr->label;

                p->label = insertUniqueSuffixedNameInList(names, val->label, " %1");

                dropIndex = destinationCategory->presets.indexOf(overPreset);
                if(dropIndex != -1) destinationCategory->presets.insert(dropIndex, p);
                else destinationCategory->presets.append(p);
            }
            UpdatePresetsUI(m_currentCategory);
            return true;
        }
        e->ignore();
        watched->update();
    }
    else if(event->type() == QEvent::Paint)
    {
        if(isDraggingPreset)
        {
            QPainter p(watched);
            p.setPen(QPen(QColor(255,0,0,50), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawRect(watched->rect());
            p.fillRect(watched->rect(), QBrush(QColor(255,0,0,50), Qt::BDiagPattern));

            auto listPresets = watched->findChildren<SubstancePresetPreview*>();
            if(listPresets.empty()) return false;
            SBSPreset* overPreset = nullptr;
            SubstancePresetPreview* overWidget = nullptr;
            for(auto w: listPresets)
                if(w->geometry().contains(lastTrackedMousePos))
                {
                    overPreset = w->preset();
                    overWidget = w;
                }
            // over will place it before given preset
            p.setPen(QPen(QColor(255,0,0,200),5, Qt::SolidLine, Qt::SquareCap));
            auto g = overPreset ? overWidget->geometry() : listPresets.last()->geometry();
            if(overPreset) p.drawLine(g.topLeft(), g.bottomLeft());
            else p.drawLine(g.topRight(), g.bottomRight());
        }
        return false;
    }
    return false;
}

// Some custom layouting math here
bool SubstanceManagerApp::CategoriesLayout(QSize size)
{
    int w = size.width();
    int h = size.height();
    auto allCatsHint = ui->allCategories->sizeHint();
    auto AddCatHint = ui->addCategory->sizeHint();

    // now calculate the layout of each category button + the Add button
    int space = w - (h+allCatsHint.width()) - 8 - AddCatHint.width(); // 8 is padding 2x4

    qDebug() << "space = " << space << "allCats=" << allCatsHint << "addcat=" << AddCatHint;
    if(space < 0)
    {
        //qDebug() << "Error: Layout: Space is N/A! Switching to ComboBox";
        ui->allCategories->setGeometry(0,0, w-h-h, h);
        ui->addCategory->setGeometry(w-h-h, 0, h, h);
        ui->deleteCategory->setGeometry(w-h, 0, h, h);
        ui->addCategory->setStyleSheet("color:transparent");
        delete ui->allCategories->menu();
        QMenu* m = new QMenu();
        m->addAction(QIcon(), "All Categories");
        m->addSeparator();
        for(auto b: m_categories) m->addAction(QIcon(), b->name);
        ui->allCategories->setMenu(m);
        for(auto c: categoryButtons) c->hide();
        ui->LeftLine->hide();
        ui->RightLine->hide();
    }
    else
    {
        ui->allCategories->setGeometry(0,0, allCatsHint.width(), h);
        ui->LeftLine->show();
        ui->LeftLine->setGeometry(allCatsHint.width()+2,0,2,h);

        ui->deleteCategory->setGeometry(w-h, 0, h, h);
        ui->addCategory->setStyleSheet("");
        delete ui->allCategories->menu();

        // Each button + the Add
        int sum = 0;
        for(auto b: categoryButtons) sum += float(QFontMetrics(b->font()).width(b->text()) + 16);
        float f = 1;
        float target = space;
        if(sum > space)
        {
            target = w - (h+allCatsHint.width()) - 8 - h; // Small Add Button
            f = float(sum)/float(target);
            ui->addCategory->setStyleSheet("color:transparent");
            ui->addCategory->setGeometry(w-h-h,0,h,h);
            ui->RightLine->hide();
        }
        else
        {
            ui->addCategory->setStyleSheet("");
        }

        // Layout the CBs
        float t=0;
        for(auto b: categoryButtons)
        {
            float bw = float(QFontMetrics(b->font()).width(b->text()) + 16) /f;
            b->setGeometry(allCatsHint.width()+4+t, 0, bw, h);
            t+=bw;
            b->show();
        }

        // The Add button layout
        if(sum < space)
        {
            ui->addCategory->setGeometry(allCatsHint.width()+4+t, 0, AddCatHint.width(),h);
            ui->RightLine->show();
            ui->RightLine->setGeometry(allCatsHint.width()+4+t, 0, 2, h);
        }

        // Hide the combo box
        ui->categoriesComboBox->hide();

        // Not needed for no-category case
        if(m_categories.size()==0)
        {
            ui->LeftLine->hide();
            ui->RightLine->hide();
        }
    }

    return true;
}

bool SubstanceManagerApp::InspectorLabelResetEvent(QLabel *label, QMouseEvent *event)
{
    if(event->button() == Qt::RightButton)
    {
        unsetCurrentPresetProperty(label->property("ResetProperty").toString());
        BuildObjectInspectorUI();
    }
    return false;
}

QString SubstanceManagerApp::projectDir() const
{
    return m_projectDir;
}

void SubstanceManagerApp::setProjectDir(const QString &projectDir)
{
    m_projectDir = projectDir;
}

void SubstanceManagerApp::on_refresh_clicked()
{
    gfRemoveFolder(m_projectDir + "/.SMCache");
    SubstancePresetPreview::imageCache.clear();
    UpdateLibraryUI();
    //qDebug() << "\non_refresh_clicked";
    UpdatePresetsUI(m_currentCategory);
}

void SubstanceManagerApp::on_addCategory_clicked()
{
    bool ok = true;
    QString name = QInputDialog::getText(this, "New category name", "Enter category name", QLineEdit::Normal, "New Category", &ok);
    if(!ok) return;

    for(auto c: m_categories) if(c->name == name)
    {
        QMessageBox::warning(this,  "Cannot create category", "Category with same name already exists. Switching to it.");
        m_currentCategory = c;
        UpdateCategoryButtons();
        //qDebug() << "\non_addCategory_clicked - cannot create";
        UpdatePresetsUI(m_currentCategory);
        return;
    }

    m_categories.append(new Category);
    m_categories.last()->name = name;

    if(m_currentCategory!=nullptr) m_currentCategory = m_categories.last();
    UpdateCategoryButtons();
    //qDebug() << "\non_addCategory_clicked - created";
    UpdatePresetsUI(m_currentCategory);
}

void SubstanceManagerApp::on_deleteCategory_clicked()
{
    // Forget both in-RAM cache and on-disk cache for this
    if(m_currentCategory)
    {
        QString dir = GetCategoryCacheDir(m_currentCategory);
        SubstancePresetPreview::ClearCacheForDir(dir);
        gfRemoveFolder(dir); // GET AWAY, FOLDER!
    }

    // then delete it and forget in in whole :)
    m_categories.removeAll(m_currentCategory);
    delete m_currentCategory;
    m_currentCategory = nullptr;
    UpdateCategoryButtons();
    UpdatePresetsUI(nullptr);
}

Category::~Category()
{
    qDeleteAll(presets);
}

void Category::CheckPresets()
{
    for(auto p: presets) p->isPresetValid = true;

    for(auto p: presets)
    if(p->isPresetSourceValid)
    {
        // name valid? First, this
        p->label = p->label.trimmed();

        QRegExp rx("[\\/<>|\":\?*]");
        if(p->label.contains(rx))
        {
            p->isPresetValid = false;
            p->presetInvalidErrorText = "Can't contain\n\\ / < > | \" : ? *";
        }


        // name duplications:
        auto named = GetPresetsByName(p->label);
        if(named.size()>1)
        {
            p->isPresetValid = false;
            p->presetInvalidErrorText = "Duplicated\nname";
        }
    }
}

QList<SBSPreset*> Category::GetPresetsByName(QString name)
{
    QList<SBSPreset*> res;
    for(auto p: presets)
        if(p->label == name) res.append(p);
    return res;
}

void SubstanceManagerApp::on_sz64_clicked()
{
    m_thumbSize = QSize(64,64);
    UpdatePresetsUI(m_currentCategory);
}

void SubstanceManagerApp::on_sz100_clicked()
{
    m_thumbSize = QSize(100,100);
    UpdatePresetsUI(m_currentCategory);
}

void SubstanceManagerApp::on_sz256_clicked()
{
    m_thumbSize = QSize(256,256);
    UpdatePresetsUI(m_currentCategory);
}

void SubstanceManagerApp::setCurrentPresetProperty(QString property, QString value)
{
    for(auto p: m_selectedPresets)
    {
        bool isChanged = p->tweakValue(property, value);
        if(isChanged)
        {
            p->needsRerender = true;
            ProcessManager::SchedulePresetRender(p, ui->pbrView->isVisible()? ui->pbrView->m_textureSize : m_thumbSize, "", true);
        }
    }
}

void SubstanceManagerApp::unsetCurrentPresetProperty(QString property)
{
    for(auto p: m_selectedPresets)
    {
        bool isChanged = p->resetValue(property);
        if(isChanged)
        {
            p->needsRerender = true;
            ProcessManager::SchedulePresetRender(p, ui->pbrView->isVisible()? ui->pbrView->m_textureSize : m_thumbSize, "", true);
        }
    }
}

QString SubstanceManagerApp::getCurrentPresetProperty(QString property)
{
    QSet<QString> values;
    for(auto v: m_selectedPresets) values.insert(v->getValue(property));
    QString value = values.size() == 1 ? *values.begin() : "";
    bool isMulti = values.size()>1;
    if(!isMulti) return *values.begin();

    // This will for sure generate issues!! TODO
    return "[multi]";
}

void SubstanceManagerApp::on_lineEdit_textChanged(const QString &s)
{
    for(int i=0; i<ui->library->count(); i++)
    {
        auto item = ui->library->item(i);
        if(s.isEmpty()) item->setHidden(false);
        else item->setHidden(!item->text().toLower().contains(s.toLower()));
    }
}

void SubstanceManagerApp::on_importVisible_clicked()
{
    if(m_currentCategory!=nullptr)
    {
        QString searchText = ui->lineEdit->text();

        for(int i=0; i<ui->library->count(); i++)
        {
            bool import = false;
            auto item = ui->library->item(i);
            if(searchText.isEmpty()) import = true;
            else import = item->text().toLower().contains(searchText.toLower());
            SubstancePresetPreview* preview = (SubstancePresetPreview*) ui->library->itemWidget(item);
            SBSPreset* val = preview->preset();

            if(import)
            {
                // Create Preset
                SBSPreset* p = new SBSPreset();
                p->isTemplatePreset = false;
                p->sourceGraphPtr = val;
                p->relativeSBSARPath = val->relativeSBSARPath;
                p->pkgurl = val->pkgurl;
                p->tweakedValues = val->tweakedValues;
                QStringList names;
                for(auto& pr: m_currentCategory->presets) names << pr->label;
                p->label = insertUniqueSuffixedNameInList(names, val->label, " %1");
                m_currentCategory->presets.append(p);
            }
        }
        UpdatePresetsUI(m_currentCategory);
    }
}

void SubstanceManagerApp::on_actionSettings_triggered()
{
    auto settings = new ProjectSettings(this);

    // do not load invalid settings
    if(m_projectData["settings"].isObject())
    {
        settings->setConfig(ProjectConfig::fromJson(m_projectData["settings"].toObject()));
    }

    if(settings->exec() == QDialog::Accepted)
    {
        m_projectData["settings"] = settings->getConfigJson();
        SaveProject();
    }
}

bool SubstanceManagerApp::CheckProjectSettings()
{
    if(!m_projectData["settings"].isObject())
    {
        on_actionSettings_triggered();
    }
    else return true;
}

QImage SubstanceManagerApp::GetDefaultImageForChannel(QString channel)
{
    // default of any map is black image, except
    QColor black = Qt::black;
    QColor normal(128,128,255);
    QColor half(128,128,128);
    QColor white(Qt::white);
    QColor res;
    if(channel == "normal") res = normal;
    else if(channel == "height") res = half;
    else if(channel == "transparency") res = white;
    else res = black;

    QImage i(1, 1, QImage::Format_RGBA8888);
    i.setPixelColor(0,0, res);
    return i;
}

struct CompletedPresetRender
{
    SBSPreset* preset;
    QJsonObject descriptor;
};

void SubstanceManagerApp::BakePresets(QQueue<SBSPreset *> presets)
{
    ProgressDialog pd;
    std::atomic_size_t progress = 0;
    QQueue<CompletedPresetRender> completedPresets;
    std::mutex completedPresetsMutex;
    int numPresets = presets.count();
    ProjectConfig pc = ProjectConfig::fromJson(m_projectData["settings"].toObject());

    // thread routine
    auto threadRoutine = [&completedPresets, &completedPresetsMutex, &pc, this](QQueue<SBSPreset*> q) -> void
    {
        QStringList neededMaps;

        // Don't request more than we need for the actual job
        for(const auto& cd: pc.images)
            for(const auto& channel: cd.channels)
                if(!neededMaps.contains(channel.outputName))
                    neededMaps.append(channel.outputName);

        for(const auto& p: q)
        {
            // First we ask SBSRender to render everything it can including cache
            QString dir = SubstanceManagerApp::instance->GetPresetCacheRootDir(p, "sbsout");
            bool cached = true;
            QHash<QString, QImage> images;
            CompletedPresetRender result;
            result.preset = p;
            auto cat = GetPresetCategory(p);
            result.descriptor["category"] = cat->name;
            result.descriptor["preset"] = p->label;
            QJsonObject mapsObject;


            auto TextureMapsLoader = [&](bool breakOnCacheMiss, bool useCache){
                for(auto channelName: neededMaps)
                {
                    auto out = p->GetChannelOutput(channelName);
                    if(out)
                    {
                        QString fn = dir + "/" + out->identifier + ".png";
                        QImage img = SubstancePresetPreview::LoadImageThroughCache(fn, useCache);
                        if(breakOnCacheMiss && img.isNull()) { cached = false; break; }
                        images[channelName] = img;
                    }
                    // else no such output in SBSAR, we will use default output
                    else
                    {
                        images[channelName] = GetDefaultImageForChannel(channelName);
                    }
                }
            };

            if(pc.useCacheWhenBaking)
                TextureMapsLoader(true, true);
            else cached = false;

            // Ask SBSRender to generate them and reload them
            if(!cached)
            {
                qDebug() << p->label << " NO CACHE, RENDERING VIA SBSRENDER";
                images.clear(); //force clear previously loaded images if any
                auto process = CreateProcessForSbsPresetRender(p, QSize(), "sbsout", true, &neededMaps);
                auto args = process->arguments();
                args.append(pc.extraSbsRenderArgs.split(" "));
                process->setArguments(args);
                process->start();
                process->waitForFinished(-1);

                // Now we should for sure have the maps
                TextureMapsLoader(false, false);
            }
            // else they are already loaded, proceed!
            else qDebug() << p->label << " EVERYTHING IS IN CACHE";

            // Maps are loaded in images: key => image

            // All maps should be same size, take largest
            QSet<QString> diffSizes;
            QSize largest = QSize();
            for(const auto& i: images) diffSizes.insert( QString("%0x%1").arg(i.size().width()).arg(i.size().height()));
            bool hadDifferentSizes = diffSizes.size()>1;
            if(hadDifferentSizes)
            {
                for(const auto& i: images)
                {
                    if(!largest.isValid()) largest = i.size();
                    else if(i.size().width() > largest.width() || i.size().height() > largest.height())
                    {
                        largest = i.size();
                    }
                }
                for(auto& i: images) if(i.size()!=largest) i = i.scaled(largest);
            }
            else largest = images.begin().value().size();

            // Convert to RGBA8888
            for(auto& i: images) i = i.convertToFormat(QImage::Format_RGBA8888);

            // input images are ready to be channel-swizzled into output images
            QHash<QString, QImage> outputImages;

            // For each image definition in ProjectConfig,
            // create the channel swizzled images
            for(const auto& cd: pc.images)
            {
                if(cd.channels.size() > 1)
                {
                    // we'll need to channel-swizzle using the masks
                    QImage img(largest, QImage::Format_RGBA8888);
                    uint32_t* outPixelData = (uint32_t*) img.bits();
                    memset(outPixelData, 0, sizeof(uint32_t)*largest.width() * largest.height());

                    // now swizzle each cahnnel def into the data
                    for(auto& cdef: cd.channels)
                    {
                        const uint32_t* src = (const uint32_t*) images[cdef.outputName].constBits();

                        if(cdef.mask == ChannelMask::RGB)
                        {
                            // basic bit-swizzling
                            for(int i=0; i<largest.width() * largest.height(); i++) outPixelData[i] |= (src[i] & cdef.mask);
                        }
                        else
                        {
                            // Sadly the value here... needs th4e average, but we'll just copy the R to A!
                            uint32_t s;
                            uchar* ptr = (uchar*)&s;
                            for(int i=0; i<largest.width() * largest.height(); i++)
                            {
                                s = src[i];
                                ptr[3] = ptr[0];
                                outPixelData[i] |= (s & cdef.mask);
                            }
                        }
                    }
                    outputImages[cd.name] = img;
                }
                else
                {
                    //only one channel means it's full RGBA, just take it from inputs
                    outputImages[cd.name] = images[cd.channels[0].outputName];
                }
            }

            // Now, save the output images to the output dir
            QDir().mkpath(pc.exportDir + "/" + cat->name);

            for(auto i = outputImages.begin(); i != outputImages.end(); ++i)
            {
                QString pfn = cat->name + "/" + p->label + "-" + i.key() + ".png";
                mapsObject[i.key()] = pfn;
                i.value().save(pc.exportDir + "/" + pfn);
            }

            // Complete the maps object
            result.descriptor["maps"] = mapsObject;

            // Push it back
            MutexLocker locker(completedPresetsMutex);
            completedPresets.push_back(result);
        }
    };


    pd.setWindowFlags(Qt::Dialog);
    pd.setWindowTitle("Baking SBSAR Presets");
    pd.setText("Baking presets");

    QJsonArray log;

    int ncores = QThread::idealThreadCount();
    QVector< QQueue<SBSPreset*> > schedules;
    schedules.resize(ncores);
    int i = 0;
    while(presets.size())
    {
        schedules[i%ncores].append(presets.front());
        presets.pop_front();
        i++;
    }

    QList<std::thread*> pool;
    for(const auto& s: schedules)
        if(s.count()>0)
            pool.push_back( new std::thread(threadRoutine, s) );

    // Show the PD
    pd.setMax(numPresets);
    pd.setProgress(0);
    pd.show();

    // wait for them to complete
    while(progress < numPresets)
    {
        // need to handle updates

        // Is there completed preset?
        CompletedPresetRender completedPreset;
        bool hasCompletedPreset = false;
        {
            MutexLocker locker(completedPresetsMutex);
            if(completedPresets.size())
            {
                completedPreset = completedPresets.front();
                completedPresets.pop_front();
                hasCompletedPreset = true;
            }
        }
        if(hasCompletedPreset)
        {
            progress++;
            pd.setProgress(progress);
            pd.setText(completedPreset.preset->label);
            log.append(completedPreset.descriptor);
        }
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // Save the log in Out Dir
    QString logfn = pc.exportDir + "/last-baked.json";
    gfSaveJson(logfn, log);

    // join the threads before deleting them
    for(const auto& p: pool) p->join();
    for(const auto& p: pool) delete p;

    // Now just invoke the user-specified app
    QString wd = pc.appToRunAfterExportWorkingDir;

    QList< QPair<QString, QString> > replacements = {
        {"%SMDIR%", QDir::currentPath()},
        {"%CD%", QDir::currentPath()},
        {"%OUTPUTDIR%", pc.exportDir},
        {"%EXPORTDIR%", pc.exportDir},
        {"%ED%", pc.exportDir},
        {"%OD%", pc.exportDir},
        {"%LOG%", logfn}
    };

    for(const auto& r: replacements) wd.replace(r.first, r.second);

    pd.setText("Executing after-export program...");

    QString cmdline = pc.appToRunAfterExport;
    QStringList args = cmdline.split(" ");
    for(auto& a: args)
        for(auto r:replacements)
            a.replace(r.first, r.second);

    args.prepend("/c");
    for(auto a: args) args << a;

    QProcess p;
    p.setProgram("cmd");
    p.setArguments(args);
    p.setWorkingDirectory(wd);
    p.start();
    while(p.state() != QProcess::NotRunning)
    {
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    QString s = p.readAllStandardOutput() + p.readAllStandardError();
    if(s.size()>0)
    QMessageBox::information(this, "Output", s);
}

void SubstanceManagerApp::on_actionBake_Selected_Presets_triggered()
{
    if (!CheckProjectSettings()) return;
    // Bake only the selected

    QQueue<SBSPreset*> presets;
    for(auto p: m_selectedPresets) presets.append(p);

    BakePresets(presets);
}

void SubstanceManagerApp::on_actionBake_All_Presets_triggered()
{
    if (!CheckProjectSettings()) return;
    // Bake all based on Project Settings
    QQueue<SBSPreset*> presets;

    ProjectConfig pc = ProjectConfig::fromJson(m_projectData["settings"].toObject());
    if(pc.bakeAllIsOnlyCurrent)
    {
        if(m_currentCategory == nullptr)
        {
            for(auto cat: m_categories)
                for(auto p: cat->presets)
                    presets.append(p);
        }
        else for(auto p: m_currentCategory->presets) presets.append(p);
    }
    else
    {
        for(auto cat: m_categories)
            for(auto p: cat->presets)
                presets.append(p);
    }

    BakePresets(presets);
}
