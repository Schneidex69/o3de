#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_BUILD_HOST_TOOLS)

    include(${LY_ROOT_FOLDER}/Code/Tools/SerializeContextTools/Platform/${PAL_PLATFORM_NAME}/PAL_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)
    if (PAL_TRAIT_BUILD_SERIALIZECONTEXTTOOLS)
        list(APPEND additional_dependencies AZ::SerializeContextTools) # test_CLITool_SerializeContextTools depends on it
    endif()
    list(APPEND additional_dependencies AZ::AssetBundlerBatch) # test_CLITool_AssetBundlerBatch_Works depends on it

    ly_add_pytest(
        NAME AutomatedTesting::SmokeTest
        TEST_SUITE smoke
        TEST_SERIAL
        PATH ${CMAKE_CURRENT_LIST_DIR}
        PYTEST_MARKS "SUITE_smoke"
        TIMEOUT 1500
        RUNTIME_DEPENDENCIES
            AZ::AssetProcessor
            AZ::PythonBindingsExample
            Legacy::Editor
            AutomatedTesting.GameLauncher
            AutomatedTesting.Assets
            ${aditional_dependencies}
        COMPONENT
            Smoke
    )

    ly_add_pytest(
        NAME AutomatedTesting::SandboxTest
        TEST_SUITE sandbox
        TEST_SERIAL
        PATH ${CMAKE_CURRENT_LIST_DIR}
        PYTEST_MARKS "SUITE_sandbox"
        TIMEOUT 1500
        RUNTIME_DEPENDENCIES
            AZ::AssetProcessor
            AZ::PythonBindingsExample
            Legacy::Editor
            AutomatedTesting.GameLauncher
            AutomatedTesting.Assets
        COMPONENT
            Sandbox
    )

    ly_add_pytest(
        NAME AutomatedTesting::EditorTestWithGPU
        TEST_REQUIRES gpu
        PATH ${CMAKE_CURRENT_LIST_DIR}/test_Editor_NewExistingLevels_Works.py
        TIMEOUT 100
        RUNTIME_DEPENDENCIES
            AZ::AssetProcessor
            AZ::PythonBindingsExample
            Legacy::Editor
            AutomatedTesting.GameLauncher
            AutomatedTesting.Assets
    )

    ly_add_pytest(
        NAME AutomatedTesting::GameLauncherWithGPU
        TEST_SUITE sandbox
        TEST_REQUIRES gpu
        PATH ${CMAKE_CURRENT_LIST_DIR}/test_GameLauncher_EnterExitGameMode_Works.py
        TIMEOUT 100
        RUNTIME_DEPENDENCIES
            AZ::AssetProcessor
            AZ::PythonBindingsExample
            Legacy::Editor
            AutomatedTesting.GameLauncher
            AutomatedTesting.Assets
    )
endif()
