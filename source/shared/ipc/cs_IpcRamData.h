/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Mar 18, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc/cs_IpcRamDataContents.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * IPC ram is a reserved section in ram to communicate between bootloader, application, and microapp.
 * It is also used to store data over reboots.
 *
 * The ram is split up into items. Each item serves for communication between 1 or 2 of the mentioned programs.
 * For example, the item at index bootloader info is for communication from the bootloader to the application.
 *
 * Note that we cannot assume the ram is zero initialized. This makes it harder to quickly check if an item has
 * valid data, we need to at least verify the checksum.
 *
 * Also, since IPC ram survives reboots, this also means we have to deal with data from a previous firmware version.
 *
 * Each item has a fixed maximum size. If we want to increase this, we break all the following items as well.
 */

// The number of slots for interprocess communication
#define BLUENET_IPC_RAM_DATA_ITEMS 5

// Version for major for the IPC header itself
#define BLUENET_IPC_HEADER_MAJOR 1

// Version for minor for the IPC header itself
#define BLUENET_IPC_HEADER_MINOR 0

/**
 * The IpcIndex indexes an IPC buffer in a particular set of RAM that is maintained across warm reboots.
 */
enum IpcIndex {
	IPC_INDEX_RESERVED        = 0,
	// To communicate from bluenet towards the microapp
	IPC_INDEX_CROWNSTONE_APP  = 1,
	// To communicate from bootloader to bluenet
	IPC_INDEX_BOOTLOADER_INFO = 2,
	// To communicate from microapp to bluenet
	IPC_INDEX_MICROAPP        = 3,
	// To communicate from bluenet towards bluenet (state across reboots, for now only about microapp state)
	IPC_INDEX_MICROAPP_STATE  = 4,
};

enum IpcRetCode {
	IPC_RET_SUCCESS            = 0,
	IPC_RET_INDEX_OUT_OF_BOUND = 1,
	IPC_RET_DATA_TOO_LARGE     = 2,
	IPC_RET_BUFFER_TOO_SMALL   = 3,
	IPC_RET_NOT_FOUND          = 4,
	IPC_RET_NULL_POINTER       = 5,
	IPC_RET_DATA_INVALID       = 6,
	IPC_RET_DATA_MAJOR_DIFF    = 7,
	IPC_RET_DATA_MINOR_DIFF    = 8,
};

enum BuildType {
	BUILD_TYPE_RESERVED       = 0,
	BUILD_TYPE_DEBUG          = 1,
	BUILD_TYPE_RELEASE        = 2,
	BUILD_TYPE_RELWITHDEBINFO = 3,
	BUILD_TYPE_MINSIZEREL     = 4,
};

/**
 * The header of a data item in IPC ram.
 *
 * Bump the major version if there's a change that is not backwards-compatible. Bump the minor version if there's a
 * change that is backwards-compatible, for example the addition of a field within the data object.
 */
typedef struct {
	// The major version.
	uint8_t major;
	// The minor version.
	uint8_t minor;
	// The index of this item, to see if this item has been set.
	uint8_t index;
	// How many bytes are informational within the data array.
	uint8_t dataSize;
	// Checksum calculated over the data array, but also all fields above.
	uint16_t checksum;
	// Reserve some bytes to be word aligned.
	uint8_t reserved[2];
} __attribute__((packed, aligned(4))) bluenet_ipc_data_header_t;

/**
 * One item of data in the IPC ram. The data is word-aligned.
 * The types of data are specified in ipc/cs_IpcRamDataContents.h
 */
typedef struct {
	// The header
	bluenet_ipc_data_header_t header;
	// The data array
	bluenet_ipc_data_t data;
} __attribute__((packed, aligned(4))) bluenet_ipc_ram_data_item_t;

/**
 * Have a fixed number of IPC ram data items.
 */
typedef struct {
	bluenet_ipc_ram_data_item_t item[BLUENET_IPC_RAM_DATA_ITEMS];
} __attribute__((packed, aligned(4))) bluenet_ipc_ram_data_t;

/**
 * Set data in IPC ram.
 *
 * @param[in] index          Index of IPC segment.
 * @param[in] data           Data pointer.
 * @param[in] dataSize       Size of data.
 * @return                   Error code (success is indicated by 0).
 */
enum IpcRetCode setRamData(uint8_t index, uint8_t* data, uint8_t dataSize);

/**
 * Get data from IPC ram.
 *
 * @param[in] index          Index of IPC segment.
 * @param[out] data          Buffer to copy the data to.
 * @param[out] dataSize      Size of data.
 * @param[in] maxSize        Size of the buffer to copy the data to (should be large enough).
 * @return                   Error code (success is indicated by 0).
 */
enum IpcRetCode getRamData(uint8_t index, uint8_t* data, uint8_t* dataSize, uint8_t maxSize);

/**
 * Get the IPC ram header.
 * Useful for debug, when getRamData failed.
 *
 * @param[out] header              Buffer to copy the header to.
 * @param[in] index                Index of IPC segment.
 * @param[in] verifyChecksum       Whether to verify the checksum.
 * @return                         Error code (success is indicated by 0).
 */
enum IpcRetCode getRamDataHeader(bluenet_ipc_data_header_t* header, uint8_t index, bool verifyChecksum);

/**
 * Clear RAM data.
 *
 * @param[in] index                Index of IPC segment.
 * @return                         Error code (success is indicated by 0).
 */
enum IpcRetCode clearRamData(uint8_t index);

/**
 * Get the underlying complete data struct. Do not use if not truly necessary. Its implementation might change.
 */
bluenet_ipc_ram_data_item_t* getRamStruct(uint8_t index);

#ifdef __cplusplus
}
#endif
