cmake_minimum_required(VERSION 4.0.3)

project(DBConnect)

set(CMAKE_CXX_STANDARD 20)

# Find Qt6 packages via vcpkg toolchain (Core and Widgets are sufficient for basic GUI)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
# Apply Qt6 default project settings (enables AUTOMOC, AUTOUIC, AUTORCC automatically)
qt_standard_project_setup()

add_executable(DBConnect src/main.cpp
        src/api/BotCApiClient.cpp
        src/api/BotCApiClient.h
        src/api/models/GameImportRequest.h
        src/api/models/PlayerImportRequest.h
        src/api/models/RolesImportRequest.h
        src/api/models/HealthResponse.h
        src/api/models/GameImportResponse.h
        src/api/models/RolesResponse.h
        src/api/models/RolesImportResponse.h
        src/config/ConfigManager.cpp
        src/config/ConfigManager.h
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/ui/MainWindow.ui
        src/ui/SettingsWidget.cpp
        src/ui/SettingsWidget.h
        src/ui/SettingsWidget.ui
        src/ui/ImportRolesWidget.cpp
        src/ui/ImportRolesWidget.h
        src/ui/ImportRolesWidget.ui
        src/utils/roles/parser/RolesCsvParser.cpp
        src/utils/roles/parser/RolesCsvParser.h
)

# Link Qt6 libraries to the target
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network)

# ######### Copy files ##########
# Since we vcpkg qt we need to tell Qt where to find qml plugins and files
# and other paths like where to find the QtWebEngineProcess executable.
# This is only needed for dev builds
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    configure_file(qt.debug.conf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qt.conf COPYONLY)
else ()
    configure_file(qt.conf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qt.conf COPYONLY)
endif ()

configure_file(config.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini COPYONLY)