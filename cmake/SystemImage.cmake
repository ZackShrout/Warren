# Warren combined AArch64 system-image orchestration.

set(_warren_local_paths "${CMAKE_CURRENT_SOURCE_DIR}/.warren/ToolchainPaths.cmake")
if(NOT EXISTS "${_warren_local_paths}")
    message(FATAL_ERROR
        "Warren's local toolchain configuration is missing. "
        "Run ./tools/bootstrap.sh --install from the repository root."
    )
endif()
include("${_warren_local_paths}")

set(WARREN_LLVM_SYMBOLIZER "${WARREN_LLVM_ROOT}/bin/llvm-symbolizer")
set(WARREN_LLDB "${WARREN_LLVM_ROOT}/bin/lldb")

set(_warren_required_paths
    WARREN_HOST_NINJA
    WARREN_HOST_PYTHON
    WARREN_MFORMAT
    WARREN_MCOPY
    WARREN_QEMU_AARCH64
    WARREN_AARCH64_UEFI_CODE
    WARREN_AARCH64_UEFI_VARS
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND _warren_required_paths WARREN_LLVM_SYMBOLIZER WARREN_LLDB)
endif()
foreach(_required_path IN LISTS _warren_required_paths)
    if(NOT EXISTS "${${_required_path}}")
        message(FATAL_ERROR
            "${_required_path} does not resolve to an existing path. "
            "Rerun ./tools/bootstrap.sh --install."
        )
    endif()
endforeach()
unset(_warren_required_paths)

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
set(_warren_aarch64_faults
    common-el1-vector
    unsupported-feature
    common-el1
    planning
    tables
    activation
    lower-guard
    upper-guard
    text-write
    data-execute
    stale-identity
    identity-failure
    kernel-entry
    physical-memory
    reported-breakpoint
    timer-initialization
    monitor
    assertion
    panic
)
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
set(_warren_aarch64_fault_esps)
foreach(_warren_fault IN LISTS _warren_aarch64_faults)
    string(REPLACE "-" "_" _warren_fault_key "${_warren_fault}")
    set(_warren_fault_build
        "${_warren_product_root}/burrow-${_warren_fault}-fault")
    set(_warren_fault_image
        "${_warren_fault_build}/artifacts/burrow-runtime.elf")
    set(_warren_fault_esp
        "${_warren_artifact_directory}/warren-system-${_warren_fault}-fault-esp.img")
    set("_warren_fault_${_warren_fault_key}_build" "${_warren_fault_build}")
    set("_warren_fault_${_warren_fault_key}_image" "${_warren_fault_image}")
    set("_warren_fault_${_warren_fault_key}_esp" "${_warren_fault_esp}")
    list(APPEND _warren_aarch64_fault_esps "${_warren_fault_esp}")
endforeach()
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

foreach(_warren_fault IN LISTS _warren_aarch64_faults)
    string(REPLACE "-" "_" _warren_fault_key "${_warren_fault}")
    set(_warren_fault_build "${_warren_fault_${_warren_fault_key}_build}")
    set(_warren_fault_image "${_warren_fault_${_warren_fault_key}_image}")
    add_custom_target("WarrenSystemBurrowFault-${_warren_fault}"
        COMMAND "${CMAKE_COMMAND}"
            -S "${CMAKE_CURRENT_SOURCE_DIR}"
            -B "${_warren_fault_build}"
            -G Ninja
            -DCMAKE_MAKE_PROGRAM=${WARREN_HOST_NINJA}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/AArch64Warren.cmake
            -DWARREN_BUILD_ENVIRONMENT=burrow
            -DWARREN_ENABLE_QEMU_TEST_RESULT=ON
            -DWARREN_QEMU_TEST_RESULT_MODE=pass
            -DWARREN_AARCH64_ENTRY_FAULT=${_warren_fault}
        COMMAND "${CMAKE_COMMAND}"
            --build "${_warren_fault_build}"
            --target BurrowImage
        BYPRODUCTS "${_warren_fault_image}"
        USES_TERMINAL
        VERBATIM
    )
