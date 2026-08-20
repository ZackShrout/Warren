# Warren combined AArch64 system-image orchestration.

set(_warren_local_paths "${CMAKE_CURRENT_SOURCE_DIR}/.warren/ToolchainPaths.cmake")
if(NOT EXISTS "${_warren_local_paths}")
    message(FATAL_ERROR
        "Warren's local toolchain configuration is missing. "
        "Run ./tools/bootstrap.sh --install from the repository root."
    )
endif()
include("${_warren_local_paths}")

foreach(_required_path IN ITEMS
    WARREN_HOST_NINJA
    WARREN_HOST_PYTHON
    WARREN_MFORMAT
    WARREN_MCOPY
    WARREN_QEMU_AARCH64
    WARREN_AARCH64_UEFI_CODE
    WARREN_AARCH64_UEFI_VARS
)
    if(NOT EXISTS "${${_required_path}}")
        message(FATAL_ERROR
            "${_required_path} does not resolve to an existing path. "
            "Rerun ./tools/bootstrap.sh --install."
        )
    endif()
endforeach()

if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "System images require CMAKE_BUILD_TYPE Debug or Release")
endif()

set(_warren_product_root "${CMAKE_CURRENT_BINARY_DIR}/products")
set(_warren_host_build "${_warren_product_root}/host")
set(_warren_burrow_build "${_warren_product_root}/burrow")
set(_warren_burrow_fail_build "${_warren_product_root}/burrow-fail")
set(_warren_burrow_panic_build "${_warren_product_root}/burrow-panic")
set(_warren_burrow_emergency_fault_build
    "${_warren_product_root}/burrow-emergency-fault")
