cmake_minimum_required(VERSION 3.16)
project(aw-watcher-afk VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

set(AFK_SOURCES
    src/main.c
    src/afk.c
    src/http.c
    src/auth.c
    src/dirs.c
)

if(WIN32)
    list(APPEND AFK_SOURCES src/idle_windows.c src/window_windows.c)
elseif(APPLE)
    list(APPEND AFK_SOURCES src/idle_macos.c src/window_macos.c)
else()
    find_package(X11 REQUIRED)
    if(NOT X11_Xss_FOUND)
        message(FATAL_ERROR "XScreenSaver development files are required (libxss-dev)")
    endif()
    list(APPEND AFK_SOURCES src/idle_linux.c src/window_linux.c)
endif()

add_executable(komutracker ${AFK_SOURCES})
target_include_directories(komutracker PRIVATE src)
target_link_libraries(komutracker PRIVATE CURL::libcurl)

if(WIN32)
    target_link_libraries(komutracker PRIVATE ws2_32 bcrypt shell32)
elseif(APPLE)
    find_library(APPLICATION_SERVICES ApplicationServices REQUIRED)
    target_link_libraries(komutracker PRIVATE ${APPLICATION_SERVICES})
else()
    target_include_directories(komutracker PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(komutracker PRIVATE ${X11_LIBRARIES} ${X11_Xss_LIB})
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(afk-state-tests tests/test_state.c src/afk.c)
    target_include_directories(afk-state-tests PRIVATE src)
    add_test(NAME afk-state-tests COMMAND afk-state-tests)

    add_executable(auth-tests tests/test_auth.c src/http.c)
    target_include_directories(auth-tests PRIVATE src)
    target_link_libraries(auth-tests PRIVATE CURL::libcurl)
    add_test(NAME auth-tests COMMAND auth-tests)

    add_executable(window-tests tests/test_window.c src/http.c)
    target_include_directories(window-tests PRIVATE src)
    target_link_libraries(window-tests PRIVATE CURL::libcurl)
    add_test(NAME window-tests COMMAND window-tests)
endif()

install(TARGETS komutracker RUNTIME DESTINATION bin)