endforeach()

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

foreach(_warren_fault IN LISTS _warren_aarch64_faults)
    string(REPLACE "-" "_" _warren_fault_key "${_warren_fault}")
    set(_warren_fault_image "${_warren_fault_${_warren_fault_key}_image}")
    set(_warren_fault_esp "${_warren_fault_${_warren_fault_key}_esp}")
    add_custom_command(
        OUTPUT "${_warren_fault_esp}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_warren_artifact_directory}"
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_esp.py"
            --mformat "${WARREN_MFORMAT}"
            --mcopy "${WARREN_MCOPY}"
            --bootloader "${_warren_bootloader}"
            --burrow "${_warren_fault_image}"
            --output "${_warren_fault_esp}"
        DEPENDS
            "WarrenSystemBurrowFault-${_warren_fault}"
            WarrenSystemUefi
            "${_warren_fault_image}"
            "${_warren_bootloader}"
            tools/build_esp.py
        VERBATIM
    )
endforeach()

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
    ${_warren_aarch64_fault_esps}
    "${_warren_first_entry_fault_esp}"
    "${_warren_console_fault_esp}"
    "${_warren_post_exit_fault_esp}"
    WarrenSystemHostLoader
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_warren_debug_lldb
        "${_warren_artifact_directory}/burrow-stable.lldb")
    add_custom_command(
        OUTPUT "${_warren_debug_lldb}"
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/burrow_debug.py"
            check
            --image "${_warren_burrow_symbols}"
            --map "${_warren_burrow_map}"
            --symbolizer "${WARREN_LLVM_SYMBOLIZER}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/burrow_debug.py"
            lldb
            --image "${_warren_burrow_symbols}"
            --map "${_warren_burrow_map}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --address-space stable
            --output "${_warren_debug_lldb}"
        DEPENDS
            WarrenSystemBurrow
            "${_warren_burrow_symbols}"
            "${_warren_burrow_map}"
            tools/burrow_debug.py
        VERBATIM
    )
    add_custom_target(WarrenSystemDebugArtifacts
        DEPENDS "${_warren_debug_lldb}"
    )
    add_dependencies(WarrenSystemImage WarrenSystemDebugArtifacts)

    foreach(_warren_debug_el IN ITEMS el1 el2)
        add_custom_target("WarrenSystemDebug-${_warren_debug_el}"
            COMMAND "${WARREN_HOST_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_aarch64_debug.py"
                --qemu "${WARREN_QEMU_AARCH64}"
                --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
                --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
                --esp "${_warren_esp}"
                --initial-el "${_warren_debug_el}"
            DEPENDS WarrenSystemImage WarrenSystemDebugArtifacts
            USES_TERMINAL
            VERBATIM
        )
    endforeach()

    add_custom_target(WarrenSystemLldb
        COMMAND "${WARREN_LLDB}" --source "${_warren_debug_lldb}"
        DEPENDS WarrenSystemDebugArtifacts
        USES_TERMINAL
        VERBATIM
    )
