/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Dec 21, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */


#include <dfu/cs_MeshDfuTransport.h>
#include <dfu/cs_MeshDfuConstants.h>


#define LOGMeshDfuTransportDebug LOGd

cs_ret_code_t MeshDfuTransport::init() {
	if (!_firstInit) {
		return ERR_SUCCESS;
	}

	// Make use of the fact ERR_SUCCESS = 0, to avoid many if statements.
	cs_ret_code_t retCode = ERR_SUCCESS;
	retCode |= _dfuServiceUuid.fromShortUuid(MeshDfuConstants::DFUAdapter::dfuServiceShortUuid);

//	retCode |= _uuids[Index::ControlPoint].fromFullUuid(MeshDfuConstants::DFUAdapter::controlPointString);
//	retCode |= _uuids[Index::DataPoint].fromFullUuid(MeshDfuConstants::DFUAdapter::dataPointString);

	clearConnectionData();

	if (retCode != ERR_SUCCESS) {
		return retCode;
	}

	listen();

	_firstInit = false;
	return ERR_SUCCESS;
}

bool MeshDfuTransport::isTargetInDfuMode() {
	return _uuidHandles[Index::ControlPoint] != BLE_GATT_HANDLE_INVALID
		   && _uuidHandles[Index::DataPoint] != BLE_GATT_HANDLE_INVALID && _dfuServiceFound;
}

UUID* MeshDfuTransport::getServiceUuids(){
	return &_dfuServiceUuid;
}

uint8_t MeshDfuTransport::getServiceUuidCount() {
	return 1;
}

void MeshDfuTransport::clearConnectionData() {
	_uuidHandles[Index::ControlPoint] = BLE_GATT_HANDLE_INVALID;
	_uuidHandles[Index::DataPoint]    = BLE_GATT_HANDLE_INVALID;
	_dfuServiceFound = false;
	_discoveryComplete = false;
}


void MeshDfuTransport::onDisconnect() {
	clearConnectionData();
}

void MeshDfuTransport::onDiscover(ble_central_discovery_t& result) {
	if(_discoveryComplete) {
		LOGMeshDfuTransportDebug("Discovery was already completed. Continuing after clearing old data.");
		clearConnectionData();
	}

	if(result.uuid == _dfuServiceUuid) {
		LOGMeshDfuTransportDebug("Found dfu service handle");
		_dfuServiceFound = true;
		return;
	}

	UUID uuid;

	uuid.fromBaseUuid(_dfuServiceUuid, MeshDfuConstants::DFUAdapter::controlPointShortUuid);
	if (result.uuid == uuid) {
		LOGMeshDfuTransportDebug("Found dfu control characteristic handle");
		_uuidHandles[Index::ControlPoint] = result.valueHandle;
		return;
	}

	uuid.fromBaseUuid(_dfuServiceUuid, MeshDfuConstants::DFUAdapter::dataPointShortUuid);
	if (result.uuid == uuid) {
		LOGMeshDfuTransportDebug("Found dfu data characteristic handle");
		_uuidHandles[Index::DataPoint] = result.valueHandle;
		return;
	}
}

void MeshDfuTransport::onDiscoveryComplete() {
	_discoveryComplete = true;
}

void MeshDfuTransport::handleEvent(event_t& event) {
	switch(event.type) {
		case CS_TYPE::EVT_BLE_CENTRAL_DISCOVERY: {
			auto result = CS_TYPE_CAST(EVT_BLE_CENTRAL_DISCOVERY, event.data);
			onDiscover(*result);
			break;
		}
		case CS_TYPE::EVT_BLE_CENTRAL_DISCOVERY_RESULT: {
			onDiscoveryComplete();
			break;
		}

		case CS_TYPE::EVT_BLE_CENTRAL_DISCONNECTED: {
			onDisconnect();
			break;
		}
		case CS_TYPE::EVT_BLE_CENTRAL_CONNECT_RESULT: break;
		case CS_TYPE::EVT_BLE_CENTRAL_CONNECT_CLEARANCE_REQUEST: break;

		case CS_TYPE::EVT_BLE_CENTRAL_NOTIFICATION: break;
		case CS_TYPE::EVT_BLE_CENTRAL_READ_RESULT: break;
		case CS_TYPE::EVT_BLE_CENTRAL_WRITE_RESULT: break;
		default: break;
	}
}
