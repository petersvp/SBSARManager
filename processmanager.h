#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include "sbspreset.h"

#include <QObject>
#include <QProcess>
#include <QQueue>

struct PresetRender
{
    SBSPreset* preset;
    QString suffix;
    QSize size;
    bool modified;
    QProcess* process;
};
Q_DECLARE_METATYPE(PresetRender)

class ProcessManager: QObject
{
    Q_OBJECT
    ProcessManager();
    ~ProcessManager();

public:

    // main entry
    static ProcessManager* get();
    static void SchedulePresetRender(SBSPreset* preset, QSize size, QString suffix, bool modified);
    static int runningProcessCount();
    static void setMaxAllowedProcesses(uint16_t numProcesses);
    static void ShutDown();

private slots:
    void OnTweakFinished();

signals:
    void PresetRenderFinished(PresetRender* p);


private:
    // TweakedPresets always take precedence
    QQueue<PresetRender> tweakedPresets, runningPresets;

    uint16_t numRunningProcesses();
    void performSchedullerIteration();
};

extern ProcessManager* g_ProcManager;
extern uint16_t g_ProcManagerMaxAllowedProcesses;

#endif // PROCESSMANAGER_H
