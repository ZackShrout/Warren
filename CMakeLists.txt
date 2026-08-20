cmake_minimum_required(VERSION 3.28)

set(WARREN_BUILD_ENVIRONMENT "host" CACHE STRING "Warren build environment")

if(WARREN_BUILD_ENVIRONMENT STREQUAL "system")
    project(Warren
        VERSION 0.0.0
        DESCRIPTION "BunnySoft Warren operating system image"
        LANGUAGES NONE
    )
else()
    project(Warren
        VERSION 0.0.0
        DESCRIPTION "BunnySoft Warren operating system"
        LANGUAGES C CXX ASM
    )
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

if(WARREN_BUILD_ENVIRONMENT STREQUAL "system")
    message(STATUS "Warren system-image orchestration: ${CMAKE_HOST_SYSTEM_NAME}")
elseif(CMAKE_CROSSCOMPILING)
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
    cmake/SystemImage.cmake
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
    docs/plans/burrow-loader.md
    docs/plans/burrow-first-entry.md
    docs/plans/aarch64-normalized-entry.md
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
    docs/specifications/AARCH64_BURROW_IMAGE.md
    docs/specifications/AARCH64_LOADER_HANDOFF.md
    docs/specifications/AARCH64_NORMALIZED_ENTRY.md
    docs/specifications/BOOT_INFORMATION_V1.md
    docs/specifications/TEST_RESULT_PROTOCOL_V1.md
    boot/Platform/QemuVirt/PostExitTestResult.s
    kernel/linker/AArch64/burrow.ld
    kernel/include/burrow/Core/TransitionPlan.h
    kernel/include/burrow/Core/KernelEntry.h
    kernel/src/Arch/AArch64/EmergencyVectors.S
    kernel/src/Arch/AArch64/Entry.S
    kernel/src/Arch/AArch64/Normalization.S
    kernel/src/Arch/AArch64/Activation.S
    kernel/include/burrow/Arch/AArch64/PageTables.h
    kernel/src/Arch/AArch64/PageTables.cpp
    kernel/src/Arch/AArch64/TablePreparation.cpp
    kernel/src/Arch/AArch64/TransitionPreparation.cpp
    kernel/src/Core/ImageLayout.cpp
    kernel/src/Core/KernelEntry.cpp
    kernel/src/Core/TransitionPlan.cpp
    kernel/src/Platform/QemuVirt/TestResult.S
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
    tools/verify_burrow_image.py
    tools/verify_esp_contents.py
    tools/verify_esp_reproducibility.py
    tools/verify_uefi_image.py
    tools/warren_test_protocol.py
    tests/host/BurrowImageVerifierTests.py
    tests/host/EspBuilderTests.py
    tests/host/TransitionPlanTests.cpp
    tests/host/AArch64PageTableTests.cpp
    tests/host/KernelEntryTests.cpp
    tests/abi/KernelEntryAbiCheck.c
    tests/abi/KernelEntryAbiCheck.cpp
    tests/host/WarrenTestProtocolTests.py
    tests/host/UefiSmokeHarnessTests.py
)

if(WARREN_BUILD_ENVIRONMENT STREQUAL "system")
    include(cmake/SystemImage.cmake)
    return()
endif()

