#include "ModuleManager.h"

using namespace std;
using namespace std::chrono;
using namespace sqlite;

namespace AMM {
    ModuleManager::ModuleManager() {
        // Initialize the DDS Manager
        m_mgr = std::make_unique<DDSManager<ModuleManager>>(config_file);

        // Initialize everything we'll need to listen for
        m_mgr->InitializeSimulationControl();
        m_mgr->InitializeAssessment();
        m_mgr->InitializeLog();
        m_mgr->InitializeRenderModification();
        m_mgr->InitializePhysiologyModification();
        m_mgr->InitializeEventRecord();
        m_mgr->InitializeEventFragment();
        m_mgr->InitializeCommand();
        m_mgr->InitializeFragmentAmendmentRequest();
        m_mgr->InitializeOmittedEvent();
        m_mgr->InitializeOperationalDescription();
        m_mgr->InitializeModuleConfiguration();
        m_mgr->InitializeStatus();

        // Module Manager listens to almost everything
        m_mgr->CreateSimulationControlSubscriber(this, &ModuleManager::onNewSimulationControl);
        m_mgr->CreateAssessmentSubscriber(this, &ModuleManager::onNewAssessment);
        m_mgr->CreateLogSubscriber(this, &ModuleManager::onNewLog);
        m_mgr->CreateRenderModificationSubscriber(this, &ModuleManager::onNewRenderModification);
        m_mgr->CreatePhysiologyModificationSubscriber(this, &ModuleManager::onNewPhysiologyModification);
        m_mgr->CreateEventRecordSubscriber(this, &ModuleManager::onNewEventRecord);
        m_mgr->CreateEventFragmentSubscriber(this, &ModuleManager::onNewEventFragment);
        m_mgr->CreateCommandSubscriber(this, &ModuleManager::onNewCommand);
        m_mgr->CreateFragmentAmendmentRequestSubscriber(this, &ModuleManager::onNewFragmentAmendmentRequest);
        m_mgr->CreateOmittedEventSubscriber(this, &ModuleManager::onNewOmittedEvent);
        m_mgr->CreateOperationalDescriptionSubscriber(this, &ModuleManager::onNewOperationalDescription);
        m_mgr->CreateModuleConfigurationSubscriber(this, &ModuleManager::onNewModuleConfiguration);
        m_mgr->CreateStatusSubscriber(this, &ModuleManager::onNewStatus);

        // We only publish module configuration and sim controls
        m_mgr->CreateOperationalDescriptionPublisher();
        m_mgr->CreateModuleConfigurationPublisher();
        m_mgr->CreateSimulationControlPublisher();
        m_mgr->CreateCommandPublisher();

        m_uuid.id(m_mgr->GenerateUuidString());
    }

    ModuleManager::~ModuleManager() {
        m_mgr->Shutdown();
    }

    void ModuleManager::RunSimulation() {
        LOG_INFO << "Starting simulation run...";
        isPaused = false;
        currentStatus = "RUNNING";
    }

    void ModuleManager::HaltSimulation() {
        LOG_INFO << "Halting simulation...";
        isPaused = true;
        currentStatus = "HALTED";
    }

    void ModuleManager::ResetSimulation() {
        LOG_INFO << "Resetting simulation...";
        isPaused = false;
        currentStatus = "RESET";
        // Clear state but keep scenario loaded
    }

    void ModuleManager::SaveSimulation() {
        LOG_INFO << "Saving simulation state...";
        // TODO: Implement state persistence
        LOG_WARNING << "Save simulation not yet fully implemented.";
    }

    void ModuleManager::SetDatabaseName(const std::string& dbName) {
        if (!dbName.empty()) {
            m_database_name = dbName;
            LOG_INFO << "Database name set to: " << m_database_name;
        } else {
            LOG_WARNING << "Attempted to set empty database name, ignoring.";
        }
    }

    std::string ModuleManager::GetDatabaseName() const {
        return m_database_name;
    }

    void ModuleManager::PublishOperationalDescription() {
        AMM::OperationalDescription od;
        od.name(moduleName);
        od.model("Module Manager");
        od.manufacturer("Vcom3D");
        od.serial_number("1.0.0");
        od.module_id(m_uuid);
        od.module_version("1.0.0");
        const std::string capabilities = Utility::read_file_to_string("config/module_manager_capabilities.xml");
        od.capabilities_schema(capabilities);
        od.description();
        m_mgr->WriteOperationalDescription(od);
    }

