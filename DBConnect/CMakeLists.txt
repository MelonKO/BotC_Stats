cmake_minimum_required(VERSION 3.25)

project(DBConnect VERSION 1.1.0 LANGUAGES CXX)

# Keep executable and generated runtime configuration files beside each other.
# Presets set this explicitly; the fallback also supports direct CMake invocations.
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif ()

# ─── Qt6 ──────────────────────────────────────────────────────────────────────
# Find Qt6 packages via vcpkg toolchain (Core and Widgets are sufficient for basic GUI)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network LinguistTools)
# Apply Qt6 default project settings (enables AUTOMOC, AUTOUIC, AUTORCC automatically)
# and declare the translation direction: English source strings, Russian catalog.
qt_standard_project_setup(
        I18N_SOURCE_LANGUAGE en
        I18N_TRANSLATED_LANGUAGES ru
)

set(CMAKE_AUTORCC ON)

add_subdirectory(generated_api)
# The generated API client is not part of the UI: keep its technical messages
# out of the translator catalog.
set_property(TARGET BotcStats PROPERTY QT_EXCLUDE_FROM_TRANSLATION ON)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(SOURCES
        resources/app_icon.qrc
        src/main.cpp
        src/api/BotCApiClient.cpp
        src/api/BotCApiClient.h
        src/api/BotCNetworkAccessManager.cpp
        src/api/BotCNetworkAccessManager.h
        src/config/ConfigManager.cpp
        src/config/ConfigManager.h
        src/i18n/LocalizationManager.cpp
        src/i18n/LocalizationManager.h
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
        src/ui/PlayersWidget.cpp
        src/ui/PlayersWidget.h
        src/ui/PlayersWidget.ui
        src/ui/PlayersTableModel.cpp
        src/ui/PlayersTableModel.h
        src/ui/RolesWidget.cpp
        src/ui/RolesWidget.h
        src/ui/RolesWidget.ui
        src/ui/RolesTableModel.cpp
        src/ui/RolesTableModel.h
        src/ui/RoleEditDialog.cpp
        src/ui/RoleEditDialog.h
        src/ui/RoleEditDialog.ui
        src/ui/NetworkErrorMessages.cpp
        src/ui/NetworkErrorMessages.h
        src/utils/csv/CsvParser.cpp
        src/utils/csv/CsvParser.h
        src/db_cache/DBCache.cpp
        src/db_cache/DBCache.h
)

# WIN32 remove console window in GUI-app
if (WIN32)
    set(APP_TYPE WIN32)
    enable_language(RC)
    list(APPEND SOURCES resources/app.rc)
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

# ─── Translations ─────────────────────────────────────────────────────────────
# The .qm catalogs are compiled by lrelease and embedded into the executable
# under the "/i18n" resource prefix, which LocalizationManager reads. Running
# `update_translations` refreshes the .ts files with lupdate.
set_source_files_properties(translations/dbconnect_ru.ts
        PROPERTIES QT_I18N_TRANSLATED_LANGUAGES ru)
qt_add_translations(TARGETS DBConnect
        SOURCE_TARGETS DBConnect
        TS_FILES translations/dbconnect_ru.ts
        RESOURCE_PREFIX "/i18n"
        MERGE_QT_TRANSLATIONS)

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
# config.ini holds local connection settings (including the API key) and is
# git-ignored. Fresh clones fall back to the tracked config.ini.example, so the
# configure step never fails; real settings are entered on the Settings tab.
set(BOTC_CONFIG_INI "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
if (NOT EXISTS "${BOTC_CONFIG_INI}")
    message(STATUS "config.ini not found — using config.ini.example as the initial config")
    set(BOTC_CONFIG_INI "${CMAKE_CURRENT_SOURCE_DIR}/config.ini.example")
endif ()
configure_file("${BOTC_CONFIG_INI}" "${CMAKE_CURRENT_BINARY_DIR}/config.ini" COPYONLY)

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
                # Copying config.ini generated at configure time
                COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_BINARY_DIR}/config.ini"
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