if(NOT WARREN_BUILD_ENVIRONMENT STREQUAL "burrow")
    add_library(WarrenBurrowLoader OBJECT
        boot/include/warren/boot/AArch64Handoff.h
        boot/include/warren/boot/BootHandoffFinalization.h
        boot/include/warren/boot/BootHandoffStorage.h
        boot/include/warren/boot/BurrowLoader.h
        boot/Loader/AArch64Handoff.cpp
        boot/Loader/BootHandoffFinalization.cpp
        boot/Loader/BootHandoffStorage.cpp
        boot/Loader/BurrowLoader.cpp
    )
    target_include_directories(WarrenBurrowLoader PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/boot/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
    )
    target_compile_options(WarrenBurrowLoader PRIVATE
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
endif()

add_library(WarrenBootProtocol OBJECT
    sdk/include/warren/boot/BootInformation.h
    sdk/include/warren/boot/BootInformationProducer.h
    sdk/include/warren/boot/BootInformationValidation.h
    sdk/src/boot/BootInformationProducer.cpp
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

add_library(WarrenKernelEntryAbiCheck OBJECT
    tests/abi/KernelEntryAbiCheck.c
    tests/abi/KernelEntryAbiCheck.cpp
)
target_include_directories(WarrenKernelEntryAbiCheck PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel/include"
)
target_compile_options(WarrenKernelEntryAbiCheck PRIVATE
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

    add_library(WarrenTransitionPlan OBJECT
        kernel/include/burrow/Core/TransitionPlan.h
        kernel/src/Core/TransitionPlan.cpp
    )
    target_include_directories(WarrenTransitionPlan PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
    )
    target_compile_options(WarrenTransitionPlan PRIVATE
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

    add_executable(WarrenTransitionPlanTests
        tests/host/TransitionPlanTests.cpp
    )
    target_link_libraries(WarrenTransitionPlanTests PRIVATE
        WarrenTransitionPlan
        WarrenBootProtocol
    )
    target_compile_options(WarrenTransitionPlanTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenTransitionPlan
        COMMAND WarrenTransitionPlanTests
    )

    add_library(WarrenAArch64PageTables OBJECT
        kernel/include/burrow/Arch/AArch64/PageTables.h
        kernel/src/Arch/AArch64/PageTables.cpp
    )
    target_include_directories(WarrenAArch64PageTables PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
    )
    target_compile_options(WarrenAArch64PageTables PRIVATE
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

    add_executable(WarrenAArch64PageTableTests
        tests/host/AArch64PageTableTests.cpp
    )
    target_link_libraries(WarrenAArch64PageTableTests PRIVATE
        WarrenAArch64PageTables
    )
    target_compile_options(WarrenAArch64PageTableTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenAArch64PageTables
        COMMAND WarrenAArch64PageTableTests
    )

    add_library(WarrenKernelEntry OBJECT
        kernel/include/burrow/Core/KernelEntry.h
        kernel/src/Core/KernelEntry.cpp
    )
    target_include_directories(WarrenKernelEntry PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
    )
    target_compile_options(WarrenKernelEntry PRIVATE
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

    add_executable(WarrenKernelEntryTests
        tests/host/KernelEntryTests.cpp
    )
    target_link_libraries(WarrenKernelEntryTests PRIVATE
        WarrenKernelEntry
        WarrenBootProtocol
    )
    target_compile_options(WarrenKernelEntryTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenKernelEntry
        COMMAND WarrenKernelEntryTests
    )

    add_executable(WarrenBootInformationProducerTests
        tests/host/BootInformationProducerTests.cpp
    )
    target_link_libraries(WarrenBootInformationProducerTests PRIVATE
        WarrenBootProtocol
    )
    target_compile_options(WarrenBootInformationProducerTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenBootInformationProducer
        COMMAND WarrenBootInformationProducerTests
    )

    add_executable(WarrenBurrowLoaderTests
        tests/host/BurrowLoaderTests.cpp
    )
    target_link_libraries(WarrenBurrowLoaderTests PRIVATE
        WarrenBurrowLoader
        WarrenBootProtocol
    )
    target_compile_options(WarrenBurrowLoaderTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenBurrowLoader
        COMMAND WarrenBurrowLoaderTests
    )

    add_executable(WarrenBootHandoffStorageTests
        tests/host/BootHandoffStorageTests.cpp
    )
    target_link_libraries(WarrenBootHandoffStorageTests PRIVATE
        WarrenBurrowLoader
        WarrenBootProtocol
    )
    target_compile_options(WarrenBootHandoffStorageTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenBootHandoffStorage
        COMMAND WarrenBootHandoffStorageTests
    )

    add_executable(WarrenBootHandoffFinalizationTests
        tests/host/BootHandoffFinalizationTests.cpp
    )
    target_link_libraries(WarrenBootHandoffFinalizationTests PRIVATE
        WarrenBurrowLoader
        WarrenBootProtocol
    )
    target_compile_options(WarrenBootHandoffFinalizationTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenBootHandoffFinalization
        COMMAND WarrenBootHandoffFinalizationTests
    )

    add_executable(WarrenAArch64HandoffTests
        tests/host/AArch64HandoffTests.cpp
    )
    target_link_libraries(WarrenAArch64HandoffTests PRIVATE
        WarrenBurrowLoader
        WarrenBootProtocol
    )
    target_compile_options(WarrenAArch64HandoffTests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_test(
        NAME WarrenAArch64Handoff
        COMMAND WarrenAArch64HandoffTests
    )

    add_test(
        NAME WarrenTestResultProtocol
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/WarrenTestProtocolTests.py"
    )

    add_test(
        NAME WarrenUefiSmokeHarness
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/UefiSmokeHarnessTests.py"
    )

    add_test(
        NAME WarrenBurrowImageVerifier
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/BurrowImageVerifierTests.py"
    )

    add_test(
        NAME WarrenEspBuilderInputs
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/EspBuilderTests.py"
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

if(WARREN_BUILD_ENVIRONMENT STREQUAL "burrow")
    option(WARREN_ENABLE_QEMU_TEST_RESULT
        "Enable Burrow's QEMU-only first-entry result transport"
        OFF
    )
    set(WARREN_QEMU_TEST_RESULT_MODE "pass" CACHE STRING
        "Terminal result emitted by a QEMU test-enabled Burrow image"
    )
    set_property(CACHE WARREN_QEMU_TEST_RESULT_MODE PROPERTY STRINGS
        pass fail panic
    )
    set(WARREN_AARCH64_ENTRY_FAULT "none" CACHE STRING
        "Trusted QEMU-only AArch64 entry fault fixture"
    )
    set_property(CACHE WARREN_AARCH64_ENTRY_FAULT PROPERTY STRINGS
        none emergency
    )

    if(WARREN_ENABLE_QEMU_TEST_RESULT AND
       NOT WARREN_QEMU_TEST_RESULT_MODE MATCHES "^(pass|fail|panic)$")
        message(FATAL_ERROR
            "WARREN_QEMU_TEST_RESULT_MODE must be pass, fail, or panic"
        )
    endif()
    if(NOT WARREN_AARCH64_ENTRY_FAULT MATCHES "^(none|emergency)$")
        message(FATAL_ERROR
            "WARREN_AARCH64_ENTRY_FAULT must be none or emergency"
        )
    endif()
    if(NOT WARREN_AARCH64_ENTRY_FAULT STREQUAL "none" AND
       NOT WARREN_ENABLE_QEMU_TEST_RESULT)
        message(FATAL_ERROR
            "AArch64 entry fault injection requires QEMU test-result support"
        )
    endif()

    add_library(BurrowImageObjects OBJECT
        kernel/src/Arch/AArch64/Activation.S
        kernel/src/Arch/AArch64/EmergencyVectors.S
        kernel/src/Arch/AArch64/Entry.S
        kernel/src/Arch/AArch64/Normalization.S
        kernel/src/Arch/AArch64/PageTables.cpp
        kernel/src/Arch/AArch64/TablePreparation.cpp
        kernel/src/Arch/AArch64/TransitionPreparation.cpp
        kernel/src/Core/ImageLayout.cpp
        kernel/src/Core/KernelEntry.cpp
        kernel/src/Core/TransitionPlan.cpp
        sdk/src/boot/BootInformationValidation.cpp
    )
    add_dependencies(BurrowImageObjects WarrenKernelEntryAbiCheck)
    target_include_directories(BurrowImageObjects PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/kernel/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
    )
    if(WARREN_ENABLE_QEMU_TEST_RESULT)
        target_sources(BurrowImageObjects PRIVATE
            kernel/src/Platform/QemuVirt/TestResult.S
        )
        string(TOUPPER "${WARREN_QEMU_TEST_RESULT_MODE}" _burrow_result_mode_upper)
        set(_burrow_verifier_result_arguments
            --qemu-test-result "${WARREN_QEMU_TEST_RESULT_MODE}"
        )
    else()
        set(_burrow_result_mode_upper "PASS")
        set(_burrow_verifier_result_arguments)
    endif()
    if(WARREN_AARCH64_ENTRY_FAULT STREQUAL "emergency")
        set(_burrow_verifier_fault_arguments
            --aarch64-entry-fault emergency
        )
    else()
        set(_burrow_verifier_fault_arguments)
    endif()
    target_compile_options(BurrowImageObjects PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:CXX>:-fpie>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:CXX>:-fvisibility=hidden>
        $<$<COMPILE_LANGUAGE:CXX>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:CXX>:-fdata-sections>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
        $<$<COMPILE_LANGUAGE:CXX>:-Werror>
        $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:CXX>:-mgeneral-regs-only>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-asynchronous-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-O0>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-gdwarf-5>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-fdebug-compilation-dir=.>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:-O2>
    )
    target_compile_definitions(BurrowImageObjects PRIVATE
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:NDEBUG>
        $<$<AND:$<COMPILE_LANGUAGE:ASM>,$<BOOL:${WARREN_ENABLE_QEMU_TEST_RESULT}>>:WARREN_ENABLE_QEMU_TEST_RESULT=1>
        $<$<AND:$<COMPILE_LANGUAGE:ASM>,$<BOOL:${WARREN_ENABLE_QEMU_TEST_RESULT}>>:WARREN_QEMU_TEST_RESULT_${_burrow_result_mode_upper}=1>
        $<$<AND:$<COMPILE_LANGUAGE:ASM>,$<STREQUAL:${WARREN_AARCH64_ENTRY_FAULT},emergency>>:WARREN_AARCH64_FAULT_INJECT_EMERGENCY=1>
    )

    set(_burrow_artifact_directory "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
    set(_burrow_symbol_image "${_burrow_artifact_directory}/burrow.elf")
    set(_burrow_runtime_image "${_burrow_artifact_directory}/burrow-runtime.elf")
    set(_burrow_map "${_burrow_artifact_directory}/burrow.map")

    add_custom_command(
        OUTPUT
            "${_burrow_symbol_image}"
            "${_burrow_runtime_image}"
            "${_burrow_map}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_burrow_artifact_directory}"
        COMMAND "${WARREN_LD_LLD}"
            -shared
            --hash-style=gnu
            --no-dynamic-linker
            --entry=burrow_aarch64_entry
            --script=${CMAKE_CURRENT_SOURCE_DIR}/kernel/linker/AArch64/burrow.ld
            --gc-sections
            --build-id=none
            --no-undefined
            --fatal-warnings
            --pack-dyn-relocs=none
            -z
            text
            -z
            max-page-size=4096
            -z
            common-page-size=4096
            -z
            noexecstack
            --Map=${_burrow_map}
            -o
            "${_burrow_symbol_image}"
            $<TARGET_OBJECTS:BurrowImageObjects>
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_burrow_image.py"
            --objdump "${CMAKE_OBJDUMP}"
            ${_burrow_verifier_result_arguments}
            ${_burrow_verifier_fault_arguments}
            --image "${_burrow_symbol_image}"
        COMMAND "${CMAKE_OBJCOPY}"
            --strip-debug
            "${_burrow_symbol_image}"
            "${_burrow_runtime_image}"
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_burrow_image.py"
            --objdump "${CMAKE_OBJDUMP}"
            ${_burrow_verifier_result_arguments}
            ${_burrow_verifier_fault_arguments}
            --runtime
            --image "${_burrow_runtime_image}"
            --reference-image "${_burrow_symbol_image}"
        DEPENDS
            BurrowImageObjects
            $<TARGET_OBJECTS:BurrowImageObjects>
            kernel/linker/AArch64/burrow.ld
            tools/verify_burrow_image.py
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(BurrowImage ALL
        DEPENDS
            "${_burrow_symbol_image}"
            "${_burrow_runtime_image}"
            "${_burrow_map}"
    )

    add_custom_target(BurrowDisassembly
        COMMAND "${CMAKE_OBJDUMP}"
            --disassemble
            --demangle
            --source
            "${_burrow_symbol_image}"
        DEPENDS BurrowImage
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )

    unset(_burrow_artifact_directory)
    unset(_burrow_symbol_image)
    unset(_burrow_runtime_image)
    unset(_burrow_map)
    unset(_burrow_result_mode_upper)
    unset(_burrow_verifier_fault_arguments)
    unset(_burrow_verifier_result_arguments)
endif()

if(WARREN_BUILD_ENVIRONMENT STREQUAL "uefi")
    set(WARREN_QEMU_HANDOFF_FAULT "none" CACHE STRING
        "Trusted QEMU-only UEFI handoff fault fixture"
    )
    set_property(CACHE WARREN_QEMU_HANDOFF_FAULT PROPERTY STRINGS
        none boot-magic console-record post-exit
    )
    if(NOT WARREN_QEMU_HANDOFF_FAULT MATCHES "^(none|boot-magic|console-record|post-exit)$")
        message(FATAL_ERROR
            "WARREN_QEMU_HANDOFF_FAULT must be none, boot-magic, console-record, or post-exit"
        )
    endif()

    add_library(WarrenAArch64HandoffAssembly OBJECT
        boot/Arch/AArch64/Handoff.s
    )
    target_compile_options(WarrenAArch64HandoffAssembly PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    add_library(WarrenUefiBootloaderObject OBJECT
        boot/Arch/AArch64/Handoff.cpp
        boot/uefi/Main.cpp
        boot/uefi/Memory.cpp
        boot/uefi/UefiHandoffFinalization.cpp
        boot/uefi/UefiHandoffStorage.cpp
        boot/uefi/UefiFile.cpp
        boot/include/warren/boot/UefiHandoffFinalization.h
        boot/include/warren/boot/UefiHandoffStorage.h
        boot/include/warren/boot/Uefi.h
        boot/include/warren/boot/UefiFile.h
    )

    target_include_directories(WarrenUefiBootloaderObject
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/boot/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
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
        -fno-builtin
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

    set(_warren_qemu_post_exit_result_objects)
    set(_warren_qemu_post_exit_result_targets)
    set(_warren_uefi_verifier_test_arguments)
    if(WARREN_QEMU_HANDOFF_FAULT STREQUAL "boot-magic")
        target_compile_definitions(WarrenUefiBootloaderObject PRIVATE
            WARREN_QEMU_HANDOFF_FAULT_BOOT_MAGIC=1
        )
    elseif(WARREN_QEMU_HANDOFF_FAULT STREQUAL "console-record")
        target_compile_definitions(WarrenUefiBootloaderObject PRIVATE
            WARREN_QEMU_HANDOFF_FAULT_CONSOLE_RECORD=1
        )
    elseif(WARREN_QEMU_HANDOFF_FAULT STREQUAL "post-exit")
        target_compile_definitions(WarrenUefiBootloaderObject PRIVATE
            WARREN_QEMU_HANDOFF_FAULT_POST_EXIT=1
            WARREN_ENABLE_QEMU_POST_EXIT_FAILURE_RESULT=1
        )
        add_library(WarrenQemuPostExitResultAssembly OBJECT
            boot/Platform/QemuVirt/PostExitTestResult.s
        )
        target_compile_options(WarrenQemuPostExitResultAssembly PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        list(APPEND _warren_qemu_post_exit_result_objects
            $<TARGET_OBJECTS:WarrenQemuPostExitResultAssembly>
        )
        list(APPEND _warren_qemu_post_exit_result_targets
            WarrenQemuPostExitResultAssembly
        )
        set(_warren_uefi_verifier_test_arguments
            --qemu-post-exit-failure-result
        )
    endif()

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
            /include:warren_aarch64_synchronize_instruction_range
            /include:warren_aarch64_handoff
            /include:warren_aarch64_post_exit_failure
            $<$<CONFIG:Debug>:/debug:dwarf>
            "/out:${_warren_uefi_image}"
            $<TARGET_OBJECTS:WarrenAArch64HandoffAssembly>
            $<TARGET_OBJECTS:WarrenBurrowLoader>
            $<TARGET_OBJECTS:WarrenBootProtocol>
            $<TARGET_OBJECTS:WarrenUefiBootloaderObject>
            ${_warren_qemu_post_exit_result_objects}
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_uefi_image.py"
            --readobj "${WARREN_LLVM_READOBJ}"
            --objdump "${WARREN_LLVM_OBJDUMP}"
            --handoff-object $<TARGET_OBJECTS:WarrenAArch64HandoffAssembly>
            ${_warren_uefi_verifier_test_arguments}
            --image "${_warren_uefi_image}"
        DEPENDS
            WarrenAArch64HandoffAssembly
            WarrenUefiBootloaderObject
            WarrenBurrowLoader
            WarrenBootProtocol
            $<TARGET_OBJECTS:WarrenAArch64HandoffAssembly>
            $<TARGET_OBJECTS:WarrenBurrowLoader>
            $<TARGET_OBJECTS:WarrenBootProtocol>
            $<TARGET_OBJECTS:WarrenUefiBootloaderObject>
            ${_warren_qemu_post_exit_result_targets}
            ${_warren_qemu_post_exit_result_objects}
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

    unset(_warren_qemu_post_exit_result_objects)
    unset(_warren_qemu_post_exit_result_targets)
    unset(_warren_uefi_verifier_test_arguments)

endif()
