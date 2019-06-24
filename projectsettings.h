#ifndef PROJECTSETTINGS_H
#define PROJECTSETTINGS_H

#include <QDialog>

namespace Ui {
class ProjectSettings;
}

// ChannelMask
enum ChannelMask : uint32_t {
    None = 0,
    Red =   0x000000FF,
    Green = 0x0000FF00,
    Blue =  0x00FF0000,
    Alpha = 0xFF000000,
    RGB = Red|Green|Blue,
    RGBA = Red|Green|Blue|Alpha
};

struct ChannelDefinitionElement
{
    QString outputName;
    QColor constant;
    uint32_t mask = ChannelMask::RGBA;
};

// Each exported image have channel settings where does it come from
struct ExportedImageDefinition
{
    QString name;
    QList<ChannelDefinitionElement> channels;
};

// Everything about presets
struct ProjectConfig
{
    QString rawExportMapText;
    QList<ExportedImageDefinition> images;
    QString exportDir;

    QString appToRunAfterExport;
    QString appToRunAfterExportWorkingDir;

    QString extraSbsRenderArgs;

    bool deduplicateImagesByHashing, useGlobalHashing;
    bool bakeAllIsOnlyCurrent, useCacheWhenBaking;

    QJsonObject toJson();
    static ProjectConfig fromJson(QJsonObject json);

    // The Maps config
    bool parseExportMapText();
    QString errorExportMapText;
    bool isExportMapTextValid = false;
};

class ProjectSettings : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectSettings(QWidget *parent = nullptr);
    ~ProjectSettings();

    bool CheckTextureConfigForErrors();
    ProjectConfig& getConfig();
    QJsonObject getConfigJson();
    void setConfig(ProjectConfig config);

private slots:
    void on_buttonBox_accepted();

    void on_textureConfig_textChanged();

    void on_browseExportDir_clicked();

    void on_AfterExportProgramBrowse_clicked();

    void on_AfterExportWorkingDirBrowse_clicked();

    void on_LoadMapsConfig_clicked();

private:
    Ui::ProjectSettings *ui;
    ProjectConfig config;
};

#endif // PROJECTSETTINGS_H
