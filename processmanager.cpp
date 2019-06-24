#include "processmanager.h"
#include "globals.h"
#include "logger.h"
#include "substancemanagerapp.h"
#include "substancepresetpreview.h"
#include <QList>
#include <QDebug>
#include <QTimer>

ProcessManager* g_ProcManager = nullptr;
uint16_t g_ProcManagerMaxAllowedProcesses = 3;

ProcessManager::ProcessManager()
{
}

ProcessManager::~ProcessManager()
{
    tweakedPresets.clear();
}

ProcessManager* ProcessManager::get()
{
    // init the process manager
    if(!g_ProcManager) g_ProcManager = new ProcessManager();
    return g_ProcManager;
}

void ProcessManager::SchedulePresetRender(SBSPreset *preset, QSize size, QString suffix, bool modified)
{
    // init the process manager
    if(!g_ProcManager) g_ProcManager = new ProcessManager();

    // Schedule Task
    for(auto p: g_ProcManager->tweakedPresets)
        if(p.preset == preset && p.suffix == suffix && p.size == size)
        {
            qDebug() << preset->label << "Already scheduled, skipping";
            g_ProcManager->performSchedullerIteration();
            return;
        }

    g_ProcManager->tweakedPresets.push_back({preset, suffix, size, modified, nullptr});
    qDebug() << preset->label << "Added to process";

    // Run Scheduler iteration
    g_ProcManager->performSchedullerIteration();
}

int ProcessManager::runningProcessCount()
{
    if(!g_ProcManager) return 0;
    return g_ProcManager->runningPresets.size();
}

void ProcessManager::setMaxAllowedProcesses(uint16_t numProcesses)
{
    g_ProcManagerMaxAllowedProcesses = numProcesses;
}

void ProcessManager::ShutDown()
{
    if(!g_ProcManager) return;
    for(auto&p : g_ProcManager->runningPresets)
        p.process->terminate();
}

void ProcessManager::OnTweakFinished()
{
    auto s = sender();
    //QTimer::singleShot(5, [=] {

        PresetRender presetState = s->property("Preset").value<PresetRender>();
        qDebug() << "OnTweakFinished()" << presetState.preset->label;

        emit PresetRenderFinished(&presetState);

        g_ProcManager->runningPresets.erase(
            std::remove_if(
                g_ProcManager->runningPresets.begin(),
                g_ProcManager->runningPresets.end(),
                [&](PresetRender& pi) { return pi.preset == presetState.preset && pi.suffix == presetState.suffix && pi.size == presetState.size; }
            ),
            g_ProcManager->runningPresets.end()
        );
        g_ProcManager->performSchedullerIteration();
        s->deleteLater();
    //});
}

uint16_t ProcessManager::numRunningProcesses()
{
    return runningPresets.size();
}

// Scheduler starts processes with process count limits, or terminates the manager if no work
void ProcessManager::performSchedullerIteration()
{

    // otherwise start process - pop the record and schedule
    QQueue<PresetRender> newQueue;

    if(numRunningProcesses() < g_ProcManagerMaxAllowedProcesses/2)
    while (tweakedPresets.size() && numRunningProcesses() < g_ProcManagerMaxAllowedProcesses)
    {

        // Tweaking a preset
        auto p = tweakedPresets.front();
        tweakedPresets.pop_front();

        // if already running?
        bool isAlreadyRunning = false;
        for(auto r: runningPresets)
            if(r.preset == p.preset)
                isAlreadyRunning = true;

        if(isAlreadyRunning)
        {
            if(SubstanceManagerApp::instance->isKnownPresetPtr(p.preset))
            {
                newQueue.push_back(p);
                qDebug() << p.preset->label <<  "is already running";
            }
        }
        else
        {
            // run it
            qDebug() << "Running process";
            runningPresets.append(p);

            // Pointer to this newly added element's real location will be associated with the QProcess for signaling!
            auto* presetPtr = & runningPresets.last();

            if(p.modified)
            {
                auto cacheDir = SubstanceManagerApp::instance->GetPresetCacheRootDir(p.preset,"");
                gfRemoveFolder(cacheDir);
                SubstancePresetPreview::ClearCacheForDir(cacheDir);
            }

            // Create process
            QProcess* process = SubstanceManagerApp::instance->CreateProcessForSbsPresetRender(p.preset, p.size, p.suffix);
            process->setProperty("Preset", qVariantFromValue(runningPresets.last()));
            QObject::connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), g_ProcManager, SLOT(OnTweakFinished()));
            process->start();
        }
    }
    tweakedPresets.append(newQueue);


    //qDebug() << QString("ProcessManager: Processes %1/%2  (%3 queued)").arg(numRunningProcesses()).arg(g_ProcManagerMaxAllowedProcesses).arg(tweakedPresets.size());
}
