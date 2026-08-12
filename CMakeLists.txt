cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(ZeroJet VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(ZeroJet VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(ZEROJET_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(ZEROJET_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(ZEROJET_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(ZEROJET_CREATE_AU OFF)
if(APPLE)
    set(ZEROJET_CREATE_AU ON)
endif()
set(ZEROJET_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(zerojet_engine STATIC
    source/ZeroJetEngine.cpp)
target_include_directories(zerojet_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(zerojet_engine PUBLIC cxx_std_20)
set_target_properties(zerojet_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(zerojet_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ZEROJET_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(zerojet_engine_tests
        tests/ZeroJetEngineTests.cpp)
    target_link_libraries(zerojet_engine_tests PRIVATE zerojet_engine)
    target_compile_features(zerojet_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(zerojet_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(zerojet_engine_tests)
    add_test(NAME zerojet_engine_tests COMMAND zerojet_engine_tests)
endif()

if(ZEROJET_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${ZEROJET_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${ZEROJET_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/yup-ehl-design-module/CMakeLists.txt")
        message(FATAL_ERROR "Missing yup-ehl-design-module. Run: git submodule update --init")
    endif()
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/yup-ehl-design-module"
                     "${CMAKE_BINARY_DIR}/yup-ehl-design-module")

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME zerojet
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "ZeroJet"
        TARGET_APP_ID "jp.ehl.zerojet"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.zerojet"
        PLUGIN_NAME "ZeroJet"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Dual fractional-delay through-zero flanger"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/ZeroJet"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${ZEROJET_CREATE_AU}
        PLUGIN_AU_SUBTYPE "ZrJt"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(zerojet_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/ZeroJetPlugin.cpp
        source/ZeroJetPlugin.h)
    target_link_libraries(zerojet_shared INTERFACE zerojet_engine ehl::yup_plugin_ui)
    target_include_directories(zerojet_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(ZEROJET_BUILD_TESTS)
        add_executable(zerojet_plugin_bridge_tests
            tests/ZeroJetPluginBridgeTests.cpp
            source/ZeroJetPlugin.cpp)
        target_link_libraries(zerojet_plugin_bridge_tests PRIVATE zerojet_engine yup::yup_audio_processors)
        target_include_directories(zerojet_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(zerojet_plugin_bridge_tests PRIVATE ZEROJET_HEADLESS_TEST=1)
        target_compile_features(zerojet_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(zerojet_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(zerojet_plugin_bridge_tests)
        add_test(NAME zerojet_plugin_bridge_tests COMMAND zerojet_plugin_bridge_tests)

        add_executable(zerojet_standalone_bridge_tests
            tests/ZeroJetStandaloneBridgeTests.cpp
            source/ZeroJetPlugin.cpp)
        target_link_libraries(zerojet_standalone_bridge_tests PRIVATE zerojet_engine yup::yup_audio_processors)
        target_include_directories(zerojet_standalone_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(zerojet_standalone_bridge_tests PRIVATE
            ZEROJET_HEADLESS_TEST=1
            YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
        target_compile_features(zerojet_standalone_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(zerojet_standalone_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(zerojet_standalone_bridge_tests)
        add_test(NAME zerojet_standalone_bridge_tests COMMAND zerojet_standalone_bridge_tests)
    endif()

    set(zerojet_release_bundle_targets
        zerojet_standalone_plugin
        zerojet_vst3_plugin)
    if(APPLE)
        list(APPEND zerojet_release_bundle_targets zerojet_au_plugin)
    endif()

    add_custom_target(zerojet_release_bundles
        DEPENDS ${zerojet_release_bundle_targets})

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT ZeroJet
        SLUG zerojet
        STANDALONE_TARGET zerojet_standalone_plugin
        VST3_TARGET zerojet_vst3_plugin
        AU_TARGET zerojet_au_plugin)

    if(APPLE)
        set_target_properties(zerojet_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.zerojet"
            MACOSX_BUNDLE_BUNDLE_NAME "ZeroJet"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.zerojet")
    endif()

    if(APPLE AND ZEROJET_ADHOC_SIGN)
        find_program(ZEROJET_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET zerojet_vst3_plugin POST_BUILD
            COMMAND "${ZEROJET_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:zerojet_vst3_plugin>"
            COMMENT "Ad-hoc signing ZeroJet VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET zerojet_standalone_plugin POST_BUILD
            COMMAND "${ZEROJET_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:zerojet_standalone_plugin>"
            COMMENT "Ad-hoc signing ZeroJet standalone app for local launch"
            VERBATIM)
    endif()
endif()