    void ModuleManager::PublishConfiguration() {
        AMM::ModuleConfiguration mc;
        auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        mc.timestamp(ms);
        mc.module_id(m_uuid);
        mc.name(moduleName);
        const std::string configuration = Utility::read_file_to_string("config/module_manager_configuration.xml");
        mc.capabilities_configuration(configuration);
        m_mgr->WriteModuleConfiguration(mc);
    }

    void ModuleManager::Shutdown() {
        LOG_INFO << "Module Manager shutting down...";

        // Send disconnect message to other modules
        AMM::OperationalDescription od;
        od.name("disconnect");
        od.module_id(m_uuid);
        m_mgr->WriteOperationalDescription(od);

        // Shutdown DDS manager
        if (m_mgr) {
            m_mgr->Shutdown();
        }

        LOG_INFO << "Module Manager shutdown complete.";
    }

    void ModuleManager::ShowStatus() {
        LOG_INFO << "\n=== Module Manager Status ===";
        std::cout << "\tCurrent Scenario: " << currentScenario << std::endl;
        std::cout << "\tCurrent State: " << currentState << std::endl;
        std::cout << "\tCurrent Status: " << currentStatus << std::endl;
        std::cout << "\tPaused: " << (isPaused ? "Yes" : "No") << std::endl;

        // Query database for connected modules
        try {
            sqlite_config config;
            database db(m_database_name, config);

            std::cout << "\n=== Connected Modules ===" << std::endl;
            db << "SELECT module_name, model, module_version FROM module_capabilities ORDER BY module_name;"
               >> [&](string name, string model, string version) {
                   std::cout << "  - " << name << " (Model: " << model << ", Version: " << version << ")"  << std::endl;
               };

            std::cout << "\n=== Module Status ===";
            db << "SELECT module_name, capability, status, message FROM module_status ORDER BY module_name, capability;"
               >> [&](string name, string capability, string status, string message) {
                   std::cout << "  - " << name << " / " << capability << ": " << status
                           << (message.empty() ? "" : " - " + message)  << std::endl;
               };

        } catch (exception &e) {
            LOG_ERROR << "Error querying status: " << e.what();
        }

        LOG_INFO << "\n============================";
    }

    void ModuleManager::ClearEventLog() {
        LOG_INFO << "Clearing event log...";
        try {
            sqlite_config config;
            database db(m_database_name, config);
            std::lock_guard<std::mutex> lock(m_mapmutex);
            db << "DELETE FROM events;";
            LOG_INFO << "Event log cleared.";
        } catch (exception &e) {
            LOG_ERROR << "Error clearing event log: " << e.what();
        }
    }

    void ModuleManager::ClearDiagnosticLog() {
        LOG_INFO << "Clearing diagnostic log...";
        try {
            sqlite_config config;
            database db(m_database_name, config);
            std::lock_guard<std::mutex> lock(m_mapmutex);
            db << "DELETE FROM logs;";
            LOG_INFO << "Diagnostic log cleared.";
        } catch (exception &e) {
            LOG_ERROR << "Error clearing diagnostic log: " << e.what();
        }
    }

    void ModuleManager::SetupTables() {
        LOG_INFO << "Setting up database tables...";
        try {
            sqlite_config config;
            database db(m_database_name, config);
            std::lock_guard<std::mutex> lock(m_mapmutex);

            LOG_INFO << "Creating event log table...";
            db << "create table if not exists events("
                  "source text,"
                  "topic text,"
                  "event_id text,"
                  "timestamp bigint,"
                  "data text"
                  ");";

            LOG_INFO << "Creating module capabilities table...";
            db << "create table if not exists module_capabilities ("
                  "module_id text,"
                  "module_guid text,"
                  "module_name text,"
                  "description text,"
                  "manufacturer text,"
                  "model text,"
                  "module_version text,"
                  "serial_number text,"
                  "capabilities text"
                  ");";

            LOG_INFO << "Creating module status table...";
            db << "create table if not exists module_status ("
                  "module_id text,"
                  "module_guid text,"
                  "module_name text,"
                  "capability text,"
                  "status text,"
                  "message text,"
                  "timestamp bigint,"
                  "encounter_id text"
                  ");";

            LOG_INFO << "Creating log record table...";
            db << "create table if not exists logs("
                  "module_id text,"
                  "module_guid text,"
                  "module_name text,"
                  "message text,"
                  "log_level text,"
                  "timestamp bigint"
                  ");";

            LOG_INFO << "Database tables created successfully.";

        } catch (exception &e) {
            LOG_ERROR << "Error setting up tables: " << e.what();
            return;
        }
        
        WipeTables();
    }

