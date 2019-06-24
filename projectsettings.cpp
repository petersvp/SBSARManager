#include "projectsettings.h"
#include "ui_projectsettings.h"
#include "globals.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonObject>

ProjectSettings::ProjectSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProjectSettings)
{
    ui->setupUi(this);

    //not ready yet
    ui->deduplication->hide();
}

ProjectSettings::~ProjectSettings()
{
    delete ui;
}

bool ProjectSettings::CheckTextureConfigForErrors()
{
    ProjectConfig pc;
    pc.rawExportMapText = ui->textureConfig->toPlainText();
    if(pc.parseExportMapText())
    {
        ui->TextureConfigErrors->setText("<b>Errors: </b> None");
        ui->TextureConfigErrors->setStyleSheet("color:#080;border:1px solid #080; border-radius: 4px; padding: 2px; background:#eFe;");
    }
    else
    {
        ui->TextureConfigErrors->setText("<b>Error: </b>" + pc.errorExportMapText);
        ui->TextureConfigErrors->setStyleSheet("color:#800;border:1px solid #800; border-radius: 4px; padding: 2px; background:#Fee;");
    }
    return pc.errorExportMapText.isEmpty();
}

ProjectConfig &ProjectSettings::getConfig()
{
    return config;
}

QJsonObject ProjectSettings::getConfigJson()
{
    return config.toJson();
}

void ProjectSettings::setConfig(ProjectConfig conf)
{
    config = conf;
    ui->exportDir->setText(config.exportDir);
    ui->AfterExportProgram->setText(config.appToRunAfterExport);
    ui->AfterExportWorkingDir->setText(config.appToRunAfterExportWorkingDir);
    ui->bakeAllCurrent->setChecked(config.bakeAllIsOnlyCurrent);
    ui->deduplicateImages->setChecked(config.deduplicateImagesByHashing);
    if(config.useGlobalHashing) ui->globalHash->setChecked(true);
    else ui->presetHash->setChecked(true);
    ui->textureConfig->setPlainText(config.rawExportMapText);
    ui->useCacheWhenBaking->setChecked(config.useCacheWhenBaking);
    ui->sbsRenderArgs->setText(config.extraSbsRenderArgs);
}

void ProjectSettings::on_buttonBox_accepted()
{
    // fill the config object
    config.exportDir = ui->exportDir->text();
    config.appToRunAfterExport = ui->AfterExportProgram->text();
    config.appToRunAfterExportWorkingDir = ui->AfterExportWorkingDir->text();
    config.bakeAllIsOnlyCurrent = ui->bakeAllCurrent->isChecked();
    config.deduplicateImagesByHashing = ui->deduplicateImages->isChecked();
    config.useGlobalHashing = ui->globalHash->isChecked();
    config.rawExportMapText = ui->textureConfig->toPlainText();
    config.useCacheWhenBaking = ui->useCacheWhenBaking->isChecked();
    config.extraSbsRenderArgs = ui->sbsRenderArgs->text();
    config.parseExportMapText();
}

void ProjectSettings::on_textureConfig_textChanged()
{
    bool res = CheckTextureConfigForErrors();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(res);
}

QJsonObject ProjectConfig::toJson()
{
    QJsonObject j;
    j["rawExportMapText"] = rawExportMapText;
    j["exportDir"] = exportDir;
    j["appToRunAfterExport"] = appToRunAfterExport;
    j["appToRunAfterExportWorkingDir"] = appToRunAfterExportWorkingDir;
    j["deduplicateImagesByHashing"] = deduplicateImagesByHashing;
    j["useGlobalHashing"] = useGlobalHashing;
    j["bakeAllIsOnlyCurrent"] = bakeAllIsOnlyCurrent;
    j["useCacheWhenBaking"] = useCacheWhenBaking;
    j["extraSbsRenderArgs"] = extraSbsRenderArgs;
    return j;
}

