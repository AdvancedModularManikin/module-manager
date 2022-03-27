#include "ModuleManager.h"

#include "thirdparty/sqlite_modern_cpp.h"

#include "amm/BaseLogger.h"

using namespace std;
using namespace sqlite;


bool closed = false;
int daemonize = 0;
bool setup = false;
int autostart = 0;
bool wipe = false;

/// Clears database tables.
void WipeTables() {
    try {
        sqlite_config config;
        database db("amm.db", config);

        db << "delete from events;";
        db << "delete from module_capabilities;";
        db << "delete from module_status;";
        db << "delete from logs;";

    } catch (exception &e) {
        LOG_ERROR << e.what();
    }

}

/// On start initialization for database tables.
void SetupTables() {

    try {
        sqlite_config config;
        database db("amm.db", config);

        LOG_INFO << "Creating event log table...";
        db << "create table if not exists events("
              "source text,"
              "module_id text,"
              "module_guid text,"
              "module_name text,"
              "event_id text,"
              "topic text,"
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

    } catch (exception &e) {
        LOG_ERROR << e.what();
    }

    WipeTables();
}

/// Displays current manager configuration.
static void ShowUsage(const std::string &name) {
    cerr << "Usage: " << name << " <option(s)>"
         << "\nOptions:\n"
         << "\t-a\t\t\tAuto start\n"
         << "\t-d\t\t\tDaemonize\n"
         << "\t-s\t\t\tSetup module manager tables\n"
         << "\t-w\t\t\tWipe tables\n"
         << "\t-h,--help\t\t\tShow this help message\n"
         << endl;
}

/// Main menu for Module Manager.
void ShowMenu(AMM::ModuleManager *modManager) {
    string action;

    cout << endl;
    cout << " [1]Status " << endl;
    cout << " [2]Setup tables" << endl;
    cout << " [3]Wipe tables" << endl;
    cout << " [4]Shutdown" << endl;
    cout << " [5]Test scenario file loading" << endl;
    cout << " >> ";

    getline(cin, action);

    transform(action.begin(), action.end(), action.begin(), ::toupper);

    if (action == "1") {
        modManager->ShowStatus();
    } else if (action == "2") {
        SetupTables();
    } else if (action == "3") {
        WipeTables();
    } else if (action == "4") {
        LOG_INFO << "Shutting down Module Manager.";
        closed = true;
        modManager->Shutdown();
    }  else {
            /// TODO: Unknown menu action.
    }
}

/// Main program
int main(int argc, char *argv[]) {
    static plog::ColorConsoleAppender <plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    LOG_INFO << "AMM - Module Manager";

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if ((arg == "-h") || (arg == "--help")) {
            ShowUsage(argv[0]);
            return 0;
        }

        if (arg == "-d") {
            daemonize = 1;
        }

        if (arg == "-a") {
            autostart = 1;
        }

        if (arg == "-s") {
            setup = true;
        }

        if (arg == "-w") {
            wipe = true;
        }
    }

    if (setup) {
        LOG_INFO << "Creating AMM database schema.";
        SetupTables();
    }

    if (wipe) {
        LOG_INFO << "Wiping tables on startup";
        WipeTables();
    }

    AMM::ModuleManager modManager;

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    modManager.PublishOperationalDescription();
    modManager.PublishConfiguration();

    while (!closed) {
        if (autostart != 1) {
            ShowMenu(&modManager);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    modManager.Shutdown();
    LOG_INFO << "Exiting.";

    return 0;
}
