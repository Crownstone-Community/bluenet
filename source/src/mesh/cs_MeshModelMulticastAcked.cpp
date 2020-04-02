/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Apr 1, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <mesh/cs_MeshCommon.h>
#include <mesh/cs_MeshModelMulticastAcked.h>
#include <mesh/cs_MeshUtil.h>
#include <protocol/mesh/cs_MeshModelPacketHelper.h>
#include <util/cs_BleError.h>
#include <util/cs_Utils.h>

extern "C" {
#include <access.h>
#include <access_config.h>
//#include <nrf_mesh.h>
#include <log.h>
}

static void staticMshHandler(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args) {
	MeshModelMulticastAcked* meshModel = (MeshModelMulticastAcked*)p_args;
	meshModel->handleMsg(p_message);
}

static const access_opcode_handler_t opcodeHandlers[] = {
		{ACCESS_OPCODE_VENDOR(CS_MESH_MODEL_OPCODE_MULTICAST_RELIABLE_MSG, CROWNSTONE_COMPANY_ID), staticMshHandler},
		{ACCESS_OPCODE_VENDOR(CS_MESH_MODEL_OPCODE_MULTICAST_REPLY, CROWNSTONE_COMPANY_ID), staticMshHandler},
};

void MeshModelMulticastAcked::registerMsgHandler(const callback_msg_t& closure) {
	_msgCallback = closure;
}

void MeshModelMulticastAcked::init(uint16_t modelId) {
	assert(_msgCallback != nullptr, "Callback not set");
	uint32_t retVal;
	access_model_add_params_t accessParams;
	accessParams.model_id.company_id = CROWNSTONE_COMPANY_ID;
	accessParams.model_id.model_id = modelId;
	accessParams.element_index = 0;
	accessParams.p_opcode_handlers = opcodeHandlers;
	accessParams.opcode_count = (sizeof(opcodeHandlers) / sizeof((opcodeHandlers)[0]));
	accessParams.p_args = this;
	accessParams.publish_timeout_cb = NULL;
	retVal = access_model_add(&accessParams, &_accessModelHandle);
	APP_ERROR_CHECK(retVal);
	retVal = access_model_subscription_list_alloc(_accessModelHandle);
	APP_ERROR_CHECK(retVal);
}