set(_warren_uefi_build "${_warren_product_root}/uefi")
set(_warren_uefi_first_entry_fault_build "${_warren_product_root}/uefi-first-entry-fault")
set(_warren_uefi_console_fault_build "${_warren_product_root}/uefi-console-fault")
set(_warren_uefi_post_exit_fault_build "${_warren_product_root}/uefi-post-exit-fault")
set(_warren_artifact_directory "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(_warren_burrow_image "${_warren_burrow_build}/artifacts/burrow-runtime.elf")
set(_warren_burrow_symbols "${_warren_burrow_build}/artifacts/burrow.elf")
set(_warren_burrow_map "${_warren_burrow_build}/artifacts/burrow.map")
set(_warren_burrow_fail_image "${_warren_burrow_fail_build}/artifacts/burrow-runtime.elf")
set(_warren_burrow_panic_image "${_warren_burrow_panic_build}/artifacts/burrow-runtime.elf")
set(_warren_burrow_emergency_fault_image
    "${_warren_burrow_emergency_fault_build}/artifacts/burrow-runtime.elf")
set(_warren_bootloader "${_warren_uefi_build}/artifacts/BOOTAA64.EFI")
set(_warren_first_entry_fault_bootloader
    "${_warren_uefi_first_entry_fault_build}/artifacts/BOOTAA64.EFI")
set(_warren_console_fault_bootloader
    "${_warren_uefi_console_fault_build}/artifacts/BOOTAA64.EFI")
set(_warren_post_exit_fault_bootloader
    "${_warren_uefi_post_exit_fault_build}/artifacts/BOOTAA64.EFI")
set(_warren_loader_test "${_warren_host_build}/WarrenBurrowLoaderTests")
set(_warren_esp "${_warren_artifact_directory}/warren-system-esp.img")
set(_warren_fail_esp "${_warren_artifact_directory}/warren-system-fail-esp.img")
set(_warren_panic_esp "${_warren_artifact_directory}/warren-system-panic-esp.img")
set(_warren_emergency_fault_esp
    "${_warren_artifact_directory}/warren-system-emergency-fault-esp.img")
set(_warren_first_entry_fault_esp
    "${_warren_artifact_directory}/warren-system-first-entry-fault-esp.img")
set(_warren_console_fault_esp
    "${_warren_artifact_directory}/warren-system-console-fault-esp.img")
set(_warren_post_exit_fault_esp
    "${_warren_artifact_directory}/warren-system-post-exit-fault-esp.img")

add_custom_target(WarrenSystemBurrow
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_burrow_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Warren.cmake
        -DWARREN_BUILD_ENVIRONMENT=burrow
        -DWARREN_ENABLE_QEMU_TEST_RESULT=ON
        -DWARREN_QEMU_TEST_RESULT_MODE=pass
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_burrow_build}"
        --target BurrowImage
    BYPRODUCTS
        "${_warren_burrow_image}"
        "${_warren_burrow_symbols}"
        "${_warren_burrow_map}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemBurrowFail
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_burrow_fail_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Warren.cmake
        -DWARREN_BUILD_ENVIRONMENT=burrow
        -DWARREN_ENABLE_QEMU_TEST_RESULT=ON
        -DWARREN_QEMU_TEST_RESULT_MODE=fail
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_burrow_fail_build}"
        --target BurrowImage
    BYPRODUCTS "${_warren_burrow_fail_image}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemBurrowPanic
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_burrow_panic_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Warren.cmake
        -DWARREN_BUILD_ENVIRONMENT=burrow
        -DWARREN_ENABLE_QEMU_TEST_RESULT=ON
        -DWARREN_QEMU_TEST_RESULT_MODE=panic
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_burrow_panic_build}"
        --target BurrowImage
    BYPRODUCTS "${_warren_burrow_panic_image}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemBurrowEmergencyFault
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_burrow_emergency_fault_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Warren.cmake
        -DWARREN_BUILD_ENVIRONMENT=burrow
        -DWARREN_ENABLE_QEMU_TEST_RESULT=ON
        -DWARREN_QEMU_TEST_RESULT_MODE=pass
        -DWARREN_AARCH64_ENTRY_FAULT=emergency
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_burrow_emergency_fault_build}"
        --target BurrowImage
    BYPRODUCTS "${_warren_burrow_emergency_fault_image}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemHostLoader
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_host_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DWARREN_BUILD_ENVIRONMENT=host
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_host_build}"
        --target WarrenBurrowLoaderTests
    BYPRODUCTS "${_warren_loader_test}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemUefi
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_uefi_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Uefi.cmake
        -DWARREN_BUILD_ENVIRONMENT=uefi
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_uefi_build}"
        --target WarrenUefiBootloader
    BYPRODUCTS "${_warren_bootloader}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemUefiFirstEntryFault
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_uefi_first_entry_fault_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Uefi.cmake
        -DWARREN_BUILD_ENVIRONMENT=uefi
        -DWARREN_QEMU_HANDOFF_FAULT=boot-magic
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_uefi_first_entry_fault_build}"
        --target WarrenUefiBootloader
    BYPRODUCTS "${_warren_first_entry_fault_bootloader}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemUefiConsoleFault
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_uefi_console_fault_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Uefi.cmake
        -DWARREN_BUILD_ENVIRONMENT=uefi
        -DWARREN_QEMU_HANDOFF_FAULT=console-record
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_uefi_console_fault_build}"
        --target WarrenUefiBootloader
    BYPRODUCTS "${_warren_console_fault_bootloader}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(WarrenSystemUefiPostExitFault
    COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}"
        -B "${_warren_uefi_post_exit_fault_build}"
        -G Ninja
        -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Uefi.cmake
        -DWARREN_BUILD_ENVIRONMENT=uefi
        -DWARREN_QEMU_HANDOFF_FAULT=post-exit
    COMMAND "${CMAKE_COMMAND}"
        --build "${_warren_uefi_post_exit_fault_build}"
        --target WarrenUefiBootloader
    BYPRODUCTS "${_warren_post_exit_fault_bootloader}"
    USES_TERMINAL
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_bootloader}"
        --burrow "${_warren_burrow_image}"
        --output "${_warren_esp}"
    DEPENDS
        WarrenSystemBurrow
        WarrenSystemUefi
        "${_warren_burrow_image}"
        "${_warren_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_fail_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_bootloader}"
        --burrow "${_warren_burrow_fail_image}"
        --output "${_warren_fail_esp}"
    DEPENDS
        WarrenSystemBurrowFail
        WarrenSystemUefi
        "${_warren_burrow_fail_image}"
        "${_warren_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_panic_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_bootloader}"
        --burrow "${_warren_burrow_panic_image}"
        --output "${_warren_panic_esp}"
    DEPENDS
        WarrenSystemBurrowPanic
        WarrenSystemUefi
        "${_warren_burrow_panic_image}"
        "${_warren_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_emergency_fault_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_bootloader}"
        --burrow "${_warren_burrow_emergency_fault_image}"
        --output "${_warren_emergency_fault_esp}"
    DEPENDS
        WarrenSystemBurrowEmergencyFault
        WarrenSystemUefi
        "${_warren_burrow_emergency_fault_image}"
        "${_warren_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_first_entry_fault_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_first_entry_fault_bootloader}"
        --burrow "${_warren_burrow_image}"
        --output "${_warren_first_entry_fault_esp}"
    DEPENDS
        WarrenSystemBurrow
        WarrenSystemUefiFirstEntryFault
        "${_warren_burrow_image}"
        "${_warren_first_entry_fault_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_post_exit_fault_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_post_exit_fault_bootloader}"
        --burrow "${_warren_burrow_image}"
        --output "${_warren_post_exit_fault_esp}"
    DEPENDS
        WarrenSystemBurrow
        WarrenSystemUefiPostExitFault
        "${_warren_burrow_image}"
        "${_warren_post_exit_fault_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_command(
    OUTPUT "${_warren_console_fault_esp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_console_fault_bootloader}"
        --burrow "${_warren_burrow_image}"
        --output "${_warren_console_fault_esp}"
    DEPENDS
        WarrenSystemBurrow
        WarrenSystemUefiConsoleFault
        "${_warren_burrow_image}"
        "${_warren_console_fault_bootloader}"
        tools/build_esp.py
    VERBATIM
)

