/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Sep 20, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <drivers/cs_RNG.h>
#include <drivers/cs_Serial.h>
#include <events/cs_EventDispatcher.h>
#include <storage/cs_State.h>
#include <uart/cs_UartCommandHandler.h>
#include <uart/cs_UartConnection.h>
#include <uart/cs_UartHandler.h>
#include <util/cs_BleError.h>
#include <util/cs_Utils.h>


#define LOGUartHandlerDebug LOGnone

void handle_msg(void * data, uint16_t size) {
	UartHandler::getInstance().handleMsg((cs_data_t*)data);
}

UartHandler::UartHandler() {

}

void UartHandler::init(serial_enable_t serialEnabled) {
	if (_initialized) {
		return;
	}
	switch (serialEnabled) {
		case SERIAL_ENABLE_NONE:
			return;
		case SERIAL_ENABLE_RX_ONLY:
		case SERIAL_ENABLE_RX_AND_TX:
			// We init both at the same time.
			break;
		default:
			return;
	}
	_initialized = true;
	_readBuffer = new uint8_t[UART_RX_BUFFER_SIZE];
	_writeBuffer = new uint8_t[UART_TX_BUFFER_SIZE];
	_encryptionBuffer = new uint8_t[UART_TX_ENCRYPTION_BUFFER_SIZE];

	State::getInstance().get(CS_TYPE::CONFIG_CROWNSTONE_ID, &_stoneId, sizeof(_stoneId));

	// TODO: get UART key?

	UartConnection::getInstance().init();

	writeMsg(UART_OPCODE_TX_BOOTED, nullptr, 0);

	listen();
}

ret_code_t UartHandler::writeMsg(UartOpcodeTx opCode, uint8_t * data, uint16_t size, UartProtocol::Encrypt encrypt) {

#if CS_UART_BINARY_PROTOCOL_ENABLED == 0
	switch(opCode) {
		// when debugging we would like to drop out of certain binary data coming over the console...
		case UART_OPCODE_TX_TEXT:
			// Now only the special chars get escaped, no header and tail.
			writeBytes(cs_data_t(data, size), false);
			return ERR_SUCCESS;
		case UART_OPCODE_TX_SERVICE_DATA:
			return ERR_SUCCESS;
		case UART_OPCODE_TX_FIRMWARESTATE:
//			writeBytes(data, size);
			return ERR_SUCCESS;
		default:
//			_log(SERIAL_DEBUG, "writeMsg opCode=%u data=", opCode);
//			BLEutil::printArray(data, size);
			return ERR_SUCCESS;
	}
#endif

	ret_code_t retCode;

	retCode = writeMsgStart(opCode, size, encrypt);
	if (retCode != ERR_SUCCESS) {
		return retCode;
	}

	retCode = writeMsgPart(opCode, data, size, encrypt);
	if (retCode != ERR_SUCCESS) {
		return retCode;
	}

	retCode = writeMsgEnd(opCode, encrypt);
	return retCode;
}

ret_code_t UartHandler::writeMsgStart(UartOpcodeTx opCode, uint16_t size, UartProtocol::Encrypt encrypt) {
//	if (size > UART_TX_MAX_PAYLOAD_SIZE) {
//		return;
//	}

	uart_msg_header_t uartMsgHeader;
	uartMsgHeader.deviceId = _stoneId;
	uartMsgHeader.type = opCode;

	uint16_t uartMsgSize = sizeof(uartMsgHeader) + size;

	if (mustEncrypt(encrypt, opCode)) {
		// Write wrapper header
		uint16_t wrapperPayloadSize = getEncryptedBufferSize(uartMsgSize);
		writeWrapperStart(UartMsgType::ENCRYPTED_UART_MSG, wrapperPayloadSize);

		// Set nonce.
		RNG::fillBuffer(_writeNonce.packetNonce, sizeof(_writeNonce.packetNonce));
		cs_ret_code_t retCode = UartConnection::getInstance().getSessionNonceTx(cs_data_t(_writeNonce.sessionNonce, sizeof(_writeNonce.sessionNonce)));
		if (retCode != ERR_SUCCESS) {
			return retCode;
		}

		// Write unencrypted header
		uart_encrypted_msg_header_t msgHeader;
		memcpy(msgHeader.packetNonce, _writeNonce.packetNonce, sizeof(_writeNonce.packetNonce));
		msgHeader.keyId = 0;
		writeBytes(cs_data_t(reinterpret_cast<uint8_t*>(&msgHeader), sizeof(msgHeader)), true);

		return writeEncryptedStart(uartMsgSize);
	}
	else {
		// Write wrapper header
		writeWrapperStart(UartMsgType::UART_MSG, uartMsgSize);

		// Write msg header
		writeBytes(cs_data_t(reinterpret_cast<uint8_t*>(&uartMsgHeader), sizeof(uartMsgHeader)), true);
	}
	return ERR_SUCCESS;
}

