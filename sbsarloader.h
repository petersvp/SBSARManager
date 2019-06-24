#ifndef SBSARLOADER_H
#define SBSARLOADER_H

#include "sbspreset.h"

#include <QString>


class SBSARLoader
{
public:
    SBSARLoader(QString fileName);
    QList<SBSPreset*> GetPresets();
private:
    QList<SBSPreset*> m_presets;
};

#endif // SBSARLOADER_H
