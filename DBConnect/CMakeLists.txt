cmake_minimum_required(VERSION 3.20)

project(DBConnect VERSION 1.0.0 LANGUAGES CXX)

# ─── Qt6 ──────────────────────────────────────────────────────────────────────
# Find Qt6 packages via vcpkg toolchain (Core and Widgets are sufficient for basic GUI)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
# Apply Qt6 default project settings (enables AUTOMOC, AUTOUIC, AUTORCC automatically)
qt_standard_project_setup()

add_subdirectory(generated_api)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(SOURCES
        src/main.cpp
        src/api/BotCApiClient.cpp
        src/api/BotCApiClient.h
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
        src/utils/games/parser/GamesCsvParser.cpp
        src/utils/games/parser/GamesCsvParser.h
        src/ui/ImportGamesWidget.cpp
        src/ui/ImportGamesWidget.h
        src/ui/ImportGamesWidget.ui
        src/utils/csv/CsvParser.cpp
        src/utils/csv/CsvParser.h
        src/db_cache/DBCache.cpp
        src/db_cache/DBCache.h
)

# WIN32 remove console window in GUI-app
if (WIN32)
    set(APP_TYPE WIN32)
endif ()

add_executable(DBConnect ${APP_TYPE} ${SOURCES})
target_include_directories(DBConnect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(DBConnect PRIVATE cxx_std_23)

# Link Qt6 libraries to the target
target_link_libraries(DBConnect PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        BotcStats)

# ─── Определяем путь к vcpkg_installed ────────────────────────────────────────
# Priority: the explicitly set VCPKG_INSTALLED_DIR from the preset.
# Otherwise, look for it next to the binary directory (old vcpkg behavior).
if (DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}")
    set(VCPKG_INST "${VCPKG_INSTALLED_DIR}")
elseif (EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    set(VCPKG_INST "${CMAKE_SOURCE_DIR}/vcpkg_installed")
else ()
    message(FATAL_ERROR "vcpkg_installed not found")
endif ()

message(STATUS "vcpkg_installed path: ${VCPKG_INST}")

# ─── qt.conf ──────────────────────────────────────────────────────────────────
# Only needed for dev builds: specifies where to look for Qt plugins (platforms/ etc.).
# In the final distribution, windeployqt places the plugins next to the exe.
if (DEFINED VCPKG_INST)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(QT_PREFIX "${VCPKG_INST}/x64-windows/debug")
    else ()
        set(QT_PREFIX "${VCPKG_INST}/x64-windows")
    endif ()

    configure_file(qt.conf.in "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qt.conf" @ONLY)
endif ()

# ─── config.ini ───────────────────────────────────────────────────────────────
configure_file(config.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini COPYONLY)

# ─── Deploy target (только Windows, только Release) ───────────────────────────
# Launch: cmake --build . --target deploy
# It collects a self-contained dist/ folder with exe + all Qt DLLs + plugins.
if (WIN32)
    find_program(WINDEPLOYQT windeployqt
            HINTS
            "${VCPKG_INST}/x64-windows/tools/Qt6/bin"
            "${Qt6_DIR}/../../../tools/Qt6/bin"
            "${Qt6_DIR}/../../../bin"
    )
    find_program(WINDEPLOYQT_DEBUG windeployqt.debug.bat
            HINTS
            "${VCPKG_INST}/x64-windows/tools/Qt6/bin"
            "${Qt6_DIR}/../../../tools/Qt6/bin"
            "${Qt6_DIR}/../../../bin"
    )

    if (WINDEPLOYQT AND WINDEPLOYQT_DEBUG)
        message(STATUS "windeployqt found: ${WINDEPLOYQT}")
        message(STATUS "windeployqt.debug found: ${WINDEPLOYQT_DEBUG}")

        add_custom_target(deploy
                DEPENDS ${PROJECT_NAME}
                # make dist/
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_BINARY_DIR}/dist"
                # Copying .exe
                COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:${PROJECT_NAME}>"
                "${CMAKE_BINARY_DIR}/dist/"
                # Copying config.ini
                COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
                "${CMAKE_BINARY_DIR}/dist/"
                # vcpkg applocal places third-party runtime DLLs (zlib, pcre2,
                # harfbuzz, openssl, ...) next to the built exe; windeployqt
                # only knows about Qt's own DLLs, so copy them explicitly
                COMMAND ${CMAKE_COMMAND}
                -DSRC_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>
                -DVCPKG_BIN_DIR=${VCPKG_INST}/x64-windows$<$<CONFIG:Debug>:/debug>/bin
                -DDST_DIR=${CMAKE_BINARY_DIR}/dist
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
                # windeployqt copy all Qt's DLLs and plugins into dist/
                COMMAND $<IF:$<CONFIG:Debug>,${WINDEPLOYQT_DEBUG},${WINDEPLOYQT}>
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --dir "${CMAKE_BINARY_DIR}/dist"
                "${CMAKE_BINARY_DIR}/dist/${PROJECT_NAME}.exe"
                COMMENT "Packaging into ${CMAKE_BINARY_DIR}/dist/ ..."
        )
    else ()
        message(WARNING "windeployqt not found — 'deploy' target is unavailable")
    endif ()
endif ()