ret_code_t UartHandler::writeMsgPart(UartOpcodeTx opCode, uint8_t * data, uint16_t size, UartProtocol::Encrypt encrypt) {
#if CS_UART_BINARY_PROTOCOL_ENABLED == 0
	// when debugging we would like to drop out of certain binary data coming over the console...
	switch(opCode) {
		case UART_OPCODE_TX_TEXT:
			// Now only the special chars get escaped, no header and tail.
			writeBytes(cs_data_t(data, size), false);
			return ERR_SUCCESS;
		default:
			return ERR_SUCCESS;
	}
#endif

	// No logs, this function is called when logging
	if (mustEncrypt(encrypt, opCode)) {
		return writeEncryptedPart(cs_data_t(data, size));
	}
	else {
		writeBytes(cs_data_t(data, size), true);
		return ERR_SUCCESS;
	}
}

ret_code_t UartHandler::writeMsgEnd(UartOpcodeTx opCode, UartProtocol::Encrypt encrypt) {
	// No logs, this function is called when logging
	if (mustEncrypt(encrypt, opCode)) {
		writeEncryptedEnd();
	}

	uart_msg_tail_t tail;
	tail.crc = _crc;
	writeBytes(cs_data_t(reinterpret_cast<uint8_t*>(&tail), sizeof(uart_msg_tail_t)), false);
	return ERR_SUCCESS;
}

cs_ret_code_t UartHandler::writeStartByte() {
	if (!serial_tx_ready()) {
		return ERR_NOT_INITIALIZED;
	}
	writeByte(UART_START_BYTE);
	return ERR_SUCCESS;
}

cs_ret_code_t UartHandler::writeBytes(cs_data_t data, bool updateCrc) {

	if (!serial_tx_ready()) {
		return ERR_NOT_INITIALIZED;
	}

	if (updateCrc) {
		UartProtocol::crc16(data.data, data.len, _crc);
	}

	uint8_t val;
	for(cs_buffer_size_t i = 0; i < data.len; ++i) {
		val = data.data[i];
		// Escape when necessary
		switch (val) {
			case UART_START_BYTE:
			case UART_ESCAPE_BYTE:
				writeByte(UART_ESCAPE_BYTE);
				val ^= UART_ESCAPE_FLIP_MASK;
				break;
		}
		writeByte(val);
	}
	return ERR_SUCCESS;
}

cs_ret_code_t UartHandler::writeWrapperStart(UartMsgType msgType, uint16_t payloadSize) {
	// Set headers.
	uart_msg_size_header_t sizeHeader;
	uart_msg_wrapper_header_t wrapperHeader;
	sizeHeader.size = sizeof(wrapperHeader) + payloadSize + sizeof(uart_msg_tail_t);
	wrapperHeader.type = static_cast<uint8_t>(msgType);

	// Start CRC.
	_crc = UartProtocol::crc16(nullptr, 0);

	// Write to uart.
	writeStartByte();
	writeBytes(cs_data_t(reinterpret_cast<uint8_t*>(&sizeHeader), sizeof(sizeHeader)), false);
	writeBytes(cs_data_t(reinterpret_cast<uint8_t*>(&wrapperHeader), sizeof(wrapperHeader)), true);

	return ERR_SUCCESS;
}