ProjectConfig ProjectConfig::fromJson(QJsonObject o)
{
    ProjectConfig p;
    p.rawExportMapText = o["rawExportMapText"].toString();
    p.exportDir = o["exportDir"].toString();
    p.appToRunAfterExport = o["appToRunAfterExport"].toString();
    p.appToRunAfterExportWorkingDir = o["appToRunAfterExportWorkingDir"].toString();
    p.deduplicateImagesByHashing = o["deduplicateImagesByHashing"].toBool();
    p.useGlobalHashing = o["useGlobalHashing"].toBool();
    p.bakeAllIsOnlyCurrent = o["bakeAllIsOnlyCurrent"].toBool();
    p.useCacheWhenBaking = o["useCacheWhenBaking"].toBool();
    p.extraSbsRenderArgs = o["extraSbsRenderArgs"].toString();
    p.parseExportMapText();
    return p;
}

/*
albedoAlpha: RGB(baseColor) A(transparency)
metalSmooth: RGB(metallic) A(smoothness)
normal: RGBA(normal)
height: RGBA(height)
*/
#define FAIL(S) {isExportMapTextValid=false;errorExportMapText=(S);images.clear();return false;}
bool ProjectConfig::parseExportMapText()
{
    images.clear();
    for(QString l: rawExportMapText.split("\n"))
    {
        // ignore empty lines
        if(l.trimmed().isEmpty()) continue;

        ExportedImageDefinition image;

        QStringList items = l.split(':');
        if(items.count()!= 2)
            FAIL(l+"<br>Cannot parse. Format is like:  name: RGB(output) A(output)  or  name: RGBA(output)");

        QString first = items[0];
        QString second = items[1];
        if(!isSuitableForFileSystem(first))
            FAIL(l+"<br>"+first+" is not a valid filename");

        image.name = first;

        // now the definitions:
        uint32_t fullyDefined(ChannelMask::None);
        auto defs = second.split(" ", QString::SkipEmptyParts);
        for(QString& d: defs)
        {
            d = d.trimmed();
            QRegExp rx("\\b(RGB|RGBA|R|G|B|A)\\b\\(([a-zA-Z0-9_]+)\\)");
            rx.setCaseSensitivity(Qt::CaseInsensitive);

            if(rx.exactMatch(d))
            {
                auto func = rx.cap(1);
                auto arg = rx.cap(2);
                ChannelDefinitionElement cd;
                cd.outputName = arg;
                if(func == "RGBA") cd.mask = ChannelMask::RGBA;
                else if(func == "RGB") cd.mask = ChannelMask::RGB;
                else if(func == "R") cd.mask = ChannelMask::Red;
                else if(func == "G") cd.mask = ChannelMask::Green;
                else if(func == "B") cd.mask = ChannelMask::Blue;
                else if(func == "A") cd.mask = ChannelMask::Alpha;

                if( (cd.mask & fullyDefined) != 0) FAIL(l + "<br>==> A channel was multiply-defined.");
                fullyDefined |= cd.mask;
                image.channels.append(cd);
            }

            else FAIL(l+"<br> ==> "+ d+": Only R, G, B, A, RGB and RGBA functions are available");
        }
        if(fullyDefined != ChannelMask::RGBA) FAIL(l + "<br>==> All channels R,G,B,A must be defined!");
        images.append(image);
    }
    return true;
}
#undef FAIL;

void ProjectSettings::on_browseExportDir_clicked()
{
    auto d = QFileDialog::getExistingDirectory(this, "Browse for output dir", ui->exportDir->text());
    if(!d.isEmpty()) ui->exportDir->setText(d);
}

void ProjectSettings::on_AfterExportProgramBrowse_clicked()
{
    auto d = QFileDialog::getOpenFileName(this, "Select file to execute after export", ui->AfterExportProgram->text());
    d.replace(QDir::currentPath(), "%SMDIR%");
    d.append(" %LOG%");

    if(!d.isEmpty()) ui->AfterExportProgram->setText(d);
}

void ProjectSettings::on_AfterExportWorkingDirBrowse_clicked()
{
    auto d = QFileDialog::getExistingDirectory(this, "Select After-export Program Working Directory", ui->exportDir->text());
    if(!d.isEmpty()) ui->exportDir->setText(d);
}

void ProjectSettings::on_LoadMapsConfig_clicked()
{
    auto d = QFileDialog::getOpenFileName(this, "Load Texture maps config from file", "texturemaps");
    if(QFile::exists(d)) ui->textureConfig->setPlainText(gfReadFile(d));
}
