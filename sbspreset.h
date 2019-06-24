#ifndef SBSPRESET_H
#define SBSPRESET_H

#include <QList>
#include <QSize>
#include <cstdint>
#include <qstring.h>

// One SBSARLoader will generate multiple SBSPresets
class SBSPreset
{
public:
    SBSPreset();

    /* About this class:

    If template, these will contain the full substance definition - each property and default value
    Templates will be cached with last modif date so the Library will reimport changed SBSARs
    If NOT template they will contain only the tweaked properties, changes based on base SBSAR Graph

    What is important for a substance graph:
      INPUTS - Properties we tweak
      OUTPUTS - Texture Maps a graph generate
    */

    // The structs for I/O:

    // Input Property
    struct Input
    {
        Input(){uid = 0; propertyType = 0; isModified = false;}

        uint64_t uid;
        QString identifier;

        /* TYPES:

            Types are not documented but based on my analysis, they are as follows:
            0 - float
            1 - vec2f
            2 - vec3f
            3 - vec4f   (also color)
            4 - int     (also used as bool)
            6 - string
            8 - vec2i?  (for $outputsize)
            vec3i & vec4i = ?
            unknown will fall-back to string
        */
        int propertyType;

        /*
          WIDGETS:

            togglebutton:   N/A
            color:          N/A
            slider:         <guislider min="0" max="1" clamp="on" step="0.01"/>
            combobox:       <guicombobox>
                                <guicomboboxitem value="0" text="DirectX"/>
                                <guicomboboxitem value="1" text="OpenGL"/>
                            </guicombobox>
            angle:          <guiangle  min="-1" max="1"/>

            unknown will fall-back to reasonable default
        */
        QString uiWidget, uiLabel, uiVisibleIf, uiGroup;

        // Based on Widget Type, options will be in this map
        // slider: min, max, clamp, step
        // angle: min, max
        // Combobox: key=>value items
        QMap<QString, QString> uiWidgetOptions;

        // Value
        QString value;  // as string!

        // State changes:
        bool isModified;
    };

    // Substance Output Map
    struct Output
    {
        Output(){uid = 0; width = 0; height = 0;}

        uint64_t uid;
        QString identifier;
        int width, height;

        // From OutputUI tag in meta xml
        QString uiLabel, uiGroup, uiVisibleIf;
        QString outChannelName; // outputgui/channels/channel names=????
    };

    // if true this is a library template, otherwise it's a preset
    bool isTemplatePreset;
    // relativeSBSARPath and pkgurl are identification of the source graph in case of non-template preset

    // if a preset, the pointer MUST be set to a template preset and the string is used to find the pointer
    // rebuilt from relativeSBSARPath and pkgurl if !isTemplatePreset/
    // if isTemplatePreset must be nullptr
    SBSPreset* sourceGraphPtr;

    void ResolveSource();
    bool isPresetSourceValid; // invalid if source is not resolved

    // Preset is invalid if it's name is duplicate of same name from this category
    bool isPresetValid;
    QString presetInvalidErrorText;
    bool needsRerender;


    // Each graph have inputs and outputs...
    QList<Output> outputs;
    QList<Input> inputs;
    QList<Input> GetInputDefinitions();
    QMap<QString, QString> tweakedValues;

    QString getValue(QString in);
    bool isValueTweaked(QString in);
    bool tweakValue(QString in, QString val);
    bool resetValue(QString in);

    // ...as well as other Manager-specific attributes
    QString relativeSBSARPath;  // Relative SBSAR Path
    QString pkgurl;             // The package URL of the Substance
    QString label;              // For presets the preset label, otherwise the graph label
    // SM Project must not allow duplicated labels in a Manager Group!

    // Normal Presets
    void PresetFromCache(QJsonObject cache);
    void PresetFromSBSPRS(QString xml);

    // Template Presets
    static SBSPreset* TemplateFromCache(QJsonObject cache);
    static QList<SBSPreset*> TemplatePresetsFromSBSARMeta(QString xml);

    // Serialization
    QJsonObject toJson();
    static SBSPreset* FromJson(QJsonObject object, bool isTemplatePreset);

    // Computation ease
    const Output* GetDefaultPreviewOutput();
    const Output* GetNormalOutput();
    const Output* GetChannelOutput(QString semantic);
    const QList<Output> GetOutputs();
};

#endif // SBSPRESET_H