cs_buffer_size_t UartHandler::getEncryptedBufferSize(cs_buffer_size_t uartMsgSize) {
	cs_buffer_size_t encryptedSize = sizeof(uart_encrypted_data_header_t) + uartMsgSize;
	return sizeof(uart_encrypted_msg_header_t) + CS_ROUND_UP_TO_MULTIPLE_OF_POWER_OF_2(encryptedSize, AES_BLOCK_SIZE);
}

bool UartHandler::mustEncrypt(UartProtocol::Encrypt encrypt, UartOpcodeTx opCode) {
	switch (encrypt) {
		case UartProtocol::ENCRYPT_NEVER:
			return false;
		case UartProtocol::ENCRYPT_OR_FAIL:
			return true;
		case UartProtocol::ENCRYPT_ACCORDING_TO_TYPE: {
			if (!UartProtocol::mustBeEncryptedTx(opCode)) {
				return false;
			}
			return UartConnection::getInstance().getSelfStatus().flags.flags.encryptionRequired;
		}
		case UartProtocol::ENCRYPT_IF_ENABLED:
			return UartConnection::getInstance().getSelfStatus().flags.flags.encryptionRequired;
	}
	// Should not be reached.
	return true;
}

cs_ret_code_t UartHandler::writeEncryptedStart(cs_buffer_size_t uartMsgSize) {
	cs_ret_code_t retCode;

	// Always start with empty encryption buffer.
	_encryptionBufferWritten = 0;
	_encryptionBlocksWritten = 0;

	// Write encrypted header.
	uart_encrypted_data_header_t encryptedHeader;
	encryptedHeader.size = uartMsgSize;
	retCode = writeEncryptedPart(cs_data_t(reinterpret_cast<uint8_t*>(&encryptedHeader), sizeof(encryptedHeader)));

	return retCode;
}

cs_ret_code_t UartHandler::writeEncryptedPart(cs_data_t data) {
	cs_ret_code_t retCode;

	// Keep up how much data we read from the input data buffer.
	uint8_t dataSizeRead = 0;

	// TODO: use KeysAndAccess class instead.
	uint8_t key[ENCRYPTION_KEY_LENGTH];
	State::getInstance().get(CS_TYPE::STATE_UART_KEY, key, sizeof(key));

	while (dataSizeRead < data.len) {
		// How much to read from input data and write to the encryption buffer.
		uint8_t writeSize = std::min(data.len - dataSizeRead, AES_BLOCK_SIZE - _encryptionBufferWritten);

		// Append input data to encryption buffer.
		memcpy(_encryptionBuffer + _encryptionBufferWritten, data.data + dataSizeRead, writeSize);

		dataSizeRead += writeSize;
		_encryptionBufferWritten += writeSize;

		// Check if we encryption buffer is full, so we can encrypt a block and write to uart.
		if (_encryptionBufferWritten >= AES_BLOCK_SIZE) {
			retCode = writeEncryptedBlock(cs_data_t(key, sizeof(key)));
			if (retCode != ERR_SUCCESS) {
				return retCode;
			}
		}
	}

	return ERR_SUCCESS;
}

cs_ret_code_t UartHandler::writeEncryptedEnd() {
	// TODO: use KeysAndAccess class instead.
	uint8_t key[ENCRYPTION_KEY_LENGTH];
	State::getInstance().get(CS_TYPE::STATE_UART_KEY, key, sizeof(key));

	if (_encryptionBufferWritten) {
		// Zero pad the remaining bytes.
		memset(_encryptionBuffer + _encryptionBufferWritten, 0, AES_BLOCK_SIZE - _encryptionBufferWritten);
		cs_ret_code_t retCode = writeEncryptedBlock(cs_data_t(key, sizeof(key)));
		if (retCode != ERR_SUCCESS) {
			return retCode;
		}
	}
	return ERR_SUCCESS;
}