void MeshModelMulticastAcked::configureSelf(dsm_handle_t appkeyHandle) {
	uint32_t retCode;
	retCode = dsm_address_publish_add(MESH_MODEL_GROUP_ADDRESS_ACKED, &_groupAddressHandle);
	APP_ERROR_CHECK(retCode);
	retCode = dsm_address_subscription_add_handle(_groupAddressHandle);
	APP_ERROR_CHECK(retCode);

	retCode = access_model_application_bind(_accessModelHandle, appkeyHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_publish_application_set(_accessModelHandle, appkeyHandle);
	APP_ERROR_CHECK(retCode);

	retCode = access_model_publish_address_set(_accessModelHandle, _groupAddressHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_subscription_add(_accessModelHandle, _groupAddressHandle);
	APP_ERROR_CHECK(retCode);
}

void MeshModelMulticastAcked::handleMsg(const access_message_rx_t * accessMsg) {
	if (accessMsg->meta_data.p_core_metadata->source != NRF_MESH_RX_SOURCE_LOOPBACK) {
		LOGMeshModelVerbose("Handle mesh msg. opcode=%u appkey=%u subnet=%u ttl=%u rssi=%i",
				accessMsg->opcode.opcode,
				accessMsg->meta_data.appkey_handle,
				accessMsg->meta_data.subnet_handle,
				accessMsg->meta_data.ttl,
				MeshUtil::getRssi(accessMsg->meta_data.p_core_metadata)
		);
		MeshUtil::printMeshAddress("  Src: ", &(accessMsg->meta_data.src));
		MeshUtil::printMeshAddress("  Dst: ", &(accessMsg->meta_data.dst));
//		LOGMeshModelVerbose("ownAddress=%u  Data:", _ownAddress);
#if CS_SERIAL_NRF_LOG_ENABLED == 1
		__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Handle mesh msg. opcode=%u appkey=%u subnet=%u ttl=%u rssi=%i\n",
				accessMsg->opcode.opcode,
				accessMsg->meta_data.appkey_handle,
				accessMsg->meta_data.subnet_handle,
				accessMsg->meta_data.ttl,
				MeshUtil::getRssi(accessMsg->meta_data.p_core_metadata)
		);
	}
	else {
		__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Handle mesh msg loopback\n");
#endif
	}
//	bool ownMsg = (_ownAddress == accessMsg->meta_data.src.value) && (accessMsg->meta_data.src.type == NRF_MESH_ADDRESS_TYPE_UNICAST);
	bool ownMsg = accessMsg->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_LOOPBACK;
	if (ownMsg) {
		return;
	}

	MeshUtil::cs_mesh_received_msg_t msg;
	msg.opCode = accessMsg->opcode.opcode;
	msg.srcAddress = accessMsg->meta_data.src.value;
	msg.msg = (uint8_t*)(accessMsg->p_data);
	msg.msgSize = accessMsg->length;
	msg.rssi = MeshUtil::getRssi(accessMsg->meta_data.p_core_metadata);
	msg.hops = ACCESS_DEFAULT_TTL - accessMsg->meta_data.ttl;

	switch (msg.opCode) {
		case CS_MESH_MODEL_OPCODE_MULTICAST_REPLY: {
			handleReply(msg);
			return;
		}
		case CS_MESH_MODEL_OPCODE_MULTICAST_RELIABLE_MSG: {
			// Prepare a reply message, to send the result back.
			uint8_t replyMsg[MAX_MESH_MSG_NON_SEGMENTED_SIZE];
			replyMsg[0] = CS_MESH_MODEL_TYPE_RESULT;

			cs_mesh_model_msg_result_header_t* resultHeader = (cs_mesh_model_msg_result_header_t*) (replyMsg + MESH_HEADER_SIZE);

			uint8_t headerSize = MESH_HEADER_SIZE + sizeof(cs_mesh_model_msg_result_header_t);
			cs_data_t resultData(replyMsg + headerSize, sizeof(replyMsg) - headerSize);
			cs_result_t result(resultData);

			// Handle the message, get the result.
			_msgCallback(msg, result);

			// Send the result as reply.
			resultHeader->msgType = (msg.msgSize >= MESH_HEADER_SIZE) ? MeshUtil::getType(msg.msg) : CS_MESH_MODEL_TYPE_UNKNOWN;
			resultHeader->retCode = MeshUtil::getShortenedRetCode(result.returnCode);
			stone_id_t srcId = msg.srcAddress;
			sendReply(srcId, replyMsg, headerSize + result.dataSize);
			break;
		}
		default:
			return;
	}
}

cs_ret_code_t MeshModelMulticastAcked::sendMsg(const uint8_t* data, uint16_t len) {
	setPublishAddress(0);

	access_message_tx_t accessMsg;
	accessMsg.opcode.company_id = CROWNSTONE_COMPANY_ID;
	accessMsg.opcode.opcode = CS_MESH_MODEL_OPCODE_MULTICAST_RELIABLE_MSG;
	accessMsg.p_buffer = data;
	accessMsg.length = len;
	accessMsg.force_segmented = false;
	accessMsg.transmic_size = NRF_MESH_TRANSMIC_SIZE_SMALL;
	accessMsg.access_token = nrf_mesh_unique_token_get();

	uint32_t status = NRF_SUCCESS;
	status = access_model_publish(_accessModelHandle, &accessMsg);
	if (status != NRF_SUCCESS) {
		LOGMeshModelInfo("sendMsg failed: %u", status);
	}
	return status;
}

void MeshModelMulticastAcked::sendReply(stone_id_t targetId, const uint8_t* data, uint16_t len) {
	if (setPublishAddress(targetId) != NRF_SUCCESS) {
		return;
	}

	access_message_tx_t accessMsg;
	accessMsg.opcode.company_id = CROWNSTONE_COMPANY_ID;
	accessMsg.opcode.opcode = CS_MESH_MODEL_OPCODE_MULTICAST_REPLY;
	accessMsg.p_buffer = data;
	accessMsg.length = len;
	accessMsg.force_segmented = false;
	accessMsg.transmic_size = NRF_MESH_TRANSMIC_SIZE_SMALL;

	uint32_t status = NRF_SUCCESS;
	for (int i = 0; i < MESH_MODEL_ACK_TRANSMISSIONS; ++i) {
		accessMsg.access_token = nrf_mesh_unique_token_get();
		status = access_model_publish(_accessModelHandle, &accessMsg);
		if (status != NRF_SUCCESS) {
			LOGMeshModelInfo("sendReply failed: %u", status);
			break;
		}
	}
	LOGMeshModelDebug("Sent reply to id=%u", targetId);
}

void MeshModelMulticastAcked::handleReply(MeshUtil::cs_mesh_received_msg_t & msg) {
	if (_queueIndexInProgress == queue_index_none) {
		return;
	}

	// Handle reply message.
	cs_result_t result;
	_msgCallback(msg, result);
	if (result.returnCode != ERR_SUCCESS || MeshUtil::getType(msg.msg) != CS_MESH_MODEL_TYPE_RESULT) {
		LOGw("Invalid reply:");
		BLEutil::printArray(msg.msg, msg.msgSize);
		return;
	}

	stone_id_t srcId = msg.srcAddress;
//	cs_mesh_model_msg_result_header_t* resultHeader = (cs_mesh_model_msg_result_header_t*) (msg + MESH_HEADER_SIZE);
//	LOGMeshModelDebug("Received ack from id=%u type=%u retCode=%u", srcId, resultHeader->msgType, resultHeader->retCode);

	// Mark id as acked.
	auto item = _queue[_queueIndexInProgress];
	for (uint8_t i = 0; i < item.numIds; ++i) {
		if (item.stoneIdsPtr[i] == srcId) {
			LOGMeshModelVerbose("Set acked bit %u", i);
			_ackedStonesBitmask.setBit(i);
			break;
		}
	}
}

cs_ret_code_t MeshModelMulticastAcked::setPublishAddress(stone_id_t id) {
	uint32_t retCode;

	// First clean up the previous unicast address.
	dsm_address_publish_remove(_unicastAddressHandle);
	_unicastAddressHandle = DSM_HANDLE_INVALID;

	if (id == 0) {
		// Set group address.
		retCode = access_model_publish_address_set(_accessModelHandle, _groupAddressHandle);
		assert(retCode == NRF_SUCCESS, "failed to set publish address");
		if (retCode != NRF_SUCCESS) {
			return ERR_UNSPECIFIED;
		}
	}
	else {
		// Add and set unicast address.
		// All addresses with first 2 bits 0, are unicast addresses.
		uint16_t address = id;
		retCode = dsm_address_publish_add(address, &_unicastAddressHandle);
		assert(retCode == NRF_SUCCESS, "failed to add publish address");
		if (retCode != NRF_SUCCESS) {
			return ERR_UNSPECIFIED;
		}
		retCode = access_model_publish_address_set(_accessModelHandle, _unicastAddressHandle);
		assert(retCode == NRF_SUCCESS, "failed to set publish address");
		if (retCode != NRF_SUCCESS) {
			return ERR_UNSPECIFIED;
		}
	}
	return ERR_SUCCESS;
}

cs_ret_code_t MeshModelMulticastAcked::addToQueue(MeshUtil::cs_mesh_queue_item_t& item) {
	MeshUtil::printQueueItem("MulticastAcked addToQueue", item.metaData);
#if MESH_MODEL_TEST_MSG != 0
	if (item.metaData.type != CS_MESH_MODEL_TYPE_TEST) {
		return ERR_SUCCESS;
	}
#endif
	size16_t msgSize = MeshUtil::getMeshMessageSize(item.msgPayload.len);
	assert(item.msgPayload.data != nullptr || item.msgPayload.len == 0, "Null pointer");
	assert(msgSize != 0, "Empty message");
	assert(msgSize <= MAX_MESH_MSG_NON_SEGMENTED_SIZE, "Message too large"); // Only unsegmented for now.
	assert(item.broadcast == true, "Multicast only");
	assert(item.reliable == true, "Reliable only");

	// Find an empty spot in the queue (transmissions == 0).
	// Start looking at _queueIndexNext, then reverse iterate over the queue.
	// Then set the new _queueIndexNext at the newly added item, so that it will be sent next.
	// We do the reverse iterate, so that the chance is higher that
	// the old _queueIndexNext will be sent quickly after this newly added item.
	uint8_t index;
	for (int i = _queueIndexNext; i < _queueIndexNext + queue_size; ++i) {
		index = i % queue_size;
		cs_multicast_acked_queue_item_t* it = &(_queue[index]);
		if (it->metaData.transmissionsOrTimeout == 0) {

			// Allocate and copy msg.
			it->msgPtr = (uint8_t*)malloc(msgSize);
			LOGMeshModelVerbose("msg alloc %p size=%u", it->msgPtr, msgSize);
			if (it->msgPtr == NULL) {
				return ERR_NO_SPACE;
			}
			if (!MeshUtil::setMeshMessage((cs_mesh_model_msg_type_t)item.metaData.type, item.msgPayload.data, item.msgPayload.len, it->msgPtr, msgSize)) {
				LOGMeshModelVerbose("msg free %p", it->msgPtr);
				free(it->msgPtr);
				return ERR_WRONG_PAYLOAD_LENGTH;
			}

			// Allocate and copy stone ids.
			it->stoneIdsPtr = (stone_id_t*)malloc(item.numIds * sizeof(stone_id_t));
			LOGMeshModelVerbose("ids alloc %p size=%u", it->stoneIdsPtr, item.numIds * sizeof(stone_id_t));
			if (it->stoneIdsPtr == NULL) {
				LOGMeshModelVerbose("msg free %p", it->msgPtr);
				free(it->msgPtr);
				return ERR_NO_SPACE;
			}
			memcpy(it->stoneIdsPtr, item.stoneIdsPtr, item.numIds * sizeof(stone_id_t));

			// Copy meta data.
			memcpy(&(it->metaData), &(item.metaData), sizeof(item.metaData));
			it->numIds = item.numIds;
			it->msgSize = msgSize;

			LOGMeshModelVerbose("added to ind=%u", index);
			BLEutil::printArray(it->msgPtr, it->msgSize);

			// If queue was empty, we can start sending this item.
			sendMsgFromQueue();
			return ERR_SUCCESS;
		}
	}
	LOGw("queue is full");
	return ERR_BUSY;
}

cs_ret_code_t MeshModelMulticastAcked::remFromQueue(cs_mesh_model_msg_type_t type, uint16_t id) {
	cs_ret_code_t retCode = ERR_NOT_FOUND;
	for (int i = 0; i < queue_size; ++i) {
		if (_queue[i].metaData.id == id && _queue[i].metaData.type == type && _queue[i].metaData.transmissionsOrTimeout != 0) {
			cancelQueueItem(i);
			remQueueItem(i);
			retCode = ERR_SUCCESS;
		}
	}
	return retCode;
}

void MeshModelMulticastAcked::cancelQueueItem(uint8_t index) {
	if (_queueIndexInProgress == index) {
		LOGe("TODO: Cancel progress");
		_queueIndexInProgress = queue_index_none;
	}
}

void MeshModelMulticastAcked::remQueueItem(uint8_t index) {
	_queue[index].metaData.transmissionsOrTimeout = 0;
	LOGMeshModelVerbose("msg free %p", _queue[index].msgPtr);
	free(_queue[index].msgPtr);
	LOGMeshModelVerbose("ids free %p", _queue[index].stoneIdsPtr);
	free(_queue[index].stoneIdsPtr);
	_ackedStonesBitmask.setNumBits(0);
	LOGMeshModelVerbose("removed from queue: ind=%u", index);
}

int MeshModelMulticastAcked::getNextItemInQueue(bool priority) {
	int index;
	for (int i = _queueIndexNext; i < _queueIndexNext + queue_size; i++) {
		index = i % queue_size;
		if ((!priority || _queue[index].metaData.priority) && _queue[index].metaData.transmissionsOrTimeout > 0) {
			return index;
		}
	}
	return -1;
}

bool MeshModelMulticastAcked::sendMsgFromQueue() {
	if (_queueIndexInProgress != queue_index_none) {
		return false;
	}
	int index = getNextItemInQueue(true);
	if (index == -1) {
		index = getNextItemInQueue(false);
	}
	if (index == -1) {
		return false;
	}

	cs_multicast_acked_queue_item_t* item = &(_queue[index]);
	if (!prepareForMsg(item)) {
		return false;
	}

	cs_ret_code_t retCode = sendMsg(item->msgPtr, item->msgSize);
	if (retCode != ERR_SUCCESS) {
		return false;
	}
	_queueIndexInProgress = index;
	LOGMeshModelInfo("sent ind=%u timeout=%u type=%u id=%u", index, item->metaData.transmissionsOrTimeout, item->metaData.type, item->metaData.id);

	// Next item will be sent next, so that items are sent interleaved.
	_queueIndexNext = (index + 1) % queue_size;
	return true;
}

bool MeshModelMulticastAcked::prepareForMsg(cs_multicast_acked_queue_item_t* item) {
	_processCallsLeft = item->metaData.transmissionsOrTimeout * 1000 / MESH_MODEL_ACKED_RETRY_INTERVAL_MS;
	return _ackedStonesBitmask.setNumBits(item->numIds);
}

void MeshModelMulticastAcked::checkDone() {
	if (_queueIndexInProgress == queue_index_none) {
		return;
	}
	auto item = _queue[_queueIndexInProgress];

	// Check acks.
	if (_ackedStonesBitmask.isAllBitsSet()) {
		LOGi("Received ack from all stones.");
		MeshUtil::printQueueItem(" ", item.metaData);

		remQueueItem(_queueIndexInProgress);
		_queueIndexInProgress = queue_index_none;
	}

	// Check for timeout.
	if (_processCallsLeft == 0) {
		LOGi("Timeout.")
		MeshUtil::printQueueItem(" ", item.metaData);

		remQueueItem(_queueIndexInProgress);
		_queueIndexInProgress = queue_index_none;
	}
	else {
		--_processCallsLeft;
	}
}

void MeshModelMulticastAcked::retryMsg() {
	if (_queueIndexInProgress == queue_index_none) {
		return;
	}
	auto item = _queue[_queueIndexInProgress];
	sendMsg(item.msgPtr, item.msgSize);
}

void MeshModelMulticastAcked::processQueue() {
	checkDone();
	retryMsg();
	sendMsgFromQueue();
}

void MeshModelMulticastAcked::tick(uint32_t tickCount) {
	// Process only at retry interval.
	if (tickCount % (MESH_MODEL_ACKED_RETRY_INTERVAL_MS / TICK_INTERVAL_MS) == 0) {
		processQueue();
	}
}
