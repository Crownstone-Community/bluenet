# This file can load variables and set them in multiple ways. 
#
# 1. From the configuration files there are variables that have to be added to compiler as definitions. For example, 
#    -DBOARD_PCA10040 for the (Nordic) mesh code. 
# 2. There are variables that need to be known to CMake, however, these are already loaded. The only reason to call
#    SET(...) would be to also add them to the cache. This has often undesired side effects.
#
# TODO: A lot of those definitions can be removed. Both from the bottom SET() list as well as precompiler definitions.


ADD_DEFINITIONS("-MMD -DNRF52 -DNRF52_SERIES -DEPD_ENABLE_EXTRA_RAM -DNRF51_USE_SOFTDEVICE=${NRF51_USE_SOFTDEVICE} -DUSE_RENDER_CONTEXT -DSYSCALLS -DUSING_FUNC -DDEBUG_NRF")

LIST(APPEND CUSTOM_DEFINITIONS, TEMPERATURE)

ADD_DEFINITIONS("-DBLE_STACK_SUPPORT_REQD")

IF(NRF51_USE_SOFTDEVICE)
	ADD_DEFINITIONS("-DSOFTDEVICE_PRESENT")
ENDIF()

MESSAGE(STATUS "crownstone.defs.cmake: Build type: ${CMAKE_BUILD_TYPE}")
IF(CMAKE_BUILD_TYPE MATCHES "Debug")
	ADD_DEFINITIONS("-DDEBUG")
ELSEIF(CMAKE_BUILD_TYPE MATCHES "Release")
	MESSAGE(STATUS "Release build")
