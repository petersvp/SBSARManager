#include "sbsarloader.h"
#include <QFile>
#include <QProcess>
#include "logger.h"
#include "substancemanagerapp.h"


SBSARLoader::SBSARLoader(QString fileName)
{
    gfLog(fileName);
    QProcess p;
    p.start(SubstanceManagerApp::sbs7zExe, QStringList() << "l" << fileName << "-slt" );
    p.waitForFinished();
    QString output = p.readAll();
    //gfInfo(output);

    // We need to parse all the graphs in the SBSAR and create Template Presets from them

    QStringList lines = output.split("\r\n");
    QStringList files;
    for(auto line: lines)
        if(line.startsWith("Path = ") && line.endsWith(".xml"))
            files.append(line.mid(7));

    for(auto file: files)
    {

        p.start(SubstanceManagerApp::sbs7zExe, QStringList() << "e" << fileName << file << "-so");
        p.waitForFinished();
        QString xml = p.readAll();
        QString rpath = SubstanceManagerApp::instance->ConvertToRelativePath(fileName);
        auto items = SBSPreset::TemplatePresetsFromSBSARMeta(xml);
        for(auto preset: items)
        {
            preset->relativeSBSARPath = rpath;
            m_presets.push_back(preset);
        }

        if(items.size() == 0)
        {
            gfError(QString("Failed to load graphs XML Meta file %1 from %2").arg(file).arg(fileName));
        }
    }
}

QList<SBSPreset *> SBSARLoader::GetPresets()
{
    return m_presets;
}

