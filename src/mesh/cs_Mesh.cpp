/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 12 Apr., 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include "mesh/cs_Mesh.h"

extern "C" {
#include "nrf_mesh.h"
#include "mesh_config.h"
#include "mesh_stack.h"
#include "access.h"
#include "access_config.h"
#include "config_server.h"
#include "config_server_events.h"
#include "device_state_manager.h"
#include "mesh_provisionee.h"
#include "nrf_mesh_configure.h"
#include "nrf_mesh_events.h"
#include "net_state.h"
#include "uri.h"
#include "utils.h"
#include "log.h"
}

#include "cfg/cs_Boards.h"
#include "drivers/cs_Serial.h"
#include "util/cs_BleError.h"
#include "util/cs_Utils.h"
#include "ble/cs_Stack.h"
#include "events/cs_EventDispatcher.h"

#define LOGMeshDebug LOGnone

#if NRF_MESH_KEY_SIZE != ENCRYPTION_KEY_LENGTH
#error "Mesh key size doesn't match encryption key size"
#endif

static void cs_mesh_event_handler(const nrf_mesh_evt_t * p_evt) {
//	LOGi("Mesh event type=%u", p_evt->type);
	switch(p_evt->type) {
	case NRF_MESH_EVT_MESSAGE_RECEIVED:
		LOGMeshDebug("NRF_MESH_EVT_MESSAGE_RECEIVED");
//		LOGi("NRF_MESH_EVT_MESSAGE_RECEIVED");
//		LOGi("src=%u data:", p_evt->params.message.p_metadata->source);
//		BLEutil::printArray(p_evt->params.message.p_buffer, p_evt->params.message.length);
		break;
	case NRF_MESH_EVT_TX_COMPLETE:
		LOGMeshDebug("NRF_MESH_EVT_TX_COMPLETE");
		break;
	case NRF_MESH_EVT_IV_UPDATE_NOTIFICATION:
		LOGMeshDebug("NRF_MESH_EVT_IV_UPDATE_NOTIFICATION");
		break;
	case NRF_MESH_EVT_KEY_REFRESH_NOTIFICATION:
		LOGMeshDebug("NRF_MESH_EVT_KEY_REFRESH_NOTIFICATION");
		break;
	case NRF_MESH_EVT_NET_BEACON_RECEIVED:
		LOGMeshDebug("NRF_MESH_EVT_NET_BEACON_RECEIVED");
		break;
	case NRF_MESH_EVT_HB_MESSAGE_RECEIVED:
		LOGMeshDebug("NRF_MESH_EVT_HB_MESSAGE_RECEIVED");
		break;
	case NRF_MESH_EVT_HB_SUBSCRIPTION_CHANGE:
		LOGMeshDebug("NRF_MESH_EVT_HB_SUBSCRIPTION_CHANGE");
		break;
	case NRF_MESH_EVT_DFU_REQ_RELAY:
		LOGMeshDebug("NRF_MESH_EVT_DFU_REQ_SOURCE");
		break;
	case NRF_MESH_EVT_DFU_REQ_SOURCE:
		LOGMeshDebug("NRF_MESH_EVT_DFU_REQ_SOURCE");
		break;
	case NRF_MESH_EVT_DFU_START:
		LOGMeshDebug("NRF_MESH_EVT_DFU_START");
		break;
	case NRF_MESH_EVT_DFU_END:
		LOGMeshDebug("NRF_MESH_EVT_DFU_END");
		break;
	case NRF_MESH_EVT_DFU_BANK_AVAILABLE:
		LOGMeshDebug("NRF_MESH_EVT_DFU_BANK_AVAILABLE");
		break;
	case NRF_MESH_EVT_DFU_FIRMWARE_OUTDATED:
		LOGMeshDebug("NRF_MESH_EVT_DFU_FIRMWARE_OUTDATED");
		break;
	case NRF_MESH_EVT_DFU_FIRMWARE_OUTDATED_NO_AUTH:
		LOGMeshDebug("NRF_MESH_EVT_DFU_FIRMWARE_OUTDATED_NO_AUTH");
		break;
	case NRF_MESH_EVT_FLASH_STABLE:
		LOGMeshDebug("NRF_MESH_EVT_FLASH_STABLE");
		break;
	case NRF_MESH_EVT_RX_FAILED:
		LOGMeshDebug("NRF_MESH_EVT_RX_FAILED");
		break;
	case NRF_MESH_EVT_SAR_FAILED:
		LOGMeshDebug("NRF_MESH_EVT_SAR_FAILED");
		break;
	case NRF_MESH_EVT_FLASH_FAILED:
		LOGMeshDebug("NRF_MESH_EVT_FLASH_FAILED");
		break;
	case NRF_MESH_EVT_CONFIG_STABLE:
		LOGMeshDebug("NRF_MESH_EVT_CONFIG_STABLE");
		break;
	case NRF_MESH_EVT_CONFIG_STORAGE_FAILURE:
		LOGMeshDebug("NRF_MESH_EVT_CONFIG_STORAGE_FAILURE");
		break;
	case NRF_MESH_EVT_CONFIG_LOAD_FAILURE:
		LOGMeshDebug("NRF_MESH_EVT_CONFIG_LOAD_FAILURE");
		break;
	case NRF_MESH_EVT_LPN_FRIEND_OFFER:
		LOGMeshDebug("NRF_MESH_EVT_LPN_FRIEND_OFFER");
		break;
	case NRF_MESH_EVT_LPN_FRIEND_UPDATE:
		LOGMeshDebug("NRF_MESH_EVT_LPN_FRIEND_UPDATE");
		break;
	case NRF_MESH_EVT_LPN_FRIEND_REQUEST_TIMEOUT:
		LOGMeshDebug("NRF_MESH_EVT_LPN_FRIEND_REQUEST_TIMEOUT");
		break;
	case NRF_MESH_EVT_LPN_FRIEND_POLL_COMPLETE:
		LOGMeshDebug("NRF_MESH_EVT_LPN_FRIEND_POLL_COMPLETE");
		break;
	case NRF_MESH_EVT_FRIENDSHIP_ESTABLISHED:
		LOGMeshDebug("NRF_MESH_EVT_FRIENDSHIP_ESTABLISHED");
		break;
	case NRF_MESH_EVT_FRIENDSHIP_TERMINATED:
		LOGMeshDebug("NRF_MESH_EVT_FRIENDSHIP_TERMINATED");
		break;
	case NRF_MESH_EVT_DISABLED:
		LOGMeshDebug("NRF_MESH_EVT_DISABLED");
		break;
	}
}
static nrf_mesh_evt_handler_t cs_mesh_event_handler_struct = {
		cs_mesh_event_handler
};



