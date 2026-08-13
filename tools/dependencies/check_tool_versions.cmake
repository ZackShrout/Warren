cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED WARREN_BASELINE_FILE OR NOT EXISTS "${WARREN_BASELINE_FILE}")
    message(FATAL_ERROR "Warren toolchain baseline is missing: ${WARREN_BASELINE_FILE}")
endif()

include("${WARREN_BASELINE_FILE}")

function(warren_check_version tool_name actual minimum maximum known_good)
    if("${actual}" STREQUAL "")
        message(FATAL_ERROR "${tool_name} did not report a parseable version")
    endif()

    if("${actual}" VERSION_LESS "${minimum}" OR
       NOT "${actual}" VERSION_LESS "${maximum}")
        message(FATAL_ERROR
            "${tool_name} ${actual} is outside Warren's supported range "
            "[${minimum}, ${maximum})"
        )
    endif()

    if("${actual}" VERSION_EQUAL "${known_good}")
        message(STATUS "${tool_name} ${actual}: known-good")
    else()
        message(STATUS
            "${tool_name} ${actual}: compatible and requires functional probes "
            "(last known-good ${known_good})"
        )
    endif()
endfunction()

warren_check_version(
    "CMake"
    "${WARREN_ACTUAL_CMAKE}"
    "${WARREN_BASELINE_CMAKE_MINIMUM}"
    "${WARREN_BASELINE_CMAKE_MAXIMUM}"
    "${WARREN_BASELINE_CMAKE_KNOWN_GOOD}"
)
warren_check_version(
    "Ninja"
    "${WARREN_ACTUAL_NINJA}"
    "${WARREN_BASELINE_NINJA_MINIMUM}"
    "${WARREN_BASELINE_NINJA_MAXIMUM}"
    "${WARREN_BASELINE_NINJA_KNOWN_GOOD}"
)
warren_check_version(
    "LLVM/Clang"
    "${WARREN_ACTUAL_LLVM}"
    "${WARREN_BASELINE_LLVM_MINIMUM}"
    "${WARREN_BASELINE_LLVM_MAXIMUM}"
    "${WARREN_BASELINE_LLVM_KNOWN_GOOD}"
)
warren_check_version(
    "LLD"
    "${WARREN_ACTUAL_LLD}"
    "${WARREN_BASELINE_LLD_MINIMUM}"
    "${WARREN_BASELINE_LLD_MAXIMUM}"
    "${WARREN_BASELINE_LLD_KNOWN_GOOD}"
)
warren_check_version(
    "QEMU"
    "${WARREN_ACTUAL_QEMU}"
    "${WARREN_BASELINE_QEMU_MINIMUM}"
    "${WARREN_BASELINE_QEMU_MAXIMUM}"
    "${WARREN_BASELINE_QEMU_KNOWN_GOOD}"
)
warren_check_version(
    "mtools"
    "${WARREN_ACTUAL_MTOOLS}"
    "${WARREN_BASELINE_MTOOLS_MINIMUM}"
    "${WARREN_BASELINE_MTOOLS_MAXIMUM}"
    "${WARREN_BASELINE_MTOOLS_KNOWN_GOOD}"
)
warren_check_version(
    "Python"
    "${WARREN_ACTUAL_PYTHON}"
    "${WARREN_BASELINE_PYTHON_MINIMUM}"
    "${WARREN_BASELINE_PYTHON_MAXIMUM}"
    "${WARREN_BASELINE_PYTHON_KNOWN_GOOD}"
)

if(NOT "${WARREN_ACTUAL_LLVM}" VERSION_EQUAL "${WARREN_ACTUAL_LLD}")
    message(FATAL_ERROR
        "LLVM/Clang ${WARREN_ACTUAL_LLVM} and LLD ${WARREN_ACTUAL_LLD} must match exactly"
    )
endif()
