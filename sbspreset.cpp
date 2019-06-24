#include "sbspreset.h"
#include "logger.h"
#include "substancemanagerapp.h"
#include <QDomDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

SBSPreset::SBSPreset()
{
    isTemplatePreset = false;
    sourceGraphPtr = nullptr;
    isPresetSourceValid = true;
    isPresetValid = true;
    needsRerender = false;
}

void SBSPreset::ResolveSource()
{
    if(isTemplatePreset) return;

    if(!sourceGraphPtr && isPresetSourceValid)
    {
        for(auto t: SubstanceManagerApp::instance->m_library)
        {
            if(t->relativeSBSARPath == relativeSBSARPath && t->pkgurl == pkgurl)
            {
                // setup the pointer marking the preset as valid.
                sourceGraphPtr = t;
                return;
            }
        }
        // otherwise the preset is made invalid
        isPresetSourceValid = false;
    }
}

QList<SBSPreset::Input> SBSPreset::GetInputDefinitions()
{
    if(isTemplatePreset) return inputs;
    else
    {
        ResolveSource();
        if(isPresetSourceValid) return sourceGraphPtr->inputs;
    }
    return QList<SBSPreset::Input>();
}

QList<SBSPreset *> SBSPreset::TemplatePresetsFromSBSARMeta(QString xml)
{
    QList<SBSPreset*> items;
    QDomDocument dom;
    dom.setContent(xml);
    QDomElement sbsdescription = dom.documentElement();
    QDomNodeList graphs = sbsdescription.elementsByTagName("graphs").at(0).toElement().elementsByTagName("graph");
    for(int graphIdx=0; graphIdx<graphs.count(); graphIdx++)
    {
        QDomElement graph = graphs.at(graphIdx).toElement();
        auto preset = new SBSPreset();

        // General
        preset->isTemplatePreset = true;

        // Main
        preset->pkgurl = graph.attribute("pkgurl");
        preset->label = graph.attribute("label");

        gfSuccess( preset->label + ": " + preset->pkgurl);

        // Outputs
        auto outputs = graph.elementsByTagName("outputs").at(0).toElement().elementsByTagName("output");
        for(int outIdx=0; outIdx<outputs.count(); outIdx++)
        {
            QDomElement output = outputs.at(outIdx).toElement();
            Output o;
            o.uid = output.attribute("uid").toLongLong();
            o.identifier = output.attribute("identifier");
            o.width = output.attribute("width").toInt();
            o.height = output.attribute("height").toInt();

            // UI:
            auto outputgui = output.elementsByTagName("outputgui").at(0).toElement();
            o.uiGroup = outputgui.attribute("group");
            o.uiLabel = outputgui.attribute("label");
            o.uiVisibleIf = outputgui.attribute("visibleif");
            o.outChannelName = outputgui.elementsByTagName("channels").at(0).toElement()
                    .elementsByTagName("channel").at(0).toElement().attribute("names");

            //gfWarning("    OUTPUT:  " + o.identifier+": " + o.outChannelName);
            preset->outputs.append(o);
        }

        // Inputs
        auto inputs = graph.elementsByTagName("inputs").at(0).toElement().elementsByTagName("input");
        for(int inIdx=0; inIdx<inputs.count(); inIdx++)
        {
            QDomElement input = inputs.at(inIdx).toElement();
            Input i;
            i.uid = input.attribute("uid").toLongLong();
            i.identifier = input.attribute("identifier");
            i.propertyType = input.attribute("type").toInt();
            i.value = input.attribute("default");

            // UI elements must properly be filled if present
            auto inputgui = input.elementsByTagName("inputgui");
            if(inputgui.count()==1)
            {
                auto it = inputgui.at(0).toElement();
                i.uiWidget = it.attribute("widget");
                i.uiLabel = it.attribute("label");
                i.uiVisibleIf = it.attribute("visibleif");
                i.uiGroup = it.attribute("group");

                // Widget-specific options
                if(i.uiWidget == "slider")
                {
                    auto guislider = it.elementsByTagName("guislider").at(0).toElement();
                    i.uiWidgetOptions["min"] = guislider.attribute("min");
                    i.uiWidgetOptions["max"] = guislider.attribute("max");
                    i.uiWidgetOptions["step"] = guislider.attribute("step");
                    i.uiWidgetOptions["clamp"] = guislider.attribute("clamp");
                }
                else if(i.uiWidget == "combobox")
                {
                    auto items = it.elementsByTagName("guicombobox").at(0).toElement().elementsByTagName("guicomboboxitem");
                    for(int id=0; id<items.count();id++)
                    {
                        auto item = items.at(id).toElement();
                        i.uiWidgetOptions[item.attribute("value")] = item.attribute("text");
                    }

                }
                else if(i.uiWidget == "angle")
                {
                    auto guiangle = it.elementsByTagName("guiangle").at(0).toElement();
                    i.uiWidgetOptions["min"] = guiangle.attribute("min");
                    i.uiWidgetOptions["max"] = guiangle.attribute("max");
                }
                // TODO if Allegorithmic adds new Widget Specific options
            }
            //gfInfo("    INPUT:  " + i.identifier+": " + i.value);
            preset->inputs.append(i);
        }

        items.push_back(preset);
    }
    return items;
}

