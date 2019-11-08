#pragma once

#include "amm_std.h"

#include "AMM/Utility.h"

#include "AMM/TopicNames.h"

#include <tinyxml2.h>

#include "thirdparty/sqlite_modern_cpp.h"

using namespace std;

namespace AMM {

    struct LogEntry {
        string source;
        string topic;
        string event_id;
        uint64_t timestamp;
        string data = "";
    };

    /// Module Manager class
    ///
    /// Container for Module Manager logic.
    class ModuleManager : ListenerInterface {

    public:


    private:

    protected:

        AMM::UUID m_uuid;

        std::string moduleName = "AMM_ModuleManager";
        const std::string config_file = "config/module_manager_amm.xml";

        DDSManager<ModuleManager> *m_mgr = new DDSManager<ModuleManager>(config_file);

        mutex m_mapmutex;

    public:
        ModuleManager();

        ~ModuleManager();

        void PublishOperationalDescription();

        void PublishConfiguration();

        void Shutdown();

        void ShowStatus();

        void WriteLogEntry(LogEntry log);

        void ClearEventLog();

        void ClearDiagnosticLog();

        std::string ExtractGUIDToString(GUID_t guid);
        uint64_t GetTimestamp();

/// Simulation controllers
    private:
        void RunSimulation();

        void HaltSimulation();

        void ResetSimulation();

        void SaveSimulation();

/// DDS event handlers
    protected:

        void onNewLog(AMM::Log& log, SampleInfo_t *info);

        void onNewModuleConfiguration(AMM::ModuleConfiguration& mc, SampleInfo_t *info);

        void onNewStatus(AMM::Status &status, SampleInfo_t *info);

        void onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info);

        void onNewAssessment(AMM::Assessment &assessment, SampleInfo_t *info);

        void onNewEventFragment(AMM::EventFragment &eventFrag, SampleInfo_t *info);

        void onNewEventRecord(AMM::EventRecord &eventRec, SampleInfo_t *info);

        void onNewFragmentAmendmentRequest(AMM::FragmentAmendmentRequest &ffar, SampleInfo_t *info);

        void onNewOmittedEvent(AMM::OmittedEvent &omittedEvent, SampleInfo_t *info);

        void onNewOperationalDescription(AMM::OperationalDescription &opDescript, SampleInfo_t *info);

        void onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info);

        void onNewPhysiologyModification(AMM::PhysiologyModification &physMod, SampleInfo_t *info);

        void onNewCommand(AMM::Command &command, eprosima::fastrtps::SampleInfo_t *info);

    };


} /// namespace AMM