add_custom_target(WarrenSystemImage ALL DEPENDS
    "${_warren_esp}"
    "${_warren_fail_esp}"
    "${_warren_panic_esp}"
    "${_warren_emergency_fault_esp}"
    "${_warren_first_entry_fault_esp}"
    "${_warren_console_fault_esp}"
    "${_warren_post_exit_fault_esp}"
    WarrenSystemHostLoader
)

add_test(
    NAME WarrenSystemBurrowProductionLoader
    COMMAND "${_warren_loader_test}" "${_warren_burrow_image}"
)

add_test(
    NAME WarrenSystemEspContents
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_esp_contents.py"
        --mcopy "${WARREN_MCOPY}"
        --esp "${_warren_esp}"
        --bootloader "${_warren_bootloader}"
        --burrow "${_warren_burrow_image}"
)

add_test(
    NAME WarrenSystemEspReproducibility
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_esp_reproducibility.py"
        --python "${WARREN_HOST_PYTHON}"
        --builder "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
        --mformat "${WARREN_MFORMAT}"
        --mcopy "${WARREN_MCOPY}"
        --bootloader "${_warren_bootloader}"
        --burrow "${_warren_burrow_image}"
)
set_tests_properties(WarrenSystemEspReproducibility PROPERTIES TIMEOUT 20)

