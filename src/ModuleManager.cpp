#include "ModuleManager.h"

using namespace std;
using namespace std::chrono;
using namespace sqlite;

namespace AMM {
ModuleManager::ModuleManager() {

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

   m_uuid.id(m_mgr->GenerateUuidString());
}

ModuleManager::~ModuleManager() {
   m_mgr->Shutdown();
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
   /// Gracefully close and delete everything created by mod manager.

}

void ModuleManager::ShowStatus() {
   // Show connected modules
}

void ModuleManager::ClearEventLog() {}

void ModuleManager::ClearDiagnosticLog() {}

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
      database db("amm.db", config);
      m_mapmutex.lock();
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
      m_mapmutex.unlock();
   }


   void ModuleManager::onNewModuleConfiguration(AMM::ModuleConfiguration &mc, SampleInfo_t *info) {
      LOG_TRACE << "Module Configuration recieved:\n"
      << "Name:         " << mc.name() << "\n"
      << "Module ID:    " << mc.module_id().id() << "\n"
      << "Encounter:    " << mc.educational_encounter().id() << "\n"
      << "Timestamp:    " << mc.timestamp() << "\n"
      << "Capabilities: Not shown";

      std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

      m_mapmutex.lock();
      try {

      } catch (exception &e) {
         LOG_ERROR << e.what();
      }

      m_mapmutex.unlock();
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
      database db("amm.db", config);

      std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

      m_mapmutex.lock();
      try {
         db << "replace into module_status (module_id, module_guid, module_name, "
         "capability, status, timestamp, encounter_id) values (?,?,?,?,?,?,?,?);"
         << status.module_id().id() << module_guid << status.module_name()
         << status.capability() << AMM::Utility::EStatusValueStr(status.value())
         << status.message()
         << status.timestamp() << status.educational_encounter().id();

      } catch (exception &e) {
         LOG_ERROR << e.what();
      }
      m_mapmutex.unlock();
   }

   void ModuleManager::onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info) {
      LOG_TRACE << "Simulation Control recieved:\n"
      << "Timestamp: " << simControl.timestamp() << "\n"
      << "Type:      " << AMM::Utility::EControlTypeStr(simControl.type()) << "\n"
      << "Encounter: " << simControl.educational_encounter().id();

      switch (simControl.type()) {
         case AMM::ControlType::RUN: {

            break;
         }

         case AMM::ControlType::HALT: {

            break;
         }

         case AMM::ControlType::RESET: {

            break;
         }

         case AMM::ControlType::SAVE: {

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
                     database db("amm.db", config);

                     std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

                     m_mapmutex.lock();
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
                           << opDescript.capabilities_schema();
                        } catch (exception &e) {
                           LOG_ERROR << e.what();
                        }
                     }
                     m_mapmutex.unlock();

                  }

                  void ModuleManager::onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info) {
                     LOG_TRACE << "Render Modification recieved:\n"
                     << "ID:       " << rendMod.id().id() << "\n"
                     << "Event ID: " << rendMod.event_id().id() << "\n"
                     << "Type:     " << rendMod.type() << "\n"
                     << "Data      " << rendMod.data();

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

                        void ModuleManager::onNewCommand(AMM::Command &command, eprosima::fastrtps::SampleInfo_t *info) {
                           LOG_TRACE << "Command recieved:\n"
                           << "Message:" << command.message();

                           uint64_t timestamp = GetTimestamp();
                           std::string module_guid = ExtractGUIDToString(info->sample_identity.writer_guid());

                           LogEntry newLogEntry{module_guid, AMM::TopicNames::Command, "n/a", timestamp,
                           command.message()};
                           WriteLogEntry(newLogEntry);
                        }


                        void ModuleManager::WriteLogEntry(LogEntry newLogEntry) {
                           sqlite_config config;
                           database db("amm.db", config);

                           m_mapmutex.lock();
                           try {
                              db << "insert into events (source, topic, event_id, timestamp, data) values (?,?,?,?,?);"
                              << newLogEntry.source << newLogEntry.topic << newLogEntry.event_id
                              << newLogEntry.timestamp << newLogEntry.data;
                           } catch (exception &e) {
                              LOG_ERROR << e.what();
                           }
                           m_mapmutex.unlock();
                        }
                     }