cs_ret_code_t UartHandler::writeEncryptedBlock(cs_data_t key) {
	cs_buffer_size_t encryptedSize;
	cs_ret_code_t retCode = AES::getInstance().encryptCtr(
			key,
			cs_data_t(reinterpret_cast<uint8_t*>(&_writeNonce), sizeof(_writeNonce)),
			cs_data_t(),
			cs_data_t(_encryptionBuffer, sizeof(_encryptionBuffer)),
			cs_data_t(_encryptionBuffer, sizeof(_encryptionBuffer)),
			encryptedSize,
			_encryptionBlocksWritten
	);
	if (retCode != ERR_SUCCESS) {
		return retCode;
	}
	writeBytes(cs_data_t(_encryptionBuffer, sizeof(_encryptionBuffer)), true);
	_encryptionBufferWritten = 0;
	++_encryptionBlocksWritten;
	return ERR_SUCCESS;
}




void UartHandler::resetReadBuf() {
	// There are no logs written from this function. It can be called from an interrupt service routine.
	_readBufferIdx = 0;
	_startedReading = false;
	_escapeNextByte = false;
	_sizeToRead = 0;
}

void UartHandler::onRead(uint8_t val) {
	// No logs, this function can be called from interrupt
	// CRC error? Reset.
	// Start char? Reset.
	// Bad escaped value? Reset.
	// Bad length? Reset. Over-run length of buffer? Reset.
	// Haven't seen a start char in too long? Reset anyway.

	// Can't read anything while still processing the previous message.
	if (_readBusy) {
		return;
	}

	if (_readBuffer == nullptr) {
		return;
	}

	// An escape shouldn't be followed by a special byte.
	switch (val) {
		case UART_START_BYTE:
		case UART_ESCAPE_BYTE:
			if (_escapeNextByte) {
				resetReadBuf();
				return;
			}
	}

	if (val == UART_START_BYTE) {
		resetReadBuf();
		_startedReading = true;
		return;
	}

	if (!_startedReading) {
		return;
	}

	if (val == UART_ESCAPE_BYTE) {
		_escapeNextByte = true;
		return;
	}

	if (_escapeNextByte) {
		UartProtocol::unEscape(val);
		_escapeNextByte = false;
	}

	_readBuffer[_readBufferIdx++] = val;

	if (_sizeToRead == 0) {
		if (_readBufferIdx == sizeof(uart_msg_size_header_t)) {
			// Check received size
			uart_msg_size_header_t* sizeHeader = reinterpret_cast<uart_msg_size_header_t*>(_readBuffer);
			if (sizeHeader->size == 0 || sizeHeader->size > UART_RX_BUFFER_SIZE) {
				resetReadBuf();
				return;
			}
			// Set size to read and reset read buffer index.
			_sizeToRead = sizeHeader->size;
			_readBufferIdx = 0;
		}
	}
	else if (_readBufferIdx >= _sizeToRead) {
		// Block reading until the buffer has been handled.
		_readBusy = true;
		// Decouple callback from interrupt handler, and put it on app scheduler instead
		cs_data_t msgData;
		msgData.data = _readBuffer;
		msgData.len = _readBufferIdx;
		uint32_t errorCode = app_sched_event_put(&msgData, sizeof(msgData), handle_msg);
		APP_ERROR_CHECK(errorCode);
	}
}


void UartHandler::handleMsg(cs_data_t* msgData) {
	uint8_t* data = msgData->data;
	uint16_t size = msgData->len;

	handleMsg(data, size);

	// When done, ALWAYS reset and set readBusy to false!
	_readBusy = false;
	resetReadBuf();
}