endif()

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
    NAME WarrenSystemAArch64NormalizedEntryEl1
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_esp}"
        --initial-el el1
        --expected-test aarch64-normalized-entry
        --require-output "BURROW_CONSOLE:driver=pl011:mode=polling:output=ready"
        --require-output "BURROW_MEMORY_V1:extents="
        --require-output ":arena_pages=128:"
        --require-output ":boot_pages=4:"
        --require-output "BURROW_TIMER:source=cntp:interrupt=30:ticks=1:frequency="
        --serial-input-after "BURROW_MONITOR:ready:"
        --serial-input-line status
        --serial-input-line exit
        --require-output-in-order "BURROW_MEMORY_V1:extents="
        --require-output-in-order "BURROW_MONITOR:ready:commands=help,status,exit"
        --require-output-in-order "BURROW_MONITOR:status=ok:timer_ticks=1"
        --require-output-in-order "BURROW_MONITOR:exit=accepted"
        --require-output-in-order "BURROW_NORMALIZED_ENTRY:initial=EL1:normalized=EL1:tables=owned:identity=removed:cpp=arrived"
)
set_tests_properties(WarrenSystemAArch64NormalizedEntryEl1 PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemAArch64NormalizedEntryEl2
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_esp}"
        --initial-el el2
        --expected-test aarch64-normalized-entry
        --require-output "BURROW_CONSOLE:driver=pl011:mode=polling:output=ready"
        --require-output "BURROW_MEMORY_V1:extents="
        --require-output ":arena_pages=128:"
        --require-output ":boot_pages=4:"
        --require-output "BURROW_TIMER:source=cntp:interrupt=30:ticks=1:frequency="
        --serial-input-after "BURROW_MONITOR:ready:"
        --serial-input-line status
        --serial-input-line exit
        --require-output-in-order "BURROW_MEMORY_V1:extents="
        --require-output-in-order "BURROW_MONITOR:ready:commands=help,status,exit"
        --require-output-in-order "BURROW_MONITOR:status=ok:timer_ticks=1"
        --require-output-in-order "BURROW_MONITOR:exit=accepted"
        --require-output-in-order "BURROW_NORMALIZED_ENTRY:initial=EL2:normalized=EL1:tables=owned:identity=removed:cpp=arrived"
)
set_tests_properties(WarrenSystemAArch64NormalizedEntryEl2 PROPERTIES TIMEOUT 40)

add_test(
    NAME WarrenSystemEmergencyVectorEl1
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_emergency_fault_esp}"
        --initial-el el1
        --expected-test aarch64-normalized-entry
        --expected-result panic
        --expected-code 4
        --require-output "BURROW_EXCEPTION:stage=2:vector=4:el=1:esr=0x00000000F2000777:"
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
        --expected-test aarch64-normalized-entry
        --expected-result panic
        --expected-code 4
        --require-output "BURROW_EXCEPTION:stage=2:vector=4:el=2:esr=0x00000000F2000777:"
)
set_tests_properties(WarrenSystemEmergencyVectorEl2 PROPERTIES TIMEOUT 40)

function(warren_add_aarch64_fault_test
    test_name
    fault
    expected_result
    expected_code)
    string(REPLACE "-" "_" _fault_key "${fault}")
    set(_fault_esp_variable "_warren_fault_${_fault_key}_esp")
    add_test(
        NAME "${test_name}"
        COMMAND "${WARREN_HOST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
            --qemu "${WARREN_QEMU_AARCH64}"
            --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
            --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
            --esp "${${_fault_esp_variable}}"
            --initial-el el1
            --expected-test aarch64-normalized-entry
            --expected-result "${expected_result}"
            --expected-code "${expected_code}"
            ${ARGN}
    )
    set_tests_properties("${test_name}" PROPERTIES TIMEOUT 40)
endfunction()