    void ModuleManager::WipeTables() {
        LOG_INFO << "Wiping database tables...";
        try {
            sqlite_config config;
            database db(m_database_name, config);
            std::lock_guard<std::mutex> lock(m_mapmutex);

            db << "delete from events;";
            db << "delete from module_capabilities;";
            db << "delete from module_status;";
            db << "delete from logs;";

            LOG_INFO << "Database tables wiped successfully.";

        } catch (exception &e) {
            LOG_ERROR << "Error wiping tables: " << e.what();
        }
    }

    uint64_t ModuleManager::GetTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string ModuleManager::ExtractGUIDToString(GUID_t guid) {
        std::ostringstream module_guid;
        module_guid << guid;
        return module_guid.str().substr(0, module_guid.str().find("|"));
    }

    void ModuleManager::onNewLog(AMM::Log &log, SampleInfo_t *info) {
        LOG_TRACE << "Log recieved:\n"
                  << "Timestamp: " << log.timestamp() << "\n"
                  << "Module ID: " << log.module_id().id() << "\n"
                  << "Level:     " << AMM::Utility::ELogLevelStr(log.level()) << "\n"
                  << "Message:   " << log.message();

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        sqlite_config config;
        database db(m_database_name, config);
        std::lock_guard<std::mutex> lock(m_mapmutex);
        try {
            db
                    << "insert into logs (module_id, module_guid, message, log_level, timestamp) values (?,?,?,?,?);"
                    << log.module_id().id()
                    << module_guid
                    << log.message()
                    << AMM::Utility::ELogLevelStr(log.level())
                    << log.timestamp();
        } catch (exception &e) {
            LOG_ERROR << e.what();
        }
    }


    void ModuleManager::onNewModuleConfiguration(AMM::ModuleConfiguration &mc, SampleInfo_t *info) {
        LOG_TRACE << "Module Configuration recieved:\n"
                  << "Name:         " << mc.name() << "\n"
                  << "Module ID:    " << mc.module_id().id() << "\n"
                  << "Encounter:    " << mc.educational_encounter().id() << "\n"
                  << "Timestamp:    " << mc.timestamp() << "\n"
                  << "Capabilities: Not shown";

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::lock_guard<std::mutex> lock(m_mapmutex);
        try {
            // TODO: Implement module configuration handling
        } catch (exception &e) {
            LOG_ERROR << e.what();
        }
    }


    void ModuleManager::onNewStatus(AMM::Status &status, SampleInfo_t *info) {
        LOG_TRACE << "Status recieved:\n"
                  << "Module ID:   " << status.module_id().id() << "\n"
                  << "Module Name: " << status.module_name() << "\n"
                  << "Encounter:   " << status.educational_encounter().id() << "\n"
                  << "Capability:  " << status.capability() << "\n"
                  << "Timestamp:   " << status.timestamp() << "\n"
                  << "Value:       " << AMM::Utility::EStatusValueStr(status.value()) << "\n"
                  << "Message:     " << status.message();

        sqlite_config config;
        database db(m_database_name, config);

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::lock_guard<std::mutex> lock(m_mapmutex);
        try {
            db << "replace into module_status (module_id, module_guid, module_name, "
                  "capability, status, message, timestamp, encounter_id) values (?,?,?,?,?,?,?,?);"
               << status.module_id().id() << module_guid << status.module_name()
               << status.capability() << AMM::Utility::EStatusValueStr(status.value())
               << status.message()
               << status.timestamp() << status.educational_encounter().id();

        } catch (exception &e) {
            LOG_ERROR << e.what();
        }
    }