void UartHandler::handleMsg(uint8_t* data, uint16_t size) {
	LOGUartHandlerDebug("Handle msg size=%u", size);

	// Check size
	uint16_t wrapperSize = sizeof(uart_msg_wrapper_header_t) + sizeof(uart_msg_tail_t);
	if (size < wrapperSize) {
		LOGw(STR_ERR_BUFFER_NOT_LARGE_ENOUGH);
//		LOGw("Wrapper won't fit required=%u size=%u", wrapperSize, size);
		return;
	}

	// Get wrapper header and payload data.
	uart_msg_wrapper_header_t* wrapperHeader = reinterpret_cast<uart_msg_wrapper_header_t*>(data);
	uint8_t* payload = data + sizeof(uart_msg_wrapper_header_t);
	uint16_t payloadSize = size - wrapperSize;

	// Check CRC
	uint16_t calculatedCrc = UartProtocol::crc16(data, size - sizeof(uart_msg_tail_t));
	uart_msg_tail_t* tail = reinterpret_cast<uart_msg_tail_t*>(data + size - sizeof(uart_msg_tail_t));
	if (calculatedCrc != tail->crc) {
		LOGw("CRC mismatch: calculated=%u received=%u", calculatedCrc, tail->crc);
		return;
	}

	switch (static_cast<UartMsgType>(wrapperHeader->type)) {
		case UartMsgType::ENCRYPTED_UART_MSG: {
			LOGd("TODO");
			break;
		}
		case UartMsgType::UART_MSG: {
			handleUartMsg(payload, payloadSize);
			break;
		}
	}
}

void UartHandler::handleUartMsg(uint8_t* data, uint16_t size) {
	if (size < sizeof(uart_msg_header_t)) {
		LOGw(STR_ERR_BUFFER_NOT_LARGE_ENOUGH);
//		LOGw("Header won't fit required=%u size=%u", sizeof(uart_msg_header_t), payloadSize);
		return;
	}
	uart_msg_header_t* header = reinterpret_cast<uart_msg_header_t*>(data);
	uint16_t msgDataSize = size - sizeof(uart_msg_header_t);
	uint8_t* msgData = data + sizeof(uart_msg_header_t);

	_commandHandler.handleCommand(
			static_cast<UartOpcodeRx>(header->type),
			cs_data_t(msgData, msgDataSize),
			cmd_source_with_counter_t(cmd_source_t(CS_CMD_SOURCE_TYPE_UART, header->deviceId)),
			EncryptionAccessLevel::ADMIN,
			cs_data_t(_writeBuffer, (_writeBuffer == nullptr) ? 0 : UART_TX_BUFFER_SIZE)
			);
}

void UartHandler::handleEvent(event_t & event) {
	switch (event.type) {
		case CS_TYPE::CONFIG_UART_ENABLED: {
			TYPIFY(CONFIG_UART_ENABLED)* enabled = (TYPIFY(CONFIG_UART_ENABLED)*)event.data;
			serial_enable(*reinterpret_cast<serial_enable_t*>(enabled));
			break;
		}
		case CS_TYPE::EVT_STATE_EXTERNAL_STONE: {
			TYPIFY(EVT_STATE_EXTERNAL_STONE)* state = (TYPIFY(EVT_STATE_EXTERNAL_STONE)*)event.data;
			writeMsg(UART_OPCODE_TX_MESH_STATE, reinterpret_cast<uint8_t*>(&state->data), sizeof(state->data));
			break;
		}
		case CS_TYPE::EVT_MESH_EXT_STATE_0: {
			TYPIFY(EVT_MESH_EXT_STATE_0)* state = (TYPIFY(EVT_MESH_EXT_STATE_0)*)event.data;
			writeMsg(UART_OPCODE_TX_MESH_STATE_PART_0, reinterpret_cast<uint8_t*>(state), sizeof(*state));
			break;
		}
		case CS_TYPE::EVT_MESH_EXT_STATE_1: {
			TYPIFY(EVT_MESH_EXT_STATE_1)* state = (TYPIFY(EVT_MESH_EXT_STATE_1)*)event.data;
			writeMsg(UART_OPCODE_TX_MESH_STATE_PART_1, reinterpret_cast<uint8_t*>(state), sizeof(*state));
			break;
		}
		case CS_TYPE::EVT_PRESENCE_CHANGE: {
			TYPIFY(EVT_PRESENCE_CHANGE)* state = (TYPIFY(EVT_PRESENCE_CHANGE)*)event.data;
			writeMsg(UART_OPCODE_TX_PRESENCE_CHANGE, reinterpret_cast<uint8_t*>(state), sizeof(*state));
			break;
		}
		default:
			break;
	}
}