static void config_server_evt_cb(const config_server_evt_t * p_evt) {
	if (p_evt->type == CONFIG_SERVER_EVT_NODE_RESET) {
		LOGi("----- Node reset  -----");
		/* This function may return if there are ongoing flash operations. */
		mesh_stack_device_reset();
	}
}



static void scan_cb(const nrf_mesh_adv_packet_rx_data_t *p_rx_data) {
	switch (p_rx_data->p_metadata->source) {
	case NRF_MESH_RX_SOURCE_SCANNER:{
//	    timestamp_t timestamp; /**< Device local timestamp of the start of the advertisement header of the packet in microseconds. */
//	    uint32_t access_addr; /**< Access address the packet was received on. */
//	    uint8_t  channel; /**< Channel the packet was received on. */
//	    int8_t   rssi; /**< RSSI value of the received packet. */
//	    ble_gap_addr_t adv_addr; /**< Advertisement address in the packet. */
//	    uint8_t adv_type;  /**< BLE GAP advertising type. */

//		const uint8_t* addr = p_rx_data->p_metadata->params.scanner.adv_addr.addr;
//		const uint8_t* p = p_rx_data->p_payload;
//		if (p[0] == 0x15 && p[1] == 0x16 && p[2] == 0x01 && p[3] == 0xC0 && p[4] == 0x05) {
////		if (p[1] == 0xFF && p[2] == 0xCD && p[3] == 0xAB) {
////		if (addr[5] == 0xE7 && addr[4] == 0x09 && addr[3] == 0x62) { // E7:09:62:02:91:3D
//			LOGd("Mesh scan: address=%02X:%02X:%02X:%02X:%02X:%02X type=%u rssi=%i chan=%u", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0], p_rx_data->p_metadata->params.scanner.adv_type, p_rx_data->p_metadata->params.scanner.rssi, p_rx_data->p_metadata->params.scanner.channel);
//			LOGd("  adv_type=%u len=%u data=", p_rx_data->adv_type, p_rx_data->length);
//			BLEutil::printArray(p_rx_data->p_payload, p_rx_data->length);
//		}

		scanned_device_t scan;
		memcpy(scan.address, p_rx_data->p_metadata->params.scanner.adv_addr.addr, sizeof(scan.address)); // TODO: check addr_type and addr_id_peer
		scan.addressType = (p_rx_data->p_metadata->params.scanner.adv_addr.addr_type & 0x7F) & ((p_rx_data->p_metadata->params.scanner.adv_addr.addr_id_peer & 0x01) << 7);
		scan.rssi = p_rx_data->p_metadata->params.scanner.rssi;
		scan.channel = p_rx_data->p_metadata->params.scanner.channel;
		scan.dataSize = p_rx_data->length;
		scan.data = (uint8_t*)(p_rx_data->p_payload);
		event_t event(CS_TYPE::EVT_DEVICE_SCANNED, (void*)&scan, sizeof(scan));
		EventDispatcher::getInstance().dispatch(event);
		break;
	}
	case NRF_MESH_RX_SOURCE_GATT:

		break;
	case NRF_MESH_RX_SOURCE_FRIEND:

		break;
	case NRF_MESH_RX_SOURCE_LOW_POWER:

		break;
	case NRF_MESH_RX_SOURCE_INSTABURST:

		break;
	case NRF_MESH_RX_SOURCE_LOOPBACK:
		break;
	}
}






