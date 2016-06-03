/**
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Oct 21, 2014
 * License: LGPLv3+, Apache License, or MIT, your choice
 */

#include <services/cs_PowerService.h>

#include <cfg/cs_UuidConfig.h>
#include <drivers/cs_Serial.h>
#include <drivers/cs_Timer.h>
#include <processing/cs_CommandHandler.h>
#include <processing/cs_PowerSampling.h>
#include <structs/buffer/cs_MasterBuffer.h>

using namespace BLEpp;

PowerService::PowerService() : EventListener(),
		_pwmCharacteristic(NULL),
		_relayCharacteristic(NULL),
		_powerConsumptionCharacteristic(NULL),
		_powerSamplesCharacteristic(NULL)
{
	EventDispatcher::getInstance().addListener(this);

	setUUID(UUID(POWER_UUID));

	setName(BLE_SERVICE_POWER);

	addCharacteristics();

	// todo: enable again if tick is needed
//	Timer::getInstance().createSingleShot(_appTimerId, (app_timer_timeout_handler_t)PowerService::staticTick);
}

void PowerService::addCharacteristics() {
	LOGi("Create power service");

#if CHAR_PWM==1
	LOGi("add PWM Characteristic");
	addPWMCharacteristic();
#else
	LOGi("skip PWM Characteristic");
#endif

#if CHAR_RELAY==1
	LOGi("add Relay Characteristic");
	addRelayCharacteristic();
#else
	LOGi("skip Relay Characteristic");
#endif

#if CHAR_SAMPLE_CURRENT==1
	LOGi("add Power sample characteristic");
	addPowerSamplesCharacteristic();
	LOGi("add Current Consumption Characteristic");
	addPowerConsumptionCharacteristic();
#else
	LOGi("skip Sample Current Characteristic");
	LOGi("skip Current Curve Characteristic");
	LOGi("skip Current Consumption Characteristic");
#endif

	addCharacteristicsDone();
}

//void PowerService::tick() {
//	scheduleNextTick();
//}

//void PowerService::scheduleNextTick() {
//	Timer::getInstance().start(_appTimerId, HZ_TO_TICKS(POWER_SERVICE_UPDATE_FREQUENCY), this);
//}

void PowerService::addPWMCharacteristic() {
	_pwmCharacteristic = new Characteristic<uint8_t>();
	addCharacteristic(_pwmCharacteristic);
	_pwmCharacteristic->setUUID(UUID(getUUID(), PWM_UUID));
	_pwmCharacteristic->setName(BLE_CHAR_PWM);
	_pwmCharacteristic->setDefaultValue(255);
	_pwmCharacteristic->setWritable(true);
	_pwmCharacteristic->onWrite([&](const uint8_t& value) -> void {
		CommandHandler::getInstance().handleCommand(CMD_PWM, (buffer_ptr_t)&value, 4);
	});
}

void PowerService::addRelayCharacteristic() {

	_relayCharacteristic = new Characteristic<uint8_t>();
	addCharacteristic(_relayCharacteristic);
	_relayCharacteristic->setUUID(UUID(getUUID(), RELAY_UUID));
	_relayCharacteristic->setName(BLE_CHAR_RELAY);
	_relayCharacteristic->setDefaultValue(255);
	_relayCharacteristic->setWritable(true);
	_relayCharacteristic->onWrite([&](const uint8_t& value) -> void {
		CommandHandler::getInstance().handleCommand(CMD_SWITCH, (buffer_ptr_t)&value, 4);
	});
}

void PowerService::addPowerSamplesCharacteristic() {
	_powerSamplesCharacteristic = new Characteristic<uint8_t*>();
	addCharacteristic(_powerSamplesCharacteristic);
	_powerSamplesCharacteristic->setUUID(UUID(getUUID(), POWER_SAMPLES_UUID));
	_powerSamplesCharacteristic->setName("Power Curve");
	_powerSamplesCharacteristic->setWritable(false);
	_powerSamplesCharacteristic->setNotifies(true);

	buffer_ptr_t buffer = NULL;
	uint16_t size = 0;
	PowerSampling::getInstance().getBuffer(buffer, size);
	LOGd("buffer=%u", buffer);

	_powerSamplesCharacteristic->setValue(buffer);
	_powerSamplesCharacteristic->setMaxLength(size);
	_powerSamplesCharacteristic->setDataLength(size);
}

void PowerService::addPowerConsumptionCharacteristic() {
	_powerConsumptionCharacteristic = new Characteristic<uint16_t>();
	addCharacteristic(_powerConsumptionCharacteristic);
	_powerConsumptionCharacteristic->setUUID(UUID(getUUID(), CURRENT_CONSUMPTION_UUID));
	_powerConsumptionCharacteristic->setName("Power Consumption");
	_powerConsumptionCharacteristic->setDefaultValue(0);
	_powerConsumptionCharacteristic->setWritable(false);
	_powerConsumptionCharacteristic->setNotifies(true);
}

void PowerService::handleEvent(uint16_t evt, void* p_data, uint16_t length) {
	switch(evt) {
	case EVT_POWER_OFF:
	case EVT_POWER_ON: {
		if (_pwmCharacteristic) {
			LOGi("pwm update");
			(*_pwmCharacteristic) = *(uint8_t*)p_data;
		}
		break;
	}
	case EVT_POWER_SAMPLES: {
		if (_powerSamplesCharacteristic) {
//			LOGd("power samples update");
			_powerSamplesCharacteristic->setDataLength(length);
			_powerSamplesCharacteristic->notify();
		}
		break;
	}
	case EVT_POWER_CONSUMPTION: {
		if (_powerConsumptionCharacteristic) {
			LOGi("power consumption update");
			(*_powerConsumptionCharacteristic) = *(uint32_t*)p_data;
		}
	}
	}
}


