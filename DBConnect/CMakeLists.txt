cmake_minimum_required(VERSION 4.2)
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
)

# Link Qt6 libraries to the target
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)