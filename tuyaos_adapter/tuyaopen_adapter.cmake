if(CONFIG_ENABLE_WIFI STREQUAL "y")
    set(CONFIG_ENABLE_WIFI ON)
else()
    set(CONFIG_ENABLE_WIFI OFF)
endif()

if(CONFIG_ENABLE_BLUETOOTH STREQUAL "y")
    set(CONFIG_ENABLE_BLUETOOTH ON)
else()
    set(CONFIG_ENABLE_BLUETOOTH OFF)
endif()

if(CONFIG_ENABLE_WIRED STREQUAL "y")
    set(CONFIG_ENABLE_WIRED ON)
else()
    set(CONFIG_ENABLE_WIRED OFF)
endif()

set(SOURCES "")

# Initialize global variable to collect found libraries
set(TUYAOPEN_FOUND_LIBRARIES "")

# Function to find and collect library
# Argument 1: library_path - Path to the library directory
# Argument 2: library_name - Name of the library
function(collect_library library_path library_name)
    set(LIB_EXTENSIONS .so .a .dylib .lib)
    
    # Search for library with different prefixes and extensions
    foreach(prefix "" "lib")
        foreach(ext ${LIB_EXTENSIONS})
            set(full_path "${library_path}/${prefix}${library_name}${ext}")
            if(EXISTS ${full_path})
                message(STATUS "Found library: ${full_path}")
                link_directories(${library_path})
                list(APPEND TUYAOPEN_FOUND_LIBRARIES ${library_name})
                set(TUYAOPEN_FOUND_LIBRARIES ${TUYAOPEN_FOUND_LIBRARIES} PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()
    
    message(WARNING "Library not found: ${library_name} (path: ${library_path})")
endfunction()

# common files
file(GLOB_RECURSE UTILITIES_SRC "${CMAKE_CURRENT_LIST_DIR}/include/utilities/*.c")
file(GLOB ADAPTER_SRC "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
set(INIT_SRC "${CMAKE_CURRENT_LIST_DIR}/include/init/src/tkl_init_common.c")
list(REMOVE_ITEM ADAPTER_SRC "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wired.c")

list(APPEND SOURCES 
    ${UTILITIES_SRC} 
    ${ADAPTER_SRC} 
    ${INIT_SRC}
)
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/include/adc
    ${CMAKE_CURRENT_LIST_DIR}/include/flash
    ${CMAKE_CURRENT_LIST_DIR}/include/gpio
    ${CMAKE_CURRENT_LIST_DIR}/include/i2c
    ${CMAKE_CURRENT_LIST_DIR}/include/init/include
    ${CMAKE_CURRENT_LIST_DIR}/include/network
    ${CMAKE_CURRENT_LIST_DIR}/include/pinmux
    ${CMAKE_CURRENT_LIST_DIR}/include/pwm
    ${CMAKE_CURRENT_LIST_DIR}/include/rtc
    ${CMAKE_CURRENT_LIST_DIR}/include/security
    ${CMAKE_CURRENT_LIST_DIR}/include/spi
    ${CMAKE_CURRENT_LIST_DIR}/include/system
    ${CMAKE_CURRENT_LIST_DIR}/include/uart
    ${CMAKE_CURRENT_LIST_DIR}/include/utilities/include
    ${CMAKE_CURRENT_LIST_DIR}/include/watchdog
    ${CMAKE_CURRENT_LIST_DIR}/include/tuyaopen
)

# camera files (V4L2)
if(CONFIG_ENABLE_CAMERA STREQUAL "y" AND CONFIG_ENABLE_CAMERA_V4L2 STREQUAL "y")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_camera/tkl_camera_v4l2.c")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_jpeg_codec/tkl_jpeg_codec.c")
    include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/include/camera
        ${CMAKE_CURRENT_LIST_DIR}/include/jpeg_codec
    )

    # JPEG codec dependency - use prebuilt static library
    set(LIBJPEG_PREBUILT_DIR "${CMAKE_CURRENT_LIST_DIR}/src/tkl_jpeg_codec/libs/${PLATFORM_CHIP}/libjpeg")
    include_directories("${LIBJPEG_PREBUILT_DIR}/include")
    list(APPEND TUYAOPEN_FOUND_LIBRARIES "${LIBJPEG_PREBUILT_DIR}/libjpeg.a")
endif()

# gpio files
if(CONFIG_ENABLE_GPIO STREQUAL "y")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_gpio.c")
endif()

# i2c files
if(CONFIG_ENABLE_I2C STREQUAL "y")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_i2c.c")
endif()

# spi files
if(CONFIG_ENABLE_SPI STREQUAL "y")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_spi.c")
endif()

# pwm files
if(CONFIG_ENABLE_PWM STREQUAL "y")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_pwm.c")
endif()

# uart files
if(CONFIG_ENABLE_UART STREQUAL "y")
    list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/tkl_uart.c")
endif()

# wire files
if(CONFIG_ENABLE_WIRED)
    list(APPEND SOURCES 
            "${CMAKE_CURRENT_LIST_DIR}/include/init/src/tkl_init_wired.c"
            "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wired.c"
    )
    include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/include/wired
    )
endif()

