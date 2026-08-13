cmake_minimum_required(VERSION 3.28)

project(Warren
    VERSION 0.0.0
    DESCRIPTION "BunnySoft Warren operating system"
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WARREN_BUILD_ENVIRONMENT "host" CACHE STRING "Warren build environment")

enable_testing()

if(CMAKE_CROSSCOMPILING)
    message(STATUS "Warren target: ${CMAKE_SYSTEM_PROCESSOR} (${CMAKE_SYSTEM_NAME})")
else()
    message(STATUS "Warren host tools: ${CMAKE_HOST_SYSTEM_PROCESSOR} (${CMAKE_HOST_SYSTEM_NAME})")
endif()

# Keep policy, planning, and bootstrap inputs visible in IDEs without treating
# documentation or host scripts as target code.
add_custom_target(WarrenPlanning SOURCES
    .gitignore
    CODE_STANDARDS.md
    README.md
    CMakePresets.json
    LICENSE
    docs/ARCHITECTURE.md
    docs/BRANCHING.md
    docs/DECISIONS.md
    docs/DEVELOPMENT.md
    docs/FILESYSTEM.md
    docs/GUARDRAILS.md
    docs/ROADMAP.md
    docs/SELF_HOSTING.md
    docs/VISION.md
    docs/plans/burrow-image.md
    docs/plans/phase-0-contracts.md
    docs/adr/0000-template.md
    docs/adr/0001-reference-target.md
    docs/adr/0002-language-and-low-level-abi.md
    docs/adr/0003-initial-kernel-structure.md
    docs/adr/0004-compatibility-direction.md
    docs/adr/0005-warren-owned-bootloader.md
    docs/adr/0006-cmake-and-ninja-build.md
    docs/adr/0007-reference-aarch64-machine.md
    docs/adr/0008-uefi-loader-toolchain-and-headers.md
    docs/adr/0009-position-independent-burrow-image.md
    docs/adr/0010-boot-information-protocol-v1.md
    docs/adr/0011-aarch64-kernel-handoff.md
    docs/adr/0012-qemu-test-result-transport.md
    docs/adr/0013-project-license.md
    docs/adr/0014-toolchain-compatibility-baseline.md
    docs/adr/0015-boot-information-v1-layout.md
    docs/adr/0016-initial-aarch64-virtual-memory-layout.md
    docs/specifications/BOOT_INFORMATION_V1.md
    docs/specifications/TEST_RESULT_PROTOCOL_V1.md
    third_party/README.md
    third_party/edk2/LICENSE
    third_party/edk2/README.md
    third_party/edk2/headers.sha256
    tools/bootstrap.sh
    tools/build_esp.py
    tools/dependencies/macos-brew.txt
    tools/dependencies/fetch_edk2_headers.sh
    tools/dependencies/qemu-firmware.sha256
    tools/dependencies/ToolchainBaseline.cmake
    tools/dependencies/check_tool_versions.cmake
    tools/run_uefi_smoke.py
    tools/verify_esp_reproducibility.py
    tools/verify_uefi_image.py
    tools/warren_test_protocol.py
    tests/host/WarrenTestProtocolTests.py
)