add_test(
    NAME WarrenSystemBurrowFirstEntryEl1
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_esp}"
        --initial-el el1
        --expected-test burrow-first-entry
)
set_tests_properties(WarrenSystemBurrowFirstEntryEl1 PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemBurrowFirstEntryEl2
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_esp}"
        --initial-el el2
        --expected-test burrow-first-entry
)
set_tests_properties(WarrenSystemBurrowFirstEntryEl2 PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemEmergencyVectorEl1
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_emergency_fault_esp}"
        --initial-el el1
        --expected-test burrow-first-entry
        --expected-result panic
        --expected-code 4
        --require-output "BURROW_EXCEPTION:stage=1:vector=4:el=1:esr=0x00000000F2000777:"
)
set_tests_properties(WarrenSystemEmergencyVectorEl1 PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemEmergencyVectorEl2
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_emergency_fault_esp}"
        --initial-el el2
        --expected-test burrow-first-entry
        --expected-result panic
        --expected-code 4
        --require-output "BURROW_EXCEPTION:stage=1:vector=4:el=2:esr=0x00000000F2000777:"
)
set_tests_properties(WarrenSystemEmergencyVectorEl2 PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemQemuResultFailure
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_fail_esp}"
        --initial-el el1
        --expected-test burrow-first-entry
        --expected-result fail
)
set_tests_properties(WarrenSystemQemuResultFailure PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemQemuResultPanic
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_panic_esp}"
        --initial-el el1
        --expected-test burrow-first-entry
        --expected-result panic
)
set_tests_properties(WarrenSystemQemuResultPanic PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemBurrowRejectsInvalidHeader
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_first_entry_fault_esp}"
        --initial-el el1
        --required-el-evidence loader
        --expected-test burrow-first-entry
        --expected-result fail
        --expected-code 68
        --require-output "WARREN_POST_EXIT:ExitBootServices:EL1"
        --forbid-output "BURROW_FIRST_ENTRY:"
)
set_tests_properties(WarrenSystemBurrowRejectsInvalidHeader PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemBurrowRejectsInvalidConsole
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_console_fault_esp}"
        --initial-el el1
        --required-el-evidence loader
        --expected-test burrow-first-entry
        --expected-result fail
        --expected-code 72
        --require-output "WARREN_POST_EXIT:ExitBootServices:EL1"
        --forbid-output "BURROW_FIRST_ENTRY:"
)
set_tests_properties(WarrenSystemBurrowRejectsInvalidConsole PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemPostExitFailureContainment
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_post_exit_fault_esp}"
        --initial-el el1
        --required-el-evidence none
        --expected-test burrow-first-entry
        --expected-result fail
        --expected-code 73
        --require-output "WARREN_POST_EXIT:FAIL"
        --forbid-output "WARREN_POST_EXIT:ExitBootServices:"
        --forbid-output "BURROW_FIRST_ENTRY:"
)
set_tests_properties(WarrenSystemPostExitFailureContainment PROPERTIES TIMEOUT 40)

unset(_required_path)
unset(_warren_artifact_directory)
unset(_warren_bootloader)
unset(_warren_first_entry_fault_bootloader)
unset(_warren_console_fault_bootloader)
unset(_warren_post_exit_fault_bootloader)
unset(_warren_burrow_build)
unset(_warren_burrow_fail_build)
unset(_warren_burrow_fail_image)
unset(_warren_burrow_image)
unset(_warren_burrow_map)
unset(_warren_burrow_panic_build)
unset(_warren_burrow_panic_image)
unset(_warren_burrow_emergency_fault_build)
unset(_warren_burrow_emergency_fault_image)
unset(_warren_burrow_symbols)
unset(_warren_esp)
unset(_warren_fail_esp)
unset(_warren_local_paths)
unset(_warren_host_build)
unset(_warren_loader_test)
unset(_warren_product_root)
unset(_warren_panic_esp)
unset(_warren_emergency_fault_esp)
unset(_warren_first_entry_fault_esp)
unset(_warren_console_fault_esp)
unset(_warren_post_exit_fault_esp)
unset(_warren_uefi_build)
unset(_warren_uefi_first_entry_fault_build)
unset(_warren_uefi_console_fault_build)
unset(_warren_uefi_post_exit_fault_build)