void Mesh::modelsInitCallback() {
	LOGi("Initializing and adding models");
	_model.init();
	_model.setOwnAddress(_ownAddress);
}


Mesh::Mesh() {

}

Mesh& Mesh::getInstance() {
	// Use static variant of singleton, no dynamic memory allocation
	static Mesh instance;
	return instance;
}

void Mesh::init() {
#if CS_SERIAL_NRF_LOG_ENABLED == 1
	__LOG_INIT(LOG_SRC_APP | LOG_SRC_PROV | LOG_SRC_ACCESS | LOG_SRC_BEARER | LOG_SRC_TRANSPORT | LOG_SRC_NETWORK, LOG_LEVEL_DBG3, LOG_CALLBACK_DEFAULT);
	__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "----- Mesh init -----\n");
#endif
	nrf_clock_lf_cfg_t lfclksrc;
	lfclksrc.source = NRF_SDH_CLOCK_LF_SRC;
	lfclksrc.rc_ctiv = NRF_SDH_CLOCK_LF_RC_CTIV;
	lfclksrc.rc_temp_ctiv = NRF_SDH_CLOCK_LF_RC_TEMP_CTIV;
	lfclksrc.accuracy = NRF_CLOCK_LF_ACCURACY_20_PPM;

	mesh_stack_init_params_t init_params;
	init_params.core.irq_priority       = NRF_MESH_IRQ_PRIORITY_THREAD; // See mesh_interrupt_priorities.md
	init_params.core.lfclksrc           = lfclksrc;
	init_params.core.p_uuid             = NULL;
	init_params.core.relay_cb           = NULL;
	init_params.models.models_init_cb   = staticModelsInitCallback;
	init_params.models.config_server_cb = config_server_evt_cb;

	TYPIFY(CONFIG_CROWNSTONE_ID) id;
	State::getInstance().get(CS_TYPE::CONFIG_CROWNSTONE_ID, &id, sizeof(id));
	_ownAddress = id;

	uint32_t retCode = mesh_stack_init(&init_params, &_isProvisioned);
	APP_ERROR_CHECK(retCode);
	LOGi("Mesh isProvisioned=%u", _isProvisioned);

	nrf_mesh_evt_handler_add(&cs_mesh_event_handler_struct);

	nrf_mesh_rx_cb_set(scan_cb);

