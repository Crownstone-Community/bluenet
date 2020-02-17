/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 29, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <ble/cs_Nordic.h>
#include <drivers/cs_Dimmer.h>
#include <drivers/cs_PWM.h>
#include <drivers/cs_Serial.h>
#include <storage/cs_State.h>
#include <util/cs_Error.h>

#include <test/cs_Test.h>

void Dimmer::init(const boards_config_t& board) {
	initialized = true;

	hardwareBoard = board.hardwareBoard;
	pinEnableDimmer = board.pinGpioEnablePwm;

	nrf_gpio_cfg_output(pinEnableDimmer);
	nrf_gpio_pin_clear(pinEnableDimmer);

	TYPIFY(CONFIG_PWM_PERIOD) pwmPeriodUs;
	State::getInstance().get(CS_TYPE::CONFIG_PWM_PERIOD, &pwmPeriodUs, sizeof(pwmPeriodUs));

	pwm_config_t pwmConfig;
	pwmConfig.channelCount = 1;
	pwmConfig.period_us = pwmPeriodUs;
	pwmConfig.channels[0].pin = board.pinGpioPwm;
	pwmConfig.channels[0].inverted = board.flags.pwmInverted;

	LOGd("init enablePin=%u pwmPin=%u inverted=%u period=%u µs", board.pinGpioEnablePwm, board.pinGpioPwm, board.flags.pwmInverted, pwmPeriodUs);

	PWM::getInstance().init(pwmConfig);
}

void Dimmer::start() {
	LOGd("start");
	assert(initialized == true, "Not initialized");
	if (started) {
		return;
	}
	started = true;

	enable();

	TYPIFY(CONFIG_START_DIMMER_ON_ZERO_CROSSING) startDimmerOnZeroCrossing;
	State::getInstance().get(CS_TYPE::CONFIG_START_DIMMER_ON_ZERO_CROSSING, &startDimmerOnZeroCrossing, sizeof(startDimmerOnZeroCrossing));

	switch (hardwareBoard) {
		case PCA10036:
		case PCA10040:
			// These dev boards don't have power measurement, so no zero crossing.
			PWM::getInstance().start(false);
			break;
		default:
			PWM::getInstance().start(startDimmerOnZeroCrossing);
			break;
	}
}

bool Dimmer::set(uint8_t intensity) {
	LOGd("set %u", intensity);
	assert(initialized == true, "Not initialized");
	if (!enabled && intensity > 0) {
		LOGd("Dimmer not enabled");
		return false;
	}

	TEST_PUSH_EXPR_D(this,"intensity", intensity);
	PWM::getInstance().setValue(0, intensity);
	
	return true;
}

void Dimmer::enable() {
	LOGd("enable");
	switch (hardwareBoard) {
		// Dev boards
		case PCA10036:
		case PCA10040:
		// Builtin zero
		case ACR01B1A:
		case ACR01B1B:
		case ACR01B1C:
		case ACR01B1D:
		case ACR01B1E:
		// First builtin one
		case ACR01B10B:
		// Plugs
		case ACR01B2A:
		case ACR01B2B:
		case ACR01B2C:
		case ACR01B2E:
		case ACR01B2G:
		// These don't have a dimmer.
		case GUIDESTONE:
		case CS_USB_DONGLE: {
			break;
		}
		// Newer ones have a dimmer enable pin.
		case ACR01B10C:
		default: {
			nrf_gpio_pin_set(pinEnableDimmer);
			break;
		}
	}
	enabled = true;
}