add_library(WarrenBootProtocol OBJECT
    sdk/include/warren/boot/BootInformation.h
    sdk/include/warren/boot/BootInformationValidation.h
    sdk/src/boot/BootInformationValidation.cpp
)
target_include_directories(WarrenBootProtocol PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
)
target_compile_options(WarrenBootProtocol PRIVATE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -nostdinc++
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_library(WarrenBootInformationAbiCheck OBJECT
    tests/abi/BootInformationAbiCheck.c
    tests/abi/BootInformationAbiCheck.cpp
)
target_include_directories(WarrenBootInformationAbiCheck PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
)
target_compile_options(WarrenBootInformationAbiCheck PRIVATE
    -ffreestanding
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

if(NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 3.14...<3.15 REQUIRED COMPONENTS Interpreter)

    add_executable(WarrenBootInformationTests
        tests/host/BootInformationTests.cpp
    )
    target_link_libraries(WarrenBootInformationTests PRIVATE
        WarrenBootProtocol
    )
    target_compile_options(WarrenBootInformationTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenBootInformationProtocol
        COMMAND WarrenBootInformationTests
    )

    add_test(
        NAME WarrenTestResultProtocol
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/WarrenTestProtocolTests.py"
    )

    add_test(
        NAME WarrenToolchainKnownGoodBaseline
        COMMAND "${CMAKE_COMMAND}"
            -DWARREN_BASELINE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies/ToolchainBaseline.cmake
            -DWARREN_ACTUAL_CMAKE=4.2.3
            -DWARREN_ACTUAL_NINJA=1.13.2
            -DWARREN_ACTUAL_LLVM=22.1.8
            -DWARREN_ACTUAL_LLD=22.1.8
            -DWARREN_ACTUAL_QEMU=11.0.3
            -DWARREN_ACTUAL_MTOOLS=4.0.49
            -DWARREN_ACTUAL_PYTHON=3.14.6
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies/check_tool_versions.cmake"
    )

    add_test(
        NAME WarrenToolchainRejectsFutureLlvmMajor
        COMMAND "${CMAKE_COMMAND}"
            -DWARREN_BASELINE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies/ToolchainBaseline.cmake
            -DWARREN_ACTUAL_CMAKE=4.2.3
            -DWARREN_ACTUAL_NINJA=1.13.2
            -DWARREN_ACTUAL_LLVM=23.0.0
            -DWARREN_ACTUAL_LLD=23.0.0
            -DWARREN_ACTUAL_QEMU=11.0.3
            -DWARREN_ACTUAL_MTOOLS=4.0.49
            -DWARREN_ACTUAL_PYTHON=3.14.6
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies/check_tool_versions.cmake"
    )
    set_tests_properties(WarrenToolchainRejectsFutureLlvmMajor PROPERTIES
        WILL_FAIL TRUE
    )

    add_test(
        NAME WarrenToolchainRejectsLlvmLldMismatch
        COMMAND "${CMAKE_COMMAND}"
            -DWARREN_BASELINE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies/ToolchainBaseline.cmake
            -DWARREN_ACTUAL_CMAKE=4.2.3
            -DWARREN_ACTUAL_NINJA=1.13.2
            -DWARREN_ACTUAL_LLVM=22.1.8
            -DWARREN_ACTUAL_LLD=22.1.7
            -DWARREN_ACTUAL_QEMU=11.0.3
            -DWARREN_ACTUAL_MTOOLS=4.0.49
            -DWARREN_ACTUAL_PYTHON=3.14.6
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/dependencies/check_tool_versions.cmake"
    )
    set_tests_properties(WarrenToolchainRejectsLlvmLldMismatch PROPERTIES
        WILL_FAIL TRUE
    )
endif()

if(WARREN_BUILD_ENVIRONMENT STREQUAL "uefi")
    add_library(WarrenUefiBootloaderObject OBJECT
        boot/uefi/Main.cpp
        boot/include/warren/boot/Uefi.h
    )

    target_include_directories(WarrenUefiBootloaderObject
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/boot/include"
    )
    target_compile_options(WarrenUefiBootloaderObject PRIVATE
        "SHELL:-isystem ${WARREN_EDK2_INCLUDE}"
        "SHELL:-isystem ${WARREN_EDK2_INCLUDE}/AArch64"
        -ffreestanding
        -fno-exceptions
        -fno-rtti
        -fno-stack-protector
        -fno-threadsafe-statics
        -fno-use-cxa-atexit
        -nostdinc++
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-gdwarf>
        $<$<CONFIG:Release>:-O2>
    )
    target_compile_definitions(WarrenUefiBootloaderObject PRIVATE
        $<$<CONFIG:Release>:NDEBUG>
    )

    set(_warren_artifact_directory "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
    set(_warren_uefi_image "${_warren_artifact_directory}/BOOTAA64.EFI")
    set(_warren_esp_image "${_warren_artifact_directory}/warren-esp.img")

    add_custom_command(
        OUTPUT "${_warren_uefi_image}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
        COMMAND "${WARREN_LLD_LINK}"
            /machine:arm64
            /subsystem:efi_application
            /entry:efi_main
            /nodefaultlib
            /Brepro
            $<$<CONFIG:Debug>:/debug:dwarf>
            "/out:${_warren_uefi_image}"
            $<TARGET_OBJECTS:WarrenUefiBootloaderObject>
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_uefi_image.py"
            --readobj "${WARREN_LLVM_READOBJ}"
            --image "${_warren_uefi_image}"
        DEPENDS
            WarrenUefiBootloaderObject
            $<TARGET_OBJECTS:WarrenUefiBootloaderObject>
            tools/verify_uefi_image.py
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(WarrenUefiBootloader DEPENDS "${_warren_uefi_image}")

    add_custom_command(
        OUTPUT "${_warren_esp_image}"
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
            --mformat "${WARREN_MFORMAT}"
            --mcopy "${WARREN_MCOPY}"
            --bootloader "${_warren_uefi_image}"
            --output "${_warren_esp_image}"
        DEPENDS
            "${_warren_uefi_image}"
            tools/build_esp.py
        VERBATIM
    )

    add_custom_target(WarrenEsp ALL DEPENDS "${_warren_esp_image}")

    add_test(
        NAME WarrenUefiFirstLight
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
            --qemu "${WARREN_QEMU_AARCH64}"
            --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
            --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
            --esp "${_warren_esp_image}"
    )
    set_tests_properties(WarrenUefiFirstLight PROPERTIES TIMEOUT 40)

    add_test(
        NAME WarrenEspReproducibility
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_esp_reproducibility.py"
            --python "${WARREN_HOST_PYTHON}"
            --builder "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
            --mformat "${WARREN_MFORMAT}"
            --mcopy "${WARREN_MCOPY}"
            --bootloader "${_warren_uefi_image}"
    )
    set_tests_properties(WarrenEspReproducibility PROPERTIES TIMEOUT 20)

    add_custom_target(run-uefi
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -R WarrenUefiFirstLight
        DEPENDS WarrenEsp
        USES_TERMINAL
    )
endif()
