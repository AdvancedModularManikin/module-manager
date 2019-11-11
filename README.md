# AMM - Module Manager

The Module Manager is a core software component that coordinates the participation,
initialization, configuration, and termination of AMM modules during the educational encounter.
All AMM compliant modules must perform an appropriate handshake procedure which includes
information about the module (operational description), the capabilities it provides, and the
configuration data needed to provide those capabilities. The Module Manager is also responsible
for loading scenarios and publishing the configuration data specific to each module to enable the
capabilities required by the scenario. The Modules must validate their configuration and report
the operational status of each of their enabled capabilities. The Module Manager will aggregate
the operational statuses of all the modules to determine if all the required capabilities of a
scenario are available and operational.

#### Requirements
The Module Manager requires the [AMM Standard Library](https://github.com/AdvancedModularManikin/amm-library) be built and available (and so requires FastRTPS and FastCDR).  In addition to the AMM library dependancies, the Module Manager also requires:
- sqlite3 (`apt-get install sqlite3 libsqlite3-dev`)
- tinyxml2 (`apt-get install libtinyxml2-dev`)
 

### Installation
```bash
    $ git clone https://github.com/AdvancedModularManikin/module-manager
    $ mkdir module-manager/build && cd module-manager/build
    $ cmake ..
    $ cmake --build . --target install
```

By default on a Linux system this will install into `/usr/local/bin`

The Module Manager must have write-access to the directory it is installed in so that it can create and use `amm.db`, a sqlite3 database.

#### The Module Manager is part of the [AMM Core Modules metapackage](https://github.com/AdvancedModularManikin/core-modules)

