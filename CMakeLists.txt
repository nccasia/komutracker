cmake_minimum_required(VERSION 3.16)
project(komutracker VERSION 3.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
include(GNUInstallDirs)

set(KOMUTRACKER_COMMON_SOURCES
    src/config.c
    src/tracker.c
    src/afk.c
    src/http.c
    src/auth.c
    src/dirs.c
    src/instance.c
)

if(WIN32)
    set(KOMUTRACKER_PLATFORM_SOURCES src/idle_windows.c src/window_windows.c)
elseif(APPLE)
    enable_language(OBJC)
    set(KOMUTRACKER_PLATFORM_SOURCES src/idle_macos.c src/window_macos.c)
else()
    find_package(X11 REQUIRED)
    if(NOT X11_Xss_FOUND)
        message(FATAL_ERROR "XScreenSaver development files are required (libxss-dev)")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
    if(WAYLAND_CLIENT_FOUND)
        set(KOMUTRACKER_PLATFORM_SOURCES
            src/idle_linux.c
            src/window_linux.c
            src/ext_idle_notify_v1_protocol.c
        )
    else()
        set(KOMUTRACKER_PLATFORM_SOURCES src/idle_linux.c src/window_linux.c)
    endif()
endif()

function(configure_komutracker_target target)
    target_include_directories(${target} PRIVATE src)
    target_compile_definitions(${target} PRIVATE KOMUTRACKER_VERSION="${PROJECT_VERSION}")
    target_link_libraries(${target} PRIVATE CURL::libcurl Threads::Threads)

    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32 bcrypt shell32)
    elseif(APPLE)
        find_library(APPLICATION_SERVICES ApplicationServices REQUIRED)
        target_link_libraries(${target} PRIVATE ${APPLICATION_SERVICES})
    else()
        target_include_directories(${target} PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(${target} PRIVATE ${X11_LIBRARIES} ${X11_Xss_LIB})
        if(WAYLAND_CLIENT_FOUND)
            target_link_libraries(${target} PRIVATE PkgConfig::WAYLAND_CLIENT)
            target_compile_definitions(${target} PRIVATE KOMUTRACKER_HAVE_WAYLAND=1)
        endif()
    endif()
endfunction()

# Preserve the command-line client for diagnostics and automation.
add_executable(komutracker
    src/main.c
    ${KOMUTRACKER_COMMON_SOURCES}
    ${KOMUTRACKER_PLATFORM_SOURCES}
)
configure_komutracker_target(komutracker)
if(WIN32)
    set_target_properties(komutracker PROPERTIES OUTPUT_NAME "komutracker-cli")
endif()

# Desktop/tray application. It deliberately has no normal application window.
if(WIN32)
    add_executable(komutracker_desktop WIN32
        src/desktop.c
        src/tray_windows.c
        packaging/windows/KomuTracker.rc
        ${KOMUTRACKER_COMMON_SOURCES}
        ${KOMUTRACKER_PLATFORM_SOURCES}
    )
    configure_komutracker_target(komutracker_desktop)
    set_target_properties(komutracker_desktop PROPERTIES OUTPUT_NAME "KomuTracker")
    install(TARGETS komutracker_desktop komutracker RUNTIME DESTINATION .)
elseif(APPLE)
    configure_file(packaging/macos/Info.plist.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)
    set(MACOS_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/media/logo_tray.png
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/KomuTracker.icns
    )
    set_source_files_properties(${MACOS_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    add_executable(komutracker_desktop MACOSX_BUNDLE
        src/desktop.c
        src/desktop_entry.c
        src/tray_macos.m
        ${KOMUTRACKER_COMMON_SOURCES}
        ${KOMUTRACKER_PLATFORM_SOURCES}
        ${MACOS_RESOURCES}
    )
    configure_komutracker_target(komutracker_desktop)
    find_library(COCOA Cocoa REQUIRED)
    target_link_libraries(komutracker_desktop PRIVATE ${COCOA})
    set_target_properties(komutracker_desktop PROPERTIES
        OUTPUT_NAME "KomuTracker"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "KomuTracker"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.komu.KomuTracker"
        MACOSX_BUNDLE_ICON_FILE "KomuTracker.icns"
    )
    add_custom_command(TARGET komutracker_desktop POST_BUILD
        COMMAND /usr/bin/xattr -cr $<TARGET_BUNDLE_DIR:komutracker_desktop>
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:komutracker_desktop>
        COMMENT "Applying an ad-hoc signature to the macOS app bundle")
    install(TARGETS komutracker_desktop BUNDLE DESTINATION .)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    pkg_check_modules(APPINDICATOR QUIET IMPORTED_TARGET ayatana-appindicator3-0.1)
    if(APPINDICATOR_FOUND)
        set(APPINDICATOR_TARGET PkgConfig::APPINDICATOR)
        set(APPINDICATOR_DEFINITION KOMUTRACKER_USE_AYATANA=1)
    else()
        pkg_check_modules(APPINDICATOR REQUIRED IMPORTED_TARGET appindicator3-0.1)
        set(APPINDICATOR_TARGET PkgConfig::APPINDICATOR)
    endif()

    add_executable(komutracker_desktop
        src/desktop.c
        src/desktop_entry.c
        src/tray_linux.c
        ${KOMUTRACKER_COMMON_SOURCES}
        ${KOMUTRACKER_PLATFORM_SOURCES}
    )
    configure_komutracker_target(komutracker_desktop)
    target_compile_definitions(komutracker_desktop PRIVATE
        ${APPINDICATOR_DEFINITION}
        KOMUTRACKER_TRAY_ICON_PATH="${CMAKE_INSTALL_FULL_DATADIR}/pixmaps/komutracker-tray.png"
    )
    target_link_libraries(komutracker_desktop PRIVATE PkgConfig::GTK3 ${APPINDICATOR_TARGET})
    set_target_properties(komutracker_desktop PROPERTIES OUTPUT_NAME "komutracker-desktop")

    install(TARGETS komutracker_desktop komutracker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/com.komu.KomuTracker.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/media/logo_app.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
            RENAME com.komu.KomuTracker.png)
    install(FILES packaging/media/logo_tray.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
            RENAME komutracker-tray.png)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(afk-state-tests tests/test_state.c src/afk.c)
    target_include_directories(afk-state-tests PRIVATE src)
    add_test(NAME afk-state-tests COMMAND afk-state-tests)

    add_executable(auth-tests tests/test_auth.c src/http.c src/auth.c src/dirs.c)
    target_include_directories(auth-tests PRIVATE src)
    target_link_libraries(auth-tests PRIVATE CURL::libcurl)
    if(WIN32)
        target_link_libraries(auth-tests PRIVATE bcrypt shell32)
    endif()
    add_test(NAME auth-tests COMMAND auth-tests)

    add_executable(window-tests tests/test_window.c src/http.c)
    target_include_directories(window-tests PRIVATE src)
    target_link_libraries(window-tests PRIVATE CURL::libcurl)
    add_test(NAME window-tests COMMAND window-tests)
endif()

if(WIN32)
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_FILE_NAME "KomuTracker-${PROJECT_VERSION}-windows-x64")
elseif(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_FILE_NAME "KomuTracker-${PROJECT_VERSION}-macOS")
else()
    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_FILE_NAME "komutracker-${PROJECT_VERSION}-ubuntu-amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Komu")
    set(CPACK_DEBIAN_PACKAGE_SECTION utils)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6, libcurl4, libx11-6, libxss1, libgtk-3-0, libayatana-appindicator3-1")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
endif()
set(CPACK_PACKAGE_NAME "KomuTracker")
set(CPACK_PACKAGE_VENDOR "Komu")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native active and AFK time tracker")
include(CPack)