    void ModuleManager::onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info) {
        LOG_TRACE << "Simulation Control recieved:\n"
                  << "Timestamp: " << simControl.timestamp() << "\n"
                  << "Type:      " << AMM::Utility::EControlTypeStr(simControl.type()) << "\n"
                  << "Encounter: " << simControl.educational_encounter().id();

        switch (simControl.type()) {
            case AMM::ControlType::RUN: {
                RunSimulation();
                break;
            }

            case AMM::ControlType::HALT: {
                HaltSimulation();
                break;
            }

            case AMM::ControlType::RESET: {
                ResetSimulation();
                break;
            }

            case AMM::ControlType::SAVE: {
                SaveSimulation();
                break;
            }
        }

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());
        std::ostringstream logmessage;
        logmessage << "[" << AMM::Utility::EControlTypeStr(simControl.type()) << "]";
        LogEntry newLogEntry{module_guid, AMM::TopicNames::SimControl, "n/a", simControl.timestamp(),
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewAssessment(AMM::Assessment &assessment, SampleInfo_t *info) {
        LOG_TRACE << "Assessment recieved:\n"
                  << "ID:       " << assessment.id().id() << "\n"
                  << "Event ID: " << assessment.event_id().id() << "\n"
                  << "Value:    " << AMM::Utility::EAssessmentValueStr(assessment.value()) << "\n"
                  << "Comment:  " << assessment.comment();

        uint64_t timestamp = GetTimestamp();
        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << assessment.value() << "]" << assessment.comment();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::Assessment, assessment.event_id().id(), timestamp,
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewEventFragment(AMM::EventFragment &ef, SampleInfo_t *info) {
        LOG_TRACE << "Event Fragment recieved:\n"
                  << "ID:        " << ef.id().id() << "\n"
                  << "Timestamp: " << ef.timestamp() << "\n"
                  << "Encounter: " << ef.educational_encounter().id() << "\n"
                  << "Location:  " << ef.location().name() << " - " << ef.location().FMAID() << "\n"
                  << "Agent:     " << AMM::Utility::EEventAgentTypeStr(ef.agent_type()) << "\n"
                  << "Agent ID:  " << ef.agent_id().id() << "\n"
                  << "Type:      " << ef.type() << "\n"
                  << "Data:      " << ef.data();

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << ef.type() << "]" << ef.data();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::EventFragment, ef.id().id(), ef.timestamp(),
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewEventRecord(AMM::EventRecord &er, SampleInfo_t *info) {
        LOG_TRACE << "Event Record recieved:\n"
                  << "ID:        " << er.id().id() << "\n"
                  << "timestamp: " << er.timestamp() << "\n"
                  << "Encounter: " << er.educational_encounter().id() << "\n"
                  << "Location:  " << er.location().name() << " - " << er.location().FMAID() << "\n"
                  << "Agent:     " << AMM::Utility::EEventAgentTypeStr(er.agent_type()) << "\n"
                  << "Agent ID:  " << er.agent_id().id() << "\n"
                  << "Type:      " << er.type() << "\n"
                  << "Data:      " << er.data();

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << er.type() << "]" << er.data();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::EventRecord, er.id().id(), er.timestamp(),
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewFragmentAmendmentRequest(AMM::FragmentAmendmentRequest &ffar, SampleInfo_t *info) {
        LOG_TRACE << "Fragment Amendment Request recieved:\n"
                  << "ID:          " << ffar.id().id() << "\n"
                  << "Fragment ID: " << ffar.fragment_id().id() << "\n"
                  << "Status:      " << AMM::Utility::EFarStatusStr(ffar.status()) << "\n"
                  << "Location:    " << ffar.location().name() << " - " << ffar.location().FMAID() << "\n"
                  << "Agent:       " << AMM::Utility::EEventAgentTypeStr(ffar.agent_type()) << "\n"
                  << "Agent ID:    " << ffar.agent_id().id();

        uint64_t timestamp = GetTimestamp();
        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << ffar.fragment_id().id() << "]" << ffar.status();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::FragmentAmendmentRequest, ffar.id().id(), timestamp,
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewOmittedEvent(AMM::OmittedEvent &omittedEvent, SampleInfo_t *info) {
        LOG_TRACE << "\nOmitted Event recieved:\n"
                  << "ID:        " << omittedEvent.id().id() << "\n"
                  << "timestamp: " << omittedEvent.timestamp() << "\n"
                  << "Encounter: " << omittedEvent.educational_encounter().id() << "\n"
                  << "Location:  " << omittedEvent.location().name() << " - " << omittedEvent.location().FMAID() << "\n"
                  << "Agent:     " << AMM::Utility::EEventAgentTypeStr(omittedEvent.agent_type()) << "\n"
                  << "Agent ID:  " << omittedEvent.agent_id().id() << "\n"
                  << "Type:      " << omittedEvent.type() << "\n"
                  << "Data:      " << omittedEvent.data();

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << omittedEvent.type() << "]" << omittedEvent.data();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::OmittedEvent, omittedEvent.id().id(),
                             omittedEvent.timestamp(),
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewOperationalDescription(AMM::OperationalDescription &opDescript, SampleInfo_t *info) {
        LOG_INFO << "Operational description for module " << opDescript.name() << " / model " << opDescript.model();

        sqlite_config config;
        database db(m_database_name, config);

        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::lock_guard<std::mutex> lock(m_mapmutex);
        if (opDescript.name() == "disconnect") {
            try {
                db << "delete from module_capabilities where module_id = ? ;" << module_guid;
            } catch (exception &e) {
                LOG_ERROR << e.what();
            }
        } else {
            try {
                db << "replace into module_capabilities (module_id, module_guid,"
                      "module_name, description, "
                      "manufacturer, model,"
                      "module_version, serial_number,"
                      "capabilities) values (?,?,?,?,?,?,?,?,?);"
                   << opDescript.module_id().id() << module_guid
                   << opDescript.name() << opDescript.description()
                   << opDescript.manufacturer() << opDescript.model()
                   << opDescript.module_version() << opDescript.serial_number()
                   << opDescript.capabilities_schema().to_string();
            } catch (exception &e) {
                LOG_ERROR << e.what();
            }
        }
    }

    void ModuleManager::onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info) {
        if (rendMod.data().to_string().find("START_OF") == std::string::npos) {
            LOG_TRACE << "Render Modification recieved:\n"
                      << "ID:       " << rendMod.id().id() << "\n"
                      << "Event ID: " << rendMod.event_id().id() << "\n"
                      << "Type:     " << rendMod.type() << "\n"
                      << "Data      " << rendMod.data();
        } else {
            // inhale/exhale message
            return;
        }

        uint64_t timestamp = GetTimestamp();
        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << rendMod.type() << "]" << rendMod.data();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::RenderModification, rendMod.event_id().id(), timestamp,
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::onNewPhysiologyModification(AMM::PhysiologyModification &physMod, SampleInfo_t *info) {
        LOG_TRACE << "Physiology Modification recieved:\n"
                  << "ID:       " << physMod.id().id() << "\n"
                  << "Event ID: " << physMod.event_id().id() << "\n"
                  << "Type:     " << physMod.type() << "\n"
                  << "Data:     " << physMod.data();

        uint64_t timestamp = GetTimestamp();
        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        std::ostringstream logmessage;
        logmessage << "[" << physMod.type() << "]" << physMod.data();
        LogEntry newLogEntry{module_guid, AMM::TopicNames::PhysiologyModification, physMod.event_id().id(), timestamp,
                             logmessage.str()};
        WriteLogEntry(newLogEntry);
    }

    void ModuleManager::SendTestCommand(const std::string action) {
        AMM::Command cmdInstance;
        cmdInstance.message(action);
        m_mgr->WriteCommand(cmdInstance);
    }

    void ModuleManager::onNewCommand(AMM::Command &command, eprosima::fastrtps::SampleInfo_t *info) {
        LOG_TRACE << "Command recieved:\n"
                  << "Message:" << command.message();

        uint64_t timestamp = GetTimestamp();
        std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

        LogEntry newLogEntry{module_guid, AMM::TopicNames::Command, "n/a", timestamp,
                             command.message()};
        WriteLogEntry(newLogEntry);

        std::ostringstream messageOut;

        if (!command.message().compare(0, sysPrefix.size(), sysPrefix)) {
            std::string value = command.message().substr(sysPrefix.size());
            if (value.compare("START_SIM") == 0) {
                LOG_INFO << "Received START_SIM command";
                RunSimulation();
            } else if (value.compare("STOP_SIM") == 0) {
                LOG_INFO << "Received STOP_SIM command";
                HaltSimulation();
            } else if (value.compare("PAUSE_SIM") == 0) {
                LOG_INFO << "Received PAUSE_SIM command";
                isPaused = true;
                currentStatus = "PAUSED";
            } else if (value.compare("RESET_SIM") == 0) {
                LOG_INFO << "Received RESET_SIM command";
                ResetSimulation();
            } else if (!value.compare(0, loadScenarioPrefix.size(), loadScenarioPrefix)) {
                currentScenario = value.substr(loadScenarioPrefix.size());
                LOG_INFO << "Load scenario command received for " << currentScenario;

                LOG_INFO << "Sending simcontrol RESET";
                AMM::SimulationControl simControl;
                auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                simControl.timestamp(ms);
                simControl.type(AMM::ControlType::RESET);
                m_mgr->WriteSimulationControl(simControl);

                if (currentScenario.find(";mid=") != std::string::npos) {
                    std::size_t pos = currentScenario.find(";mid=");
                    std::string mid = currentScenario.substr(pos + 5);
                    currentScenario = currentScenario.substr(0, pos);
                    LOG_INFO << " Scene is " << currentScenario;
                    LOG_INFO << " Manikin is " << mid;
                } else {
                    LOG_INFO << " Scene is " << currentScenario;
                }
                ParseScenarioFromFile("static/scenarios/" + currentScenario + ".xml");
            } else if (!value.compare(0, loadStatePrefix.size(), loadStatePrefix)) {
                currentState = value.substr(loadStatePrefix.size());
            } else {
                messageOut << "ACT" << "=" << command.message() << std::endl;
                LOG_INFO << "Unknown system message: " << messageOut.str();
            }
        } else {
            messageOut << "ACT"
                       << "=" << command.message() << "|";
            LOG_INFO << "Unknown message: " << messageOut.str();
        }
    }


    void ModuleManager::WriteLogEntry(LogEntry newLogEntry) {
        sqlite_config config;
        database db(m_database_name, config);

        std::lock_guard<std::mutex> lock(m_mapmutex);
        try {
            db << "insert into events (source, topic, event_id, timestamp, data) values (?,?,?,?,?);"
               << newLogEntry.source << newLogEntry.topic << newLogEntry.event_id
               << newLogEntry.timestamp << newLogEntry.data;
        } catch (exception &e) {
            LOG_ERROR << e.what();
        }
    }

    void ModuleManager::ParseScenarioFromFile(const std::string xmlFileName) {
        LOG_INFO << "Loading " << xmlFileName << " from filesystem.";
        tinyxml2::XMLDocument doc;
        doc.LoadFile(xmlFileName.c_str());

        // Check for XML parsing errors
        if (doc.Error()) {
            LOG_ERROR << "Failed to load or parse XML file: " << xmlFileName
                     << " - Error: " << doc.ErrorStr();
            return;
        }

        tinyxml2::XMLNode *pRoot = doc.RootElement();
        if (pRoot == nullptr) {
            LOG_ERROR << "Unable to get XML root element.";
            return;
        }

        tinyxml2::XMLElement *scenario = pRoot->FirstChildElement();
        if (scenario == nullptr) {
            LOG_ERROR << "Unable to find scenario element.";
            return;
        }

        const char* scenarioName = scenario->Attribute("name");
        if (scenarioName != nullptr) {
            LOG_INFO << "Loading scenario: " << scenarioName;
        } else {
            LOG_WARNING << "Scenario element has no name attribute.";
        }

        tinyxml2::XMLElement *metadata = scenario->FirstChildElement("metadata");
        if (metadata != nullptr) {
            LOG_TRACE << "Metadata is present, so let's parse and publish it.";
            ParseMetadata(metadata);
        }

        tinyxml2::XMLElement *capabilities = scenario->FirstChildElement("capabilities");
        if (capabilities == nullptr) {
            LOG_ERROR << "Unable to find capabilities element.";
            return;
        }

        tinyxml2::XMLElement *capability = capabilities->FirstChildElement("capability");
        if (capability == nullptr) {
            LOG_ERROR << "Unable to find any capability elements.";
            return;
        }

        ParseCapabilities(capability);
    }

    void ModuleManager::ParseMetadata(tinyxml2::XMLElement *node) {
        tinyxml2::XMLPrinter printer;
        node->Accept(&printer);
        LOG_INFO << "Sending out metadata";
        AMM::ModuleConfiguration mc;
        auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        mc.timestamp(ms);
        mc.name("metadata");
        mc.capabilities_configuration(printer.CStr());
        m_mgr->WriteModuleConfiguration(mc);
    }

    void ModuleManager::ParseCapabilities(tinyxml2::XMLElement *node) {
        while (node) {
            if (node->Attribute("name") == nullptr) {
                LOG_WARNING << "Capability is not named, skipping it.";
                node = node->NextSiblingElement();
                continue;
            }
            std::string capabilityName = node->Attribute("name");
            LOG_INFO << "Parsing capability " << node->Attribute("name");

            tinyxml2::XMLElement *config_data = node->FirstChildElement("configuration_data");
            if (config_data != nullptr) {
                LOG_INFO << "Capability has configuration, so we'll publish it to the bus.";
                tinyxml2::XMLPrinter printer;
                node->Accept(&printer);

                AMM::ModuleConfiguration mc;
                auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                mc.timestamp(ms);
                mc.name(capabilityName);
                mc.capabilities_configuration(printer.CStr());
                m_mgr->WriteModuleConfiguration(mc);
            }

            node = node->NextSiblingElement();
        }
    }
}
