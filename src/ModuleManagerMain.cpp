#include "ModuleManager.h"

#include "amm/BaseLogger.h"

#include <sys/stat.h>

bool closed = false;
int daemonize = 0;
bool setup = false;
int autostart = 0;
bool wipe = false;
std::string database_name = "mohses.db";

/// Check if a file exists.
bool FileExists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

/// Displays current manager configuration.
static void ShowUsage(const std::string &name) {
    std::cerr << "Usage: " << name << " <option(s)>"
         << "\nOptions:\n"
         << "\t-a\t\t\tAuto start\n"
         << "\t-d\t\t\tDaemonize\n"
         << "\t-s\t\t\tSetup module manager tables\n"
         << "\t-w\t\t\tWipe tables\n"
         << "\t-db <filename>\t\tDatabase filename (default: mohses.db)\n"
         << "\t-h,--help\t\tShow this help message\n"
         << std::endl;
}

/// Main menu for Module Manager.
void ShowMenu(AMM::ModuleManager *modManager) {
    std::string action;

    std::cout << std::endl;
    std::cout << " [1]Status " << std::endl;
    std::cout << " [2]Setup tables" << std::endl;
    std::cout << " [3]Wipe tables" << std::endl;
    std::cout << " [4]Shutdown" << std::endl;
    std::cout << " [5]Test scenario file loading" << std::endl;
    std::cout << " [6]Test scenario with manikin ID" << std::endl;
    std::cout << " >> ";

    getline(std::cin, action);

    transform(action.begin(), action.end(), action.begin(), ::toupper);

    if (action == "1") {
        modManager->ShowStatus();
    } else if (action == "2") {
        modManager->SetupTables();
    } else if (action == "3") {
        modManager->WipeTables();
    } else if (action == "4") {
        LOG_INFO << "Shutting down Module Manager.";
        closed = true;
        modManager->Shutdown();
    } else if (action == "5") {
        LOG_INFO << "Loading scenario file via COMMAND";
        modManager->SendTestCommand("[SYS]LOAD_SCENARIO:BVM");
    } else if (action == "6") {
        LOG_INFO << "Loading scenario file via COMMAND for manikin 1";
        modManager->SendTestCommand("[SYS]LOAD_SCENARIO:BVM;mid=manikin_1");
    } else {
        LOG_WARNING << "Unknown menu option: " << action;
    }
}

/// Main program
int main(int argc, char *argv[]) {
    static plog::ColorConsoleAppender <plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    LOG_INFO << "AMM - Module Manager";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

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

        if (arg == "-db") {
            if (i + 1 < argc) {
                database_name = argv[++i];
                LOG_INFO << "Database name set to: " << database_name;
            } else {
                std::cerr << "-db option requires a filename argument." << std::endl;
                ShowUsage(argv[0]);
                return 1;
            }
        }
    }

    AMM::ModuleManager modManager;

    // Set database name if specified
    modManager.SetDatabaseName(database_name);

    // Check if database file exists, if not, create it and setup tables
    if (!FileExists(database_name)) {
        LOG_INFO << "Database file '" << database_name << "' does not exist. Creating and initializing...";
        setup = true;  // Force setup
    }

    if (setup) {
        LOG_INFO << "Creating AMM database schema.";
        modManager.SetupTables();
    }

    if (wipe) {
        LOG_INFO << "Wiping tables on startup";
        modManager.WipeTables();
    }

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
