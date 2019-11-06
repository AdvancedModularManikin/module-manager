#############################
# CMake Mod Manager root/src
#############################

set(MODULE_MANAGER_SOURCES ModuleManagerMain.cpp ModuleManager.cpp)

add_executable(amm_module_manager ${MODULE_MANAGER_SOURCES})

target_link_libraries(amm_module_manager
        PUBLIC amm_std
        ${SQLite3_LIBRARIES}
        ${TinyXML2_LIBRARIES}
        ${Boost_LIBRARIES}
        )

install(TARGETS amm_module_manager RUNTIME DESTINATION bin)
install(DIRECTORY ../config DESTINATION bin)
