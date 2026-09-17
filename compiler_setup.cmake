# have COMPILE_PREX
if (NOT DEFINED CONFIG_COMPILE_PREX)
    set(CONFIG_COMPILE_PREX "aarch64-none-linux-gnu-")
endif()

# Is cross compile?
if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
    if (EXISTS "/sys/firmware/devicetree/base/model")
        file(READ "/sys/firmware/devicetree/base/model" DEVICE_MODEL)
        string(TOLOWER "${DEVICE_MODEL}" DEVICE_MODEL_LOWER)
        # DshanPi_A1
        if (DEVICE_MODEL_LOWER MATCHES "dshanpi a1")
            set(IS_CROSS_COMPILE FALSE)
        else()
            set(IS_CROSS_COMPILE TRUE)
        endif()
        # Raspberry_Pi
        if (DEVICE_MODEL_LOWER MATCHES "raspberry pi")
            set(IS_CROSS_COMPILE FALSE)
        else()
            set(IS_CROSS_COMPILE TRUE)
        endif()
        # DP-X5 / RDK X5 (Dipa) - native build on device
        if (DEVICE_MODEL_LOWER MATCHES "d-robotics x5")
            set(IS_CROSS_COMPILE FALSE)
        endif()
    else()
        if (PLATFORM_BOARD STREQUAL "Raspberry_Pi")
            set(IS_CROSS_COMPILE TRUE)
        elseif (PLATFORM_BOARD STREQUAL "DshanPi_A1")
            set(IS_CROSS_COMPILE TRUE)
        elseif (PLATFORM_BOARD STREQUAL "TaishanPi_3")
            set(IS_CROSS_COMPILE TRUE)
        else()
            set(IS_CROSS_COMPILE FALSE)
        endif()
    endif()
else()
    set(IS_CROSS_COMPILE TRUE)
endif()

# Toolchain path
get_filename_component(TOOLCHAIN_PATH "${CMAKE_CURRENT_LIST_DIR}/../tools/" ABSOLUTE)

if (IS_CROSS_COMPILE)
    set(TOOLCHAIN_PREFIX "")
    if (PLATFORM_BOARD STREQUAL "Raspberry_Pi")
        get_filename_component(TOOLCHAIN_PREFIX "${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-14.2-2024.10/bin" ABSOLUTE)
        # TOOLCHAIN_PREFIX Check
        if(NOT EXISTS "${TOOLCHAIN_PREFIX}")
            message(FATAL_ERROR "Toolchain directory does not exist: ${TOOLCHAIN_PREFIX}")
        endif()

        get_filename_component(TOOLCHAIN_PREFIX "${TOOLCHAIN_PREFIX}/${CONFIG_COMPILE_PREX}" ABSOLUTE)
    elseif (PLATFORM_BOARD STREQUAL "DshanPi_A1")
        get_filename_component(TOOLCHAIN_PREFIX "${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-13.3-2024.04/bin" ABSOLUTE)
        # TOOLCHAIN_PREFIX Check
        if(NOT EXISTS "${TOOLCHAIN_PREFIX}")
            message(FATAL_ERROR "Toolchain directory does not exist: ${TOOLCHAIN_PREFIX}")
        endif()

        get_filename_component(TOOLCHAIN_PREFIX "${TOOLCHAIN_PREFIX}/${CONFIG_COMPILE_PREX}" ABSOLUTE)
    elseif (PLATFORM_BOARD STREQUAL "TaishanPi_3")
        get_filename_component(TOOLCHAIN_PREFIX "${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-14.2-2024.10/bin" ABSOLUTE)
        # TOOLCHAIN_PREFIX Check
        if(NOT EXISTS "${TOOLCHAIN_PREFIX}")
            message(FATAL_ERROR "Toolchain directory does not exist: ${TOOLCHAIN_PREFIX}")
        endif()

        get_filename_component(TOOLCHAIN_PREFIX "${TOOLCHAIN_PREFIX}/${CONFIG_COMPILE_PREX}" ABSOLUTE)
    else()
        message(FATAL_ERROR "Unsupported PLATFORM_BOARD for cross compilation: ${PLATFORM_BOARD}")
    endif()

    # On Windows, add .exe suffix to executables
    if(WIN32)
        set(EXE_SUFFIX ".exe")
    else()
        set(EXE_SUFFIX "")
    endif()

    set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc${EXE_SUFFIX}")
    set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++${EXE_SUFFIX}")
    set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc${EXE_SUFFIX}")
    set(CMAKE_AR "${TOOLCHAIN_PREFIX}ar${EXE_SUFFIX}")
    set(CMAKE_RANLIB "${TOOLCHAIN_PREFIX}ranlib${EXE_SUFFIX}")
    set(CMAKE_STRIP "${TOOLCHAIN_PREFIX}strip${EXE_SUFFIX}")
else()
    # Native compile settings
    set(CMAKE_C_COMPILER "gcc")
    set(CMAKE_CXX_COMPILER "g++")
    set(CMAKE_ASM_COMPILER "gcc")
    set(CMAKE_AR "ar")
    set(CMAKE_RANLIB "ranlib")
    set(CMAKE_STRIP "strip")
endif()
