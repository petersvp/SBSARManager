#ifndef SUBSTANCEMANAGERAPP_H
#define SUBSTANCEMANAGERAPP_H

#include "sbsarloader.h"

#include <QDomDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTextCharFormat>

class SubstancePresetPreview;

namespace Ui {
class SubstanceManagerApp;
}

const QString k_formatStrings = "SBSAR Library Manager Project (*.smproj);;All files (*)";

struct Category
{
    QString name;
    QList<SBSPreset*> presets;
    // Later: Add per-category settings
    ~Category();

    void CheckPresets();
    QList<SBSPreset*> GetPresetsByName(QString name);
};

class SubstanceManagerApp : public QMainWindow
{
    Q_OBJECT

public:
    static SubstanceManagerApp* instance;
    explicit SubstanceManagerApp(QWidget *parent = nullptr);
    ~SubstanceManagerApp();

    // init apps
    void initSAT();
    void init7z();

    // Projects:
    void NewProject(QString path);
    void LoadProject(QString path, bool createIfNotExist);
    void CloseProject();
    void SaveProject();

    void closeEvent(QCloseEvent *e);
    void showEvent(QShowEvent *);

    QSettings* settings;

    // Workflow/project/the like
    QList<SBSPreset*> m_library;
    QList<Category*> m_categories;
    Category* GetCategoryByName(QString name);
    bool isKnownPresetPtr(SBSPreset* ptr);
    Category* GetPresetCategory(SBSPreset* ptr);

    // EXE Tools
    static QString sbsRenderExe;
    static QString sbs7zExe;

    QString GetCategoryCacheDir(Category* cat);
    QString GetPresetCacheDir(SBSPreset* preset, QSize size, QString suffix = "");
    QString GetPresetCacheRootDir(SBSPreset* preset, QString suffix);

    QProcess* CreateProcessForSbsPresetRender(SBSPreset* preset, QSize size, QString suffix = "", bool useRootDir = false, QStringList *outputsToBake = nullptr);

    QString ConvertToRelativePath(QString path);

    QString projectDir() const;
    void setProjectDir(const QString &projectDir);

    QString projectFilePath() const;
    void setProjectFilePath(const QString &projectFilePath);

    // Event Filtering
    bool eventFilter(QObject *watched, QEvent *event);
    bool KeyboardEvents(QKeyEvent* event);
    bool LibraryDragEvent(QEvent* event);
    bool CategoryButtonEvent(QPushButton* button, QEvent* event);
    bool PresetEvent(SubstancePresetPreview* watched, QEvent* event);
    bool PresetViewEvent(QWidget* watched, QEvent* event);
    bool CategoriesLayout(QSize size);
    bool InspectorLabelResetEvent(QLabel* label, QMouseEvent* event);
    QList<QPushButton*> categoryButtons;

public slots:
    //void OnTextLogged(QString text, QTextCharFormat format);
    void ReimportSubstances();
    void UpdateLibraryUI();
    void UpdateCategoryButtons();
    void UpdatePresetsUI(Category* cat);
    void BuildCategoryUI(Category* cat);
    void BuildObjectInspectorUI();
    void OnPresetRendered(SBSPreset* p);

private slots:
    void on_actionCreate_Variation_triggered();
    void on_actionNew_Project_triggered();
    void on_actionOpen_Project_triggered();
    void on_actionSave_Project_triggered();
    void on_actionThemeDefault_triggered();
    void on_actionThemeDark_triggered();
    void on_actionFusion_triggered();
    void on_actionTest1_triggered();
    void on_actionTest2_triggered();
    void on_actionTest3_triggered();
    void on_reimport_clicked();
    void on_refresh_clicked();
    void on_addCategory_clicked();
    void on_deleteCategory_clicked();
    void on_sz64_clicked();
    void on_sz100_clicked();
    void on_sz256_clicked();
    void on_lineEdit_textChanged(const QString &arg1);
    void on_importVisible_clicked();
    void on_actionSettings_triggered();
    void on_actionBake_Selected_Presets_triggered();
    void on_actionBake_All_Presets_triggered();

private:
    QPalette defaultPalette;
    Ui::SubstanceManagerApp *ui;
    QString m_projectFilePath;
    QString m_projectDir;
    QJsonObject m_projectData;
    Category* m_currentCategory;

    // For now, only single preset edit at a time
    QSet<SBSPreset*> m_selectedPresets;

    QSize m_thumbSize;
    int presetQualityFactor;

    struct WidgetPair
    {
        QWidget *label, *field;
    };
    WidgetPair CreateWidgetForInput(SBSPreset::Input& input, QString value, bool isMulti, bool isTweaked);
    void setCurrentPresetProperty(QString property, QString value);
    void unsetCurrentPresetProperty(QString property);
    QString getCurrentPresetProperty(QString property);
    bool CheckProjectSettings();
    QImage GetDefaultImageForChannel(QString channel);
    void BakePresets(QQueue<SBSPreset*> presets);
};

#endif // SUBSTANCEMANAGERAPP_H