ELSEIF(CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
ELSEIF(CMAKE_BUILD_TYPE MATCHES "MinSizeRel")
ELSE()
	MESSAGE(FATAL_ERROR "There should be a CMAKE_BUILD_TYPE defined (and known)")
ENDIF()

# The bluetooth name is not optional
IF(DEFINED BLUETOOTH_NAME)
	ADD_DEFINITIONS("-DBLUETOOTH_NAME=${BLUETOOTH_NAME}")
ELSE()
	MESSAGE(FATAL_ERROR "We require a BLUETOOTH_NAME in CMakeBuild.config (5 characters or less), i.e. \"Crown\" (with quotes)")
ENDIF()

IF(NOT DEFINED CS_SERIAL_ENABLED OR CS_SERIAL_ENABLED STREQUAL "0")
	MESSAGE(STATUS "Crownstone serial disabled")
ELSE()
	MESSAGE(STATUS "Crownstone serial enabled")
ENDIF()

# Copied from an example makefile.
ADD_DEFINITIONS("-DBLE_STACK_SUPPORT_REQD")

IF(DEFINED NORDIC_HARDWARE_BOARD) 
	IF(NORDIC_HARDWARE_BOARD MATCHES "PCA10040")
		ADD_DEFINITIONS("-DBOARD_PCA10040")
	ENDIF()
	IF(NORDIC_HARDWARE_BOARD MATCHES "PCA10100")
		ADD_DEFINITIONS("-DBOARD_PCA10100")
	ENDIF()
ELSE()
	MESSAGE(WARNING "Assuming NORDIC_HARDWARE_BOARD MATCHES to be PCA10040")
	ADD_DEFINITIONS("-DBOARD_PCA10040")
ENDIF()

#ADD_DEFINITIONS("-DCONFIG_GPIO_AS_PINRESET") TODO: only define this if board is pca10040?
ADD_DEFINITIONS("-DFLOAT_ABI_HARD")
ADD_DEFINITIONS("-DMBEDTLS_CONFIG_FILE=\"nrf_crypto_mbedtls_config.h\"")
ADD_DEFINITIONS("-DNRF52")
ADD_DEFINITIONS("-DNRF52832_XXAA")
ADD_DEFINITIONS("-DNRF52_PAN_74")
ADD_DEFINITIONS("-DNRF_CRYPTO_MAX_INSTANCE_COUNT=1")
ADD_DEFINITIONS("-DNRF_SD_BLE_API_VERSION=${SOFTDEVICE_MAJOR}")
ADD_DEFINITIONS("-DS${SOFTDEVICE_SERIES}")
ADD_DEFINITIONS("-DSOFTDEVICE_PRESENT")
ADD_DEFINITIONS("-DSWI_DISABLE0")
ADD_DEFINITIONS("-DuECC_ENABLE_VLI_API=0")
ADD_DEFINITIONS("-DuECC_OPTIMIZATION_LEVEL=3")
ADD_DEFINITIONS("-DuECC_SQUARE_FUNC=0")
ADD_DEFINITIONS("-DuECC_SUPPORT_COMPRESSED_POINT=0")
ADD_DEFINITIONS("-DuECC_VLI_NATIVE_LITTLE_ENDIAN=1")

# For mesh SDK:
#IF (DEFINED BUILD_MESHING AND "${BUILD_MESHING}" STRGREATER "0")
IF (BUILD_MESHING)
	ADD_DEFINITIONS("-DCONFIG_APP_IN_CORE")
	ADD_DEFINITIONS("-DNRF52832")
	SET(EXPERIMENTAL_INSTABURST_ENABLED OFF)
	IF (EXPERIMENTAL_INSTABURST_ENABLED)
		ADD_DEFINITIONS("-DEXPERIMENTAL_INSTABURST_ENABLED")
	ENDIF()
	SET(MESH_MEM_BACKEND "stdlib")
	# HF timer peripheral index to allocate for bearer handler. E.g. if set to 2, NRF_TIMER2 will be used. Must be a literal number.
	ADD_DEFINITIONS("-DBEARER_ACTION_TIMER_INDEX=3")
ENDIF()

# Overwrite sdk config options
ADD_DEFINITIONS("-DUSE_APP_CONFIG")

# Pass variables in defined in the configuration file to the compiler
ADD_DEFINITIONS("-DNRF5_DIR=${NRF5_DIR}")
ADD_DEFINITIONS("-DNORDIC_SDK_VERSION=${NORDIC_SDK_VERSION}")
ADD_DEFINITIONS("-DSOFTDEVICE_SERIES=${SOFTDEVICE_SERIES}")
ADD_DEFINITIONS("-DSOFTDEVICE_MAJOR=${SOFTDEVICE_MAJOR}")
ADD_DEFINITIONS("-DSOFTDEVICE_MINOR=${SOFTDEVICE_MINOR}")
ADD_DEFINITIONS("-DSOFTDEVICE=${SOFTDEVICE}")
ADD_DEFINITIONS("-DSOFTDEVICE_NO_SEPARATE_UICR_SECTION=${SOFTDEVICE_NO_SEPARATE_UICR_SECTION}")
ADD_DEFINITIONS("-DSERIAL_VERBOSITY=${SERIAL_VERBOSITY}")
ADD_DEFINITIONS("-DCS_SERIAL_NRF_LOG_ENABLED=${CS_SERIAL_NRF_LOG_ENABLED}")
ADD_DEFINITIONS("-DCS_SERIAL_BOOTLOADER_NRF_LOG_ENABLED=${CS_SERIAL_BOOTLOADER_NRF_LOG_ENABLED}")
ADD_DEFINITIONS("-DCS_SERIAL_NRF_LOG_PIN_TX=${CS_SERIAL_NRF_LOG_PIN_TX}")
ADD_DEFINITIONS("-DMASTER_BUFFER_SIZE=${MASTER_BUFFER_SIZE}")
ADD_DEFINITIONS("-DDEFAULT_ON=${DEFAULT_ON}")
ADD_DEFINITIONS("-DENABLE_RSSI_FOR_CONNECTION=${ENABLE_RSSI_FOR_CONNECTION}")
ADD_DEFINITIONS("-DTX_POWER=${TX_POWER}")
ADD_DEFINITIONS("-DADVERTISEMENT_INTERVAL=${ADVERTISEMENT_INTERVAL}")
ADD_DEFINITIONS("-DPWM_ENABLE=${PWM_ENABLE}")
ADD_DEFINITIONS("-DFIRMWARE_VERSION=${FIRMWARE_VERSION}")
ADD_DEFINITIONS("-DBOOT_DELAY=${BOOT_DELAY}")
ADD_DEFINITIONS("-DSCAN_DURATION=${SCAN_DURATION}")
ADD_DEFINITIONS("-DSCAN_BREAK_DURATION=${SCAN_BREAK_DURATION}")
ADD_DEFINITIONS("-DMAX_CHIP_TEMP=${MAX_CHIP_TEMP}")
ADD_DEFINITIONS("-DINTERVAL_SCANNER_ENABLED=${INTERVAL_SCANNER_ENABLED}")
ADD_DEFINITIONS("-DNRF_SERIES=${NRF_SERIES}")
ADD_DEFINITIONS("-DMAX_NUM_VS_SERVICES=${MAX_NUM_VS_SERVICES}")
ADD_DEFINITIONS("-DCONNECTION_ALIVE_TIMEOUT=${CONNECTION_ALIVE_TIMEOUT}")
ADD_DEFINITIONS("-DCS_UART_BINARY_PROTOCOL_ENABLED=${CS_UART_BINARY_PROTOCOL_ENABLED}")
ADD_DEFINITIONS("-DCS_SERIAL_ENABLED=${CS_SERIAL_ENABLED}")
IF(DEFINED RELAY_DEFAULT_ON)
ADD_DEFINITIONS("-DRELAY_DEFAULT_ON=${RELAY_DEFAULT_ON}")
ENDIF()

# UICR settings
ADD_DEFINITIONS("-DUICR_DFU_INDEX=${UICR_DFU_INDEX}")
ADD_DEFINITIONS("-DUICR_BOARD_INDEX=${UICR_BOARD_INDEX}")

# Set Attribute table size
ADD_DEFINITIONS("-DATTR_TABLE_SIZE=${ATTR_TABLE_SIZE}")

# Add encryption
ADD_DEFINITIONS("-DENCRYPTION=${ENCRYPTION}")

# Add mesh settings
ADD_DEFINITIONS("-DMESHING=${MESHING}")
ADD_DEFINITIONS("-DBUILD_MESHING=${BUILD_MESHING}")
ADD_DEFINITIONS("-DMESH_SCANNER=${MESH_SCANNER}")
ADD_DEFINITIONS("-DMESH_PERSISTENT_STORAGE=${MESH_PERSISTENT_STORAGE}")

# Add microapp settings
ADD_DEFINITIONS("-DBUILD_MICROAPP_SUPPORT=${BUILD_MICROAPP_SUPPORT}")
ADD_DEFINITIONS("-DRAM_MICROAPP_BASE=${RAM_MICROAPP_BASE}")
ADD_DEFINITIONS("-DRAM_MICROAPP_AMOUNT=${RAM_MICROAPP_AMOUNT}")

# Add twi driver
ADD_DEFINITIONS("-DBUILD_TWI=${BUILD_TWI}")

# 
ADD_DEFINITIONS("-DRSSI_DATA_TRACKER_ENABLED=${RSSI_DATA_TRACKER_ENABLED}")
ADD_DEFINITIONS("-DCLOSEST_CROWNSTONE_TRACKER_ENABLED=${CLOSEST_CROWNSTONE_TRACKER_ENABLED}")


# Add iBeacon default values
ADD_DEFINITIONS("-DIBEACON=${IBEACON}")
#IF(IBEACON)
ADD_DEFINITIONS("-DBEACON_UUID=${BEACON_UUID}")
ADD_DEFINITIONS("-DBEACON_MAJOR=${BEACON_MAJOR}")
ADD_DEFINITIONS("-DBEACON_MINOR=${BEACON_MINOR}")
ADD_DEFINITIONS("-DBEACON_RSSI=${BEACON_RSSI}")
#ENDIF()

ADD_DEFINITIONS("-DCHANGE_NAME_ON_RESET=${CHANGE_NAME_ON_RESET}")

# Add characteristics
ADD_DEFINITIONS("-DCHAR_CONTROL=${CHAR_CONTROL}")

# Publish all options as CMake options as well

# Obtain variables to be used for the compiler
SET(NRF5_DIR                                    "${NRF5_DIR}"                       CACHE STRING "Nordic SDK Directory" FORCE)
SET(NORDIC_SDK_VERSION                          "${NORDIC_SDK_VERSION}"             CACHE STRING "Nordic SDK Version" FORCE)
SET(SOFTDEVICE_SERIES                           "${SOFTDEVICE_SERIES}"              CACHE STRING "SOFTDEVICE_SERIES" FORCE)
SET(SOFTDEVICE_MAJOR                            "${SOFTDEVICE_MAJOR}"               CACHE STRING "SOFTDEVICE_MAJOR" FORCE)
SET(SOFTDEVICE_MINOR                            "${SOFTDEVICE_MINOR}"               CACHE STRING "SOFTDEVICE_MINOR" FORCE)
SET(SOFTDEVICE                                  "${SOFTDEVICE}"                     CACHE STRING "SOFTDEVICE" FORCE)
SET(SOFTDEVICE_NO_SEPARATE_UICR_SECTION         "${SOFTDEVICE_NO_SEPARATE_UICR_SECTION}"   CACHE STRING "SOFTDEVICE_NO_SEPARATE_UICR_SECTION" FORCE)
IF(DEFINED GIT_HASH)
SET(GIT_HASH                                    "${GIT_HASH}"                       CACHE STRING "GIT_HASH" FORCE)
ENDIF()
SET(SERIAL_VERBOSITY                            "${SERIAL_VERBOSITY}"               CACHE STRING "SERIAL_VERBOSITY" FORCE)
SET(CS_SERIAL_NRF_LOG_ENABLED                   "${CS_SERIAL_NRF_LOG_ENABLED}"      CACHE STRING "CS_SERIAL_NRF_LOG_ENABLED" FORCE)
SET(CS_SERIAL_NRF_LOG_PIN_TX                    "${CS_SERIAL_NRF_LOG_PIN_TX}"       CACHE STRING "CS_SERIAL_NRF_LOG_PIN_TX" FORCE)
SET(CS_SERIAL_ENABLED                           "${CS_SERIAL_ENABLED}"              CACHE STRING "CS_SERIAL_ENABLED" FORCE)
SET(MASTER_BUFFER_SIZE                          "${MASTER_BUFFER_SIZE}"             CACHE STRING "MASTER_BUFFER_SIZE" FORCE)
SET(DEFAULT_ON                                  "${DEFAULT_ON}"                     CACHE STRING "DEFAULT_ON" FORCE)
SET(ENABLE_RSSI_FOR_CONNECTION                  "${ENABLE_RSSI_FOR_CONNECTION}"     CACHE STRING "ENABLE_RSSI_FOR_CONNECTION" FORCE)
SET(TX_POWER                                    "${TX_POWER}"                       CACHE STRING "TX_POWER" FORCE)
SET(ADVERTISEMENT_INTERVAL                      "${ADVERTISEMENT_INTERVAL}"         CACHE STRING "ADVERTISEMENT_INTERVAL" FORCE)
SET(PWM_ENABLE                                  "${PWM_ENABLE}"                     CACHE STRING "PWM_ENABLE" FORCE)
SET(FIRMWARE_VERSION                            "${FIRMWARE_VERSION}"               CACHE STRING "FIRMWARE_VERSION" FORCE)
SET(BOOT_DELAY                                  "${BOOT_DELAY}"                     CACHE STRING "BOOT_DELAY" FORCE)
SET(SCAN_DURATION                               "${SCAN_DURATION}"                  CACHE STRING "SCAN_DURATION" FORCE)
SET(SCAN_BREAK_DURATION                         "${SCAN_BREAK_DURATION}"            CACHE STRING "SCAN_BREAK_DURATION" FORCE)
SET(MAX_CHIP_TEMP                               "${MAX_CHIP_TEMP}"                  CACHE STRING "MAX_CHIP_TEMP" FORCE)
SET(INTERVAL_SCANNER_ENABLED                    "${INTERVAL_SCANNER_ENABLED}"       CACHE STRING "INTERVAL_SCANNER_ENABLED" FORCE)
SET(NRF_SERIES                                  "${NRF_SERIES}"                     CACHE STRING "NRF_SERIES" FORCE)
SET(MAX_NUM_VS_SERVICES                         "${MAX_NUM_VS_SERVICES}"            CACHE STRING "MAX_NUM_VS_SERVICES" FORCE)
IF(DEFINED RELAY_DEFAULT_ON)
SET(RELAY_DEFAULT_ON                            "${RELAY_DEFAULT_ON}"               CACHE STRING "RELAY_DEFAULT_ON" FORCE)
ENDIF()
SET(CONNECTION_ALIVE_TIMEOUT                    "${CONNECTION_ALIVE_TIMEOUT}"       CACHE STRING "CONNECTION_ALIVE_TIMEOUT" FORCE)

# Set Attribute table size
SET(ATTR_TABLE_SIZE                             "${ATTR_TABLE_SIZE}"                CACHE STRING "ATTR_TABLE_SIZE" FORCE)

# Add encryption
SET(ENCRYPTION                                  "${ENCRYPTION}"                     CACHE STRING "ENCRYPTION" FORCE)

SET(MESHING                                     "${MESHING}"                        CACHE STRING "MESHING" FORCE)
SET(BUILD_MESHING                               "${BUILD_MESHING}"                  CACHE STRING "BUILD_MESHING" FORCE)
SET(MESH_SCANNER                                "${MESH_SCANNER}"                   CACHE STRING "MESH_SCANNER" FORCE)
SET(MESH_PERSISTENT_STORAGE                     "${MESH_PERSISTENT_STORAGE}"        CACHE STRING "MESH_PERSISTENT_STORAGE" FORCE)

# Add iBeacon default values
SET(IBEACON                                     "${IBEACON}"                        CACHE STRING "IBEACON" FORCE)
SET(BEACON_UUID                                 "${BEACON_UUID}"                    CACHE STRING "BEACON_UUID" FORCE)
SET(BEACON_MAJOR                                "${BEACON_MAJOR}"                   CACHE STRING "BEACON_MAJOR" FORCE)
SET(BEACON_MINOR                                "${BEACON_MINOR}"                   CACHE STRING "BEACON_MINOR" FORCE)
SET(BEACON_RSSI                                 "${BEACON_RSSI}"                    CACHE STRING "BEACON_RSSI" FORCE)

# Add characteristics
SET(CHAR_CONTROL                                "${CHAR_CONTROL}"                   CACHE STRING "CHAR_CONTROL" FORCE)