QJsonObject SBSPreset::toJson()
{
    QJsonObject j;

    // Main data
    j["pkgurl"] = pkgurl;
    j["label"] = label;
    //j["isTemplatePreset"] = isTemplatePreset;
    j["relativeSBSARPath"] = relativeSBSARPath;

    QJsonArray jin, jout;

    if(isTemplatePreset)
    {
        for(auto i: inputs)
        {
            QJsonObject j;
            j["identifier"] = i.identifier;
            j["uid"] = QString::number(i.uid);
            j["type"] = i.propertyType;
            j["value"] = i.value;
            j["ui-widget"] = i.uiWidget;
            j["ui-label"] = i.uiLabel;
            j["ui-visible-if"] = i.uiVisibleIf;
            j["ui-group"] = i.uiGroup;
            QJsonObject uiWidgetOptions;
            for(auto opt: i.uiWidgetOptions.toStdMap())
                uiWidgetOptions[opt.first] = opt.second;
            j["ui-options"] = uiWidgetOptions;
            jin.append(j);
        }

        for(auto i: outputs)
        {
            QJsonObject j;
            j["identifier"] = i.identifier;
            j["uid"] = QString::number(i.uid);
            j["width"] = i.width;
            j["height"] = i.height;
            j["channel"] = i.outChannelName;
            j["ui-label"] = i.uiLabel;
            j["ui-visible-if"] = i.uiVisibleIf;
            j["ui-group"] = i.uiGroup;
            jout.append(j);
        }

        j["inputs"] = jin;
        j["outputs"] = jout;
    }
    else
    {
        // A preset just need its tweaks, nothing else [DATA!]
        QJsonObject tweaks;
        for(auto i=tweakedValues.begin(); i!=tweakedValues.end();++i)
            tweaks[i.key()] = i.value();
        j["tweaks"] = tweaks;
    }
    return j;
}

SBSPreset* SBSPreset::FromJson(QJsonObject o, bool isTemplatePreset)
{
    auto p = new SBSPreset;

    // Main data
    p->pkgurl = o["pkgurl"].toString();
    p->label = o["label"].toString();
    p->relativeSBSARPath = o["relativeSBSARPath"].toString();
    p->isTemplatePreset = isTemplatePreset;

    QJsonArray jin, jout;
    jin = o["inputs"].toArray();
    jout = o["outputs"].toArray();

    if(isTemplatePreset)
    {
        // inputs
        for(auto J: jin)
        {
            auto j = J.toObject();
            SBSPreset::Input i;
            i.identifier = j["identifier"].toString();
            i.uid = j["uid"].toString().toLongLong();
            i.propertyType = j["type"].toInt();
            i.value = j["value"].toString();
            i.uiWidget = j["ui-widget"].toString();
            i.uiLabel = j["ui-label"].toString();
            i.uiVisibleIf = j["ui-visible-if"].toString();
            i.uiGroup = j["ui-group"].toString();

            auto uiOptions = j["ui-options"].toObject();
            for(auto it = uiOptions.begin(); it!=uiOptions.end();++it)
                i.uiWidgetOptions[it.key()] = it.value().toString();

            p->inputs.append(i);
        }

        // outputs
        for(auto J: jout)
        {
            auto j = J.toObject();
            SBSPreset::Output i;
            i.identifier = j["identifier"].toString();
            i.uid = j["uid"].toString().toLongLong();
            i.width = j["width"].toInt();
            i.height = j["height"].toInt();
            i.outChannelName = j["channel"].toString();
            i.uiLabel = j["ui-label"].toString();
            i.uiVisibleIf = j["ui-visible-if"].toString();
            i.uiGroup = j["ui-group"].toString();

            p->outputs.append(i);
        }
    }
    else
    {
        auto tweaks = o["tweaks"].toObject();
        for(QJsonObject::iterator i = tweaks.begin(); i != tweaks.end(); i++)
            p->tweakedValues[i.key()] = i.value().toString();
    }

    return p;
}

const SBSPreset::Output* SBSPreset::GetDefaultPreviewOutput()
{
    ResolveSource();
    if(sourceGraphPtr) return sourceGraphPtr->GetDefaultPreviewOutput();

    // Do we have basecolor?
    for(auto& o: outputs)
        if(o.outChannelName == "baseColor")
            return &o;

    // Else find Diffuse if any
    for(auto& o: outputs)
        if(o.outChannelName == "diffuse")
            return &o;

    // then just get first there
    if(outputs.size()>0) return &outputs[0];

    // and finally in case of SBSAR with no outputs
    return nullptr;
}

const SBSPreset::Output *SBSPreset::GetNormalOutput()
{
    ResolveSource();
    if(sourceGraphPtr) return sourceGraphPtr->GetNormalOutput();

    // Do we have normal?
    for(auto& o: outputs)
        if(o.outChannelName == "normal")
            return &o;

    // and finally in case of SBSAR with no normal
    return nullptr;
}

const SBSPreset::Output *SBSPreset::GetChannelOutput(QString channel)
{
    ResolveSource();
    if(sourceGraphPtr) return sourceGraphPtr->GetChannelOutput(channel);

    // Do we have normal?
    for(auto& o: outputs)
        if(o.outChannelName == channel)
            return &o;

    // and finally in case of SBSAR with no normal
    return nullptr;
}

const QList<SBSPreset::Output> SBSPreset::GetOutputs()
{
    ResolveSource();
    if(isTemplatePreset) return outputs;
    return sourceGraphPtr->outputs;
}


QString SBSPreset::getValue(QString in)
{
    if(isValueTweaked(in)) return tweakedValues[in];
    auto inputs = GetInputDefinitions();
    for(auto i: inputs)
        if(i.identifier == in)
            return i.value;
    return QString();
}

bool SBSPreset::isValueTweaked(QString in)
{
    return tweakedValues.keys().contains(in);
}

bool SBSPreset::tweakValue(QString in, QString val)
{
    if(tweakedValues[in] == val) return false;
    tweakedValues[in] = val;
    return true;
}

bool SBSPreset::resetValue(QString in)
{
    if(tweakedValues.keys().contains(in))
    {
        tweakedValues.remove(in);
        return true;
    }
    return false;
}