//	EventDispatcher::getInstance().addListener(this);
	if (!_isProvisioned) {
		provisionSelf(_ownAddress);
	}
	else {
		provisionLoad();
	}

	retCode = dsm_address_publish_add(0xC51E, &_groupAddressHandle);
	APP_ERROR_CHECK(retCode);

//    retCode = dsm_address_subscription_add(0xC51E, &subscriptionAddressHandle);
    retCode = dsm_address_subscription_add_handle(_groupAddressHandle);
    APP_ERROR_CHECK(retCode);

	access_model_handle_t handle = _model.getAccessModelHandle();
	retCode = access_model_application_bind(handle, _appkeyHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_publish_application_set(handle, _appkeyHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_publish_address_set(handle, _groupAddressHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_subscription_add(handle, _groupAddressHandle);
	APP_ERROR_CHECK(retCode);
}

void Mesh::start() {
	uint32_t retCode;
//	if (!_isProvisioned) {
//		static const uint8_t static_auth_data[NRF_MESH_KEY_SIZE] = STATIC_AUTH_DATA;
//		mesh_provisionee_start_params_t prov_start_params;
//		prov_start_params.p_static_data    = static_auth_data;
//		prov_start_params.prov_complete_cb = provisioning_complete_cb;
//		prov_start_params.prov_device_identification_start_cb = device_identification_start_cb;
//		prov_start_params.prov_device_identification_stop_cb = NULL;
//		prov_start_params.prov_abort_cb = provisioning_aborted_cb;
////		prov_start_params.p_device_uri = URI_SCHEME_EXAMPLE "URI for LS Client example";
//		prov_start_params.p_device_uri = URI_SCHEME_EXAMPLE "URI for LS Server example";
//		retCode = mesh_provisionee_prov_start(&prov_start_params);
//		APP_ERROR_CHECK(retCode);
//	}

	const uint8_t *uuid = nrf_mesh_configure_device_uuid_get();
	LOGi("Device UUID:");
	BLEutil::printArray(uuid, NRF_MESH_UUID_SIZE);
	retCode = mesh_stack_start();
	APP_ERROR_CHECK(retCode);

	EventDispatcher::getInstance().addListener(this);
}

void Mesh::stop() {
	// TODO: implement
}

void Mesh::provisionSelf(uint16_t id) {
	LOGd("provisionSelf");
	uint32_t retCode;

	State::getInstance().get(CS_TYPE::CONFIG_KEY_ADMIN, _devkey, sizeof(_devkey));
	State::getInstance().get(CS_TYPE::CONFIG_KEY_MEMBER, _appkey, sizeof(_appkey));
	State::getInstance().get(CS_TYPE::CONFIG_KEY_GUEST, _netkey, sizeof(_netkey));

	// Used provisioner_helper.c::prov_helper_provision_self() as example.
	// Also see mesh_stack_provisioning_data_store()
	// And https://devzone.nordicsemi.com/f/nordic-q-a/44515/mesh-node-self-provisioning

	// Store provisioning data in DSM.
	dsm_local_unicast_address_t local_address;
	local_address.address_start = id;
	local_address.count = 1;
	retCode = dsm_local_unicast_addresses_set(&local_address);
	APP_ERROR_CHECK(retCode);

	retCode = dsm_subnet_add(0, _netkey, &_netkeyHandle);
	APP_ERROR_CHECK(retCode);

	retCode = dsm_appkey_add(0, _netkeyHandle, _appkey, &_appkeyHandle);
	APP_ERROR_CHECK(retCode);

	retCode = dsm_devkey_add(id, _netkeyHandle, _devkey, &_devkeyHandle);
	APP_ERROR_CHECK(retCode);

	uint8_t key[NRF_MESH_KEY_SIZE];
	LOGi("netKeyHandle=%u netKey=", _netkeyHandle);
	dsm_subnet_key_get(_netkeyHandle, key);
	BLEutil::printArray(key, NRF_MESH_KEY_SIZE);
	LOGi("appKeyHandle=%u appKey=", _appkeyHandle);
	LOGi("devKeyHandle=%u devKey=", _devkeyHandle);

	retCode = net_state_iv_index_set(0,0);
	APP_ERROR_CHECK(retCode);

    // Bind config server to the device key
	retCode = config_server_bind(_devkeyHandle);
	APP_ERROR_CHECK(retCode);

	access_flash_config_store();
}

void Mesh::provisionLoad() {
	LOGd("provisionLoad");
	// Used provisioner_helper.c::prov_helper_device_handles_load() as example.
	uint32_t retCode;
	dsm_local_unicast_address_t local_addr;
	uint32_t netKeyCount = 1;

	// TODO: why is the handle used as index, and then passed on as handle?
	retCode = dsm_subnet_get_all(&_netkeyHandle, &netKeyCount);
	APP_ERROR_CHECK(retCode);

	if (netKeyCount != 1) {
		APP_ERROR_CHECK(ERR_UNSPECIFIED);
	}

	retCode = dsm_appkey_get_all(_netkeyHandle, &_appkeyHandle, &netKeyCount);
	APP_ERROR_CHECK(retCode);

	dsm_local_unicast_addresses_get(&local_addr);
	retCode = dsm_devkey_handle_get(local_addr.address_start, &_devkeyHandle);
	APP_ERROR_CHECK(retCode);

	LOGi("unicast address=%u", local_addr.address_start);
	uint8_t key[NRF_MESH_KEY_SIZE];
	LOGi("netKeyHandle=%u netKey=", _netkeyHandle);
	dsm_subnet_key_get(_netkeyHandle, key);
	BLEutil::printArray(key, NRF_MESH_KEY_SIZE);
	LOGi("appKeyHandle=%u appKey=", _appkeyHandle);
	LOGi("devKeyHandle=%u devKey=", _devkeyHandle);
}

void Mesh::handleEvent(event_t & event) {
	switch (event.type) {
	case CS_TYPE::EVT_TICK: {
		TYPIFY(EVT_TICK) tickCount = *((TYPIFY(EVT_TICK)*)event.data);
		if (tickCount % (500/TICK_INTERVAL_MS) == 0) {
			if (Stack::getInstance().isScanning()) {
//				Stack::getInstance().stopScanning();
			}
			else {
//				Stack::getInstance().startScanning();
			}
		}
//		if (tickCount % (5000/TICK_INTERVAL_MS) == 0) {
//			_model.sendTestMsg();
//		}
		if (_sendStateTimeCountdown-- == 0) {
			uint8_t rand8;
			RNG::fillBuffer(&rand8, 1);
			uint32_t randMs = MESH_SEND_TIME_INTERVAL_MS + rand8 * MESH_SEND_TIME_INTERVAL_MS_VARIATION / 255;
			_sendStateTimeCountdown = randMs / TICK_INTERVAL_MS;
			cs_mesh_model_msg_time_t packet;
			State::getInstance().get(CS_TYPE::STATE_TIME, &(packet.timestamp), sizeof(packet.timestamp));
			_model.sendTime(&packet);
		}

		_model.tick(tickCount);
		break;
	}
	case CS_TYPE::CMD_SEND_MESH_MSG: {
		TYPIFY(CMD_SEND_MESH_MSG)* msg = (TYPIFY(CMD_SEND_MESH_MSG)*)event.data;
		_model.sendMsg(msg);
		break;
	}
	case CS_TYPE::CMD_SEND_MESH_MSG_KEEP_ALIVE: {
		TYPIFY(CMD_SEND_MESH_MSG_KEEP_ALIVE)* packet = (TYPIFY(CMD_SEND_MESH_MSG_KEEP_ALIVE)*)event.data;
		_model.sendKeepAliveItem(packet);
		break;
	}
	case CS_TYPE::CMD_SEND_MESH_MSG_MULTI_SWITCH: {
		TYPIFY(CMD_SEND_MESH_MSG_MULTI_SWITCH)* packet = (TYPIFY(CMD_SEND_MESH_MSG_MULTI_SWITCH)*)event.data;
		_model.sendMultiSwitchItem(packet);
		break;
	}
	default:
		break;
	}
}
