cmake_minimum_required(VERSION 3.14)
project(dsaudio C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vendored UI library. Examples and tests are not needed by this app.
set(CUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/CUI)
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/CUI/cmake/CuiApp.cmake)

# libcurl: the system copy first (so a release binary runs on a clean machine),
# Homebrew's only when the system has no development headers.
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    if(EXISTS /usr/local/opt/curl/include/curl/curl.h)
        set(CURL_ROOT /usr/local/opt/curl)
    elseif(EXISTS /opt/homebrew/opt/curl/include/curl/curl.h)
        set(CURL_ROOT /opt/homebrew/opt/curl)
    endif()
    find_package(CURL REQUIRED)
endif()

# --- shared core: one code path for the app, the CLI and the MCP server -------
add_library(ds_core STATIC
    src/core/ds_buf.c
    src/core/ds_json.cpp
    src/core/ds_http.c
    src/core/ds_config.c
    src/core/ds_keychain.c
    src/core/ds_tts.c
    src/core/ds_thread.c
    src/core/ds_tts_cache.c
    src/core/ds_asr.c
    src/core/ds_audio.c
    src/core/ds_account.c
    src/core/ds_player.c
    src/core/ds_log.c
    src/core/ds_i18n.c
    src/core/ds_job.c
    src/core/ds_media.c
    src/core/ds_mcp.c
    vendor/simdjson/simdjson.cpp)

target_include_directories(ds_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/simdjson
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio)

target_link_libraries(ds_core PUBLIC CURL::libcurl ${CMAKE_THREAD_LIBS_INIT})
if(APPLE)
    target_link_libraries(ds_core PUBLIC
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework Security")
elseif(WIN32)
    target_link_libraries(ds_core PUBLIC ws2_32 crypt32)
elseif(UNIX)
    # miniaudio's Linux backend is ALSA; it also needs dl and m.
    target_link_libraries(ds_core PUBLIC asound ${CMAKE_DL_LIBS} m)
endif()

enable_language(C)

# --- command line tool: the core without the window --------------------------
add_executable(dsaudio-cli src/cli/main.c)
target_link_libraries(dsaudio-cli PRIVATE ds_core)

# --- the CUI workbench -------------------------------------------------------
add_executable(dsaudio
    src/app/main.c
    src/app/fonts_cjk.c
    src/app/app_state.c
    src/app/ui_tts.c
    src/app/ui_asr.c
    src/app/ui_settings.c
    src/app/ui_mcp.c)
target_include_directories(dsaudio PRIVATE src/app)
target_link_libraries(dsaudio PRIVATE ds_core cui::cui)
cui_app_target(dsaudio)

# --- MCP server --------------------------------------------------------------
add_executable(dsaudio-mcp src/mcp/mcp_main.c)
target_link_libraries(dsaudio-mcp PRIVATE ds_core)
if(WIN32)
    set_target_properties(dsaudio-mcp PROPERTIES WIN32_EXECUTABLE OFF)
endif()

# --- offline tests: pure logic, no network, no key ---------------------------
# MSVC reads sources as the local codepage unless told otherwise; the CJK
# billing test depends on UTF-8 bytes.
if(MSVC)
    add_compile_options(/utf-8)
endif()

enable_testing()
add_executable(dsaudio-test tests/test_core.c)
target_link_libraries(dsaudio-test PRIVATE ds_core)
add_test(NAME core COMMAND dsaudio-test)