warren_add_aarch64_fault_test(
    WarrenSystemCommonEl1Vector
    common-el1-vector panic 4
    --require-output "BURROW_EXCEPTION:stage=3:vector=4:el=1:esr=0x00000000F2000779:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsUnsupportedFeature
    unsupported-feature fail 75
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsCommonEl1State
    common-el1 fail 76
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsTransitionPlanning
    planning fail 77
    --required-el-evidence loader
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsOwnedTables
    tables fail 78
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsActivationPreflight
    activation fail 79
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemLowerStackGuard
    lower-guard panic 4
    --require-output "BURROW_EXCEPTION_V1:stage=8:vector=4:el=1:esr=0x0000000096000007:"
    --require-output ":far=0xFFFFD00000000000:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemUpperStackGuard
    upper-guard panic 4
    --require-output "BURROW_EXCEPTION_V1:stage=8:vector=4:el=1:esr=0x0000000096000007:"
    --require-output ":far=0xFFFFD00000011000:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemTextWriteProtection
    text-write panic 4
    --require-output "BURROW_EXCEPTION_V1:stage=8:vector=4:el=1:esr=0x000000009600004F:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemDataExecuteProtection
    data-execute panic 4
    --require-output "BURROW_EXCEPTION_V1:stage=8:vector=4:el=1:esr=0x000000008600000F:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemStaleIdentityAlias
    stale-identity panic 4
    --require-output "BURROW_EXCEPTION_V1:stage=8:vector=4:el=1:esr=0x0000000096000004:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsIdentityRemovalProof
    identity-failure fail 80
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsKernelEntryWitness
    kernel-entry fail 81
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsPhysicalMemory
    physical-memory fail 81
    --forbid-output "BURROW_MEMORY_V1:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemReportsCompleteExceptionFrame
    reported-breakpoint panic 4
    --require-output "BURROW_EXCEPTION_V1:stage=9:vector=4:el=1:esr=0x00000000F200077A:"
    --require-output ":x15=0x000000000000F116:"
    --require-output ":x30=0x"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsTimerInitialization
    timer-initialization fail 82
    --forbid-output "BURROW_TIMER:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemRejectsMonitor
    monitor fail 83
    --forbid-output "BURROW_MONITOR:"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemAssertionPath
    assertion panic 2
    --serial-input-after "BURROW_MONITOR:ready:"
    --serial-input-line exit
    --require-output-in-order "BURROW_MONITOR:ready:commands=help,status,exit"
    --require-output-in-order "BURROW_MONITOR:exit=accepted"
    --require-output-in-order "BURROW_PANIC_V1:kind=assertion:id=phase1.assertion:file=kernel/src/Platform/QemuVirt/Panic.cpp:line="
    --require-output ":message=fixture assertion"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)
warren_add_aarch64_fault_test(
    WarrenSystemPanicPath
    panic panic 3
    --serial-input-after "BURROW_MONITOR:ready:"
    --serial-input-line exit
    --require-output-in-order "BURROW_MONITOR:ready:commands=help,status,exit"
    --require-output-in-order "BURROW_MONITOR:exit=accepted"
    --require-output-in-order "BURROW_PANIC_V1:kind=panic:id=phase1.panic:file=kernel/src/Platform/QemuVirt/Panic.cpp:line="
    --require-output ":message=fixture panic"
    --forbid-output "BURROW_NORMALIZED_ENTRY:"
)

add_test(
    NAME WarrenSystemQemuResultFailure
    COMMAND "${WARREN_HOST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_uefi_smoke.py"
        --qemu "${WARREN_QEMU_AARCH64}"
        --firmware-code "${WARREN_AARCH64_UEFI_CODE}"
        --firmware-vars "${WARREN_AARCH64_UEFI_VARS}"
        --esp "${_warren_fail_esp}"
        --initial-el el1
        --expected-test aarch64-normalized-entry
        --expected-result fail
        --serial-input-after "BURROW_MONITOR:ready:"
        --serial-input-line status
        --serial-input-line exit
        --require-output-in-order "BURROW_MONITOR:ready:commands=help,status,exit"
        --require-output-in-order "BURROW_MONITOR:status=ok:timer_ticks=1"
        --require-output-in-order "BURROW_MONITOR:exit=accepted"
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
        --expected-test aarch64-normalized-entry
        --expected-result panic
        --serial-input-after "BURROW_MONITOR:ready:"
        --serial-input-line status
        --serial-input-line exit
        --require-output-in-order "BURROW_MONITOR:ready:commands=help,status,exit"
        --require-output-in-order "BURROW_MONITOR:status=ok:timer_ticks=1"
        --require-output-in-order "BURROW_MONITOR:exit=accepted"
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
        --expected-test aarch64-normalized-entry
        --expected-result fail
        --expected-code 74
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
unset(_warren_debug_lldb)
unset(_warren_uefi_console_fault_build)
unset(_warren_uefi_post_exit_fault_build)
