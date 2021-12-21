/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Dec 21, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include<util/cs_Utils.h>

/**
 * namespace to keep track of the constants related to Mesh DFU.
 *
 * NOTE: corresponds roughly to ble_constans.py and incorporates ble_uuid.py
 */
namespace MeshDfuConstants {
namespace DFUAdapter {
//    BASE_UUID = BLEUUIDBase([0x8E, 0xC9, 0x00, 0x00, 0xF3, 0x15, 0x4F, 0x60,
//                                 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50])

//    # Bootloader characteristics
//    CP_UUID     = BLEUUID(0x0001, BASE_UUID) // service_primary = 0x2800
//    DP_UUID     = BLEUUID(0x0002, BASE_UUID) // service_secondary = 0x2801


constexpr uint8_t CONNECTION_ATTEMPTS = 3;
constexpr uint8_t ERROR_CODE_POS      = 2;
constexpr uint8_t LOCAL_ATT_MTU       = 200;

}  // namespace DFUAdapter

namespace DfuTransportBle {
constexpr uint8_t DEFAULT_TIMEOUT = 20;
constexpr uint8_t RETRIES_NUMBER  = 5;

enum class OP_CODE : uint8_t {
	CreateObject = 0x01,
	SetPRN       = 0x02,
	CalcChecSum  = 0x03,
	Execute      = 0x04,
	ReadObject   = 0x06,
	Response     = 0x60,
};

enum class RES_CODE : uint8_t {
	InvalidCode           = 0x00,
	Success               = 0x01,
	NotSupported          = 0x02,
	InvalidParameter      = 0x03,
	InsufficientResources = 0x04,
	InvalidObject         = 0x05,
	InvalidSignature      = 0x06,
	UnsupportedType       = 0x07,
	OperationNotPermitted = 0x08,
	OperationFailed       = 0x0A,
	ExtendedError         = 0x0B,
};

constexpr auto EXT_ERROR_CODE(uint8_t errorcode) {
	auto codes = {
			"No extended error code has been set. This error indicates an implementation problem.",
			"Invalid error code. This error code should never be used outside of development.",
			"The format of the command was incorrect. This error code is not used in the current implementation, "
			"because @ref NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED and @ref NRF_DFU_RES_CODE_INVALID_PARAMETER cover all "
			"possible format errors.",
			"The command was successfully parsed, but it is not supported or unknown.",
			"The init command is invalid. The init packet either has an invalid update type or it is missing required "
			"fields for the update type (for example, the init packet for a SoftDevice update is missing the "
			"SoftDevice size field).",
			"The firmware version is too low. For an application, the version must be greater than or equal to the "
			"current application. For a bootloader, it must be greater than the current version. This requirement "
			"prevents downgrade attacks.",
			"The hardware version of the device does not match the required hardware version for the update.",
			"The array of supported SoftDevices for the update does not contain the FWID of the current SoftDevice.",
			"The init packet does not contain a signature, but this bootloader requires all updates to have one.",
			"The hash type that is specified by the init packet is not supported by the DFU bootloader.",
			"The hash of the firmware image cannot be calculated.",
			"The type of the signature is unknown or not supported by the DFU bootloader.",
			"The hash of the received firmware image does not match the hash in the init packet.",
			"The available space on the device is insufficient to hold the firmware.",
			"The requested firmware to update was already present on the system."};

	if (errorcode < ArraySize(codes)) {
		return codes[errorcode];
	}

	return "";
}
}  // namespace DfuTransportBle

}  //  namespace MeshDfuConstants