# wifi files
if(CONFIG_ENABLE_WIFI)
    file(GLOB_RECURSE WIFI_SRC "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wifi/*.c")
    list(APPEND INIT_SRC "${CMAKE_CURRENT_LIST_DIR}/include/init/src/tkl_init_wifi.c")
    list(APPEND WPA_INC
        "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wifi/iwlib"
        "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wifi/wpa_s"
        "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wifi/wpa_cli"
        "${CMAKE_CURRENT_LIST_DIR}/src/tkl_wifi/log"
    )

    list(APPEND SOURCES ${WIFI_SRC})
    include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/include/wifi
        ${WPA_INC}
    )
endif()

# bluetooth files
if(CONFIG_ENABLE_BLUETOOTH)
    file(GLOB_RECURSE BT_SRC "${CMAKE_SOURCE_DIR}/tuyaos_adapter/src/tkl_bt/bluez_inc/binc/*.c")
    list(APPEND BT_SRC 
        "${CMAKE_SOURCE_DIR}/tuyaos_adapter/src/tkl_bt/tkl_bluetooth.c"
        "${CMAKE_SOURCE_DIR}/tuyaos_adapter/src/tkl_bt/bt_dbus_api.c"
        "${CMAKE_SOURCE_DIR}/tuyaos_adapter/src/tkl_bt/bt_hci_adv_api.c"
    )

    set(BT_INC)
    list(APPEND BT_INC
        "${CMAKE_SOURCE_DIR}/tuyaos_adapter/src/tkl_bt/bluez_inc/binc"
        "${CMAKE_SOURCE_DIR}/tuyaos_adapter/include/bluetooth"
    )

    # Detect GLib and D-Bus paths using pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLIB2 glib-2.0 QUIET)
        pkg_check_modules(DBUS dbus-1 QUIET)
        
        if(GLIB2_FOUND)
            list(APPEND BT_INC ${GLIB2_INCLUDE_DIRS})
            message(STATUS "Bluetooth: Found GLib-2.0 via pkg-config: ${GLIB2_INCLUDE_DIRS}")
        else()
            message(WARNING "GLib-2.0 not found via pkg-config for Bluetooth")
        endif()
        
        if(DBUS_FOUND)
            list(APPEND BT_INC ${DBUS_INCLUDE_DIRS})
            message(STATUS "Bluetooth: Found D-Bus via pkg-config: ${DBUS_INCLUDE_DIRS}")
        else()
            message(WARNING "D-Bus not found via pkg-config for Bluetooth")
        endif()
    endif()
    
    # Fallback to manual path detection if pkg-config failed
    if(NOT GLIB2_FOUND OR NOT DBUS_FOUND)
        message(WARNING "Using manual path detection for Bluetooth dependencies")
        set(MANUAL_PATHS
            /usr/include/glib-2.0
            /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu/glib-2.0/include
            /usr/include/dbus-1.0
            /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu/dbus-1.0/include
        )
        foreach(PATH ${MANUAL_PATHS})
            if(EXISTS ${PATH})
                list(APPEND BT_INC ${PATH})
                message(STATUS "Bluetooth: Manually added include path: ${PATH}")
            endif()
        endforeach()
    endif()

    list(APPEND SOURCES ${BT_SRC})
    include_directories(${BT_INC})
endif()

# tuyaopen
list(APPEND SOURCES "${CMAKE_CURRENT_LIST_DIR}/tuyaopen/tuyaopen_license.c")

# audio files
if(CONFIG_ENABLE_AUDIO)
    file(GLOB_RECURSE AUDIO_SRC "${CMAKE_CURRENT_LIST_DIR}/src/tkl_audio/*.c")
    list(APPEND SOURCES ${AUDIO_SRC})
    include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/include/media
    )

    # audio library
    set(AUDIO_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/src/tkl_audio/libs/${PLATFORM_CHIP}")
    if (NOT EXISTS "${AUDIO_LIB_PATH}")
        message(FATAL_ERROR "Audio library path does not exist: ${AUDIO_LIB_PATH}")
    endif()
    
    ## audio subsystem library
    collect_library("${AUDIO_LIB_PATH}/audio_subsys" "audio_subsys")
    include_directories("${AUDIO_LIB_PATH}/audio_subsys/include")

    ## MNN library
    collect_library("${AUDIO_LIB_PATH}/MNN" "MNN")
    include_directories("${AUDIO_LIB_PATH}/MNN/include")

    ## audio codec library
    collect_library("${AUDIO_LIB_PATH}/opus" "opus")
    include_directories("${AUDIO_LIB_PATH}/opus/include")

    ## alsa library
    collect_library("${AUDIO_LIB_PATH}/alsa" "asound")
    include_directories("${AUDIO_LIB_PATH}/alsa/include")
endif()

# npu files (D-Robotics X5 BPU; auto-skipped when libdnn is absent, e.g. x86 CI)
if(EXISTS "/usr/lib/libdnn.so")
    file(GLOB NPU_SRC "${CMAKE_CURRENT_LIST_DIR}/src/tkl_npu/*.c")
    list(APPEND SOURCES ${NPU_SRC})
    include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/include/npu
        /usr/include/dnn
    )

    ## hobot dnn runtime (system library, not shipped in repo)
    collect_library("/usr/lib" "dnn")
    message(STATUS "NPU support enabled (X5 BPU)")
else()
    message(STATUS "NPU support skipped (libdnn.so not found)")
endif()

