/**
 * Author: Anne van Rossum
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: 4 Nov., 2014
 * License: LGPLv3+, Apache, and/or MIT, your choice
 */

#include "drivers/cs_PWM.h"

#include <ble/cs_Nordic.h>

//#include "nrf.h"
//#include "nrf_gpiote.h"
//#include "nrf_gpio.h"
//
//#if(NRF51_USE_SOFTDEVICE == 1)
//#include "nrf_sdm.h"
//#endif
//
#include <cfg/cs_Boards.h>
#include <drivers/cs_Serial.h>

#include <events/cs_EventDispatcher.h>

//static uint32_t pwm_max_value, pwm_next_value[PWM_MAX_CHANNELS],
//		pwm_io_ch[PWM_MAX_CHANNELS], pwm_running[PWM_MAX_CHANNELS];
//static uint8_t pwm_gpiote_channel[3];
//static uint32_t pwm_num_channels;

#ifdef SWITCH_INVERSED
#define PWM_GPIOTE_INITIAL_VALUE   NRF_GPIOTE_INITIAL_VALUE_LOW
#define PWM_SWITCH_ON              0
#define PWM_SWITCH_OFF             1
#else
#define PWM_GPIOTE_INITIAL_VALUE   NRF_GPIOTE_INITIAL_VALUE_HIGH
#define PWM_SWITCH_ON              1
#define PWM_SWITCH_OFF             0
#endif

/** GPIO unconfiguration struct for legacy reasons.
 *
 * [25.11.15] merging from NRF SDK 6 to SDK 8. the nrf_gpiote_unconfig function was removed in SDK 8,
 *   and no other function replaces it ... according to nordic developer, can just copy it over from
 *   SDK 6. See post https://devzone.nordicsemi.com/question/50020/what-has-replaced-nrf_gpiote_unconfig/
 */
static inline void nrf_gpiote_unconfig(uint32_t channel_number)
{
   /** Unonfigure the channel as the caller expects */
    NRF_GPIOTE->CONFIG[channel_number] = (GPIOTE_CONFIG_MODE_Disabled   << GPIOTE_CONFIG_MODE_Pos) |
                                         (31UL                          << GPIOTE_CONFIG_PSEL_Pos) |
                                         (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos);
}


int32_t PWM::ppiEnableChannel(uint32_t ch_num, volatile uint32_t *event_ptr, volatile uint32_t *task_ptr) {
	if (ch_num >= 16) return -1;
#if(NRF51_USE_SOFTDEVICE == 1)
	sd_ppi_channel_assign(ch_num, event_ptr, task_ptr);
	sd_ppi_channel_enable_set(1 << ch_num);
#else
	//! Otherwise we configure the channel and return the channel number
	NRF_PPI->CH[ch_num].EEP = (uint32_t)event_ptr;
	NRF_PPI->CH[ch_num].TEP = (uint32_t)task_ptr;
	NRF_PPI->CHENSET = (1 << ch_num);
/** From example:
	 *  //! Enable only PPI channels 0 and 1.
	 *  NRF_PPI->CHEN = (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos) | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos);
	 */

#endif
	return ch_num;
}

uint32_t PWM::init(pwm_config_t *config) {
	if (_initialized) {
		LOGw("Cannot initialize pwm: already initialized");
		return 0xFFFFFFFF;
	}
	if (config->num_channels < 1 || config->num_channels > PWM_MAX_CHANNELS) {
		return 0xFFFFFFFF;
	}

	switch (config->mode) {
		case PWM_MODE_122:     //! 8-bit resolution, 122.07 Hz PWM frequency, 31.25 kHz timer frequency (prescaler 9)
			PWM_TIMER->PRESCALER = 9;
			_maxValue = 255;
			break;
		case PWM_MODE_244:     //! 8-bit resolution, 244.14 Hz PWM frequency, 62.5 kHz timer frequency (prescaler 8)
			PWM_TIMER->PRESCALER = 8;
			_maxValue = 255;
			break;
		case PWM_MODE_976:     //! 8-bit resolution, 976.56 Hz PWM frequency, 250 kHz timer frequency (prescaler 6)
			PWM_TIMER->PRESCALER = 6;
			_maxValue = 255;
			break;
		case PWM_MODE_15625:   //! 8-bit resolution, 15.625 kHz PWM frequency, 4MHz timer frequency (prescaler 2)
			PWM_TIMER->PRESCALER = 2;
			_maxValue = 255;
			break;
		case PWM_MODE_62500:   //! 8-bit resolution, 62.5 kHz PWM frequency, 16MHz timer frequency (prescaler 0)
			PWM_TIMER->PRESCALER = 0;
			_maxValue = 255;
			break;
		default:
			return 0xFFFFFFFF;
	}
	_numChannels = config->num_channels;
	for (uint32_t i = 0; i < _numChannels; ++i) {
		_gpioPin[i] = (uint32_t)config->gpio_pin[i];
		nrf_gpio_cfg_output(_gpioPin[i]);
		_running[i] = 0;
		_gpioteChannel[i] = config->gpiote_channel[i];
		_nextValue[i] = 0;
	}
	PWM_TIMER->TASKS_CLEAR = 1;
	PWM_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;
//	PWM_TIMER->CC[3] = _maxValue*2; //! TODO: why times 2?
	PWM_TIMER->CC[3] = _maxValue;
	PWM_TIMER->MODE = TIMER_MODE_MODE_Timer;
	PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk;
	PWM_TIMER->EVENTS_COMPARE[0] = 0;
	PWM_TIMER->EVENTS_COMPARE[1] = 0;
	PWM_TIMER->EVENTS_COMPARE[2] = 0;
	PWM_TIMER->EVENTS_COMPARE[3] = 0;

	for (uint32_t i = 0; i < _numChannels; ++i) {
		ppiEnableChannel(config->ppi_channel[i*2],  &PWM_TIMER->EVENTS_COMPARE[i],
				&NRF_GPIOTE->TASKS_OUT[_gpioteChannel[i]]);
		ppiEnableChannel(config->ppi_channel[i*2+1],&PWM_TIMER->EVENTS_COMPARE[3],
				&NRF_GPIOTE->TASKS_OUT[_gpioteChannel[i]]);
	}

/** From example:
     * //! @note This example does not go to low power mode therefore constant latency is not needed.
     * //!       However this setting will ensure correct behaviour when routing TIMER events through
     * //!       PPI (shown in this example) and low power mode simultaneously.
     * NRF_POWER->TASKS_CONSTLAT = 1;
	 */

#if(NRF51_USE_SOFTDEVICE == 1)
	sd_nvic_SetPriority(PWM_IRQn, 3);
	sd_nvic_EnableIRQ(PWM_IRQn);
#else
	NVIC_SetPriority(PWM_IRQn, 3);
	NVIC_EnableIRQ(PWM_IRQn);
#endif
	PWM_TIMER->TASKS_START = 1;
	_initialized = true;
	return 0;
}

uint32_t PWM::deinit() {
	//! TODO: actually deinitialize

	//! Stop the timer
	if((PWM_TIMER->INTENSET & TIMER_INTENSET_COMPARE3_Msk) == 0) {
		PWM_TIMER->TASKS_STOP = 1;
		PWM_TIMER->INTENSET = TIMER_INTENSET_COMPARE3_Msk;
	}
	_initialized = false;
	return 0;
}

void PWM::setValue(uint8_t channel, uint32_t value) {
	if (!_initialized) {
		LOGw("Cannot set pwm: not initialized");
		return;
	}
	LOGd("set pwm channel %i to %i", channel, value);
	_nextValue[channel] = value;

	PWM_TIMER->EVENTS_COMPARE[3] = 0;
	PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk | TIMER_SHORTS_COMPARE3_STOP_Msk;

	for (uint32_t i = 0; i < _numChannels; ++i) {
		if(_nextValue[i] == 0) {
			//! todo: DE fix for new SDK
			nrf_gpiote_unconfig(_gpioteChannel[i]);
			nrf_gpio_pin_write(_gpioPin[i], PWM_SWITCH_OFF);
			_running[i] = 0;
		}
		else if (_nextValue[i] >= _maxValue)
		{
			nrf_gpiote_unconfig(_gpioteChannel[i]);
			nrf_gpio_pin_write(_gpioPin[i], PWM_SWITCH_ON);
			_running[i] = 0;
		}
	}

	//! Reset timer
	if((PWM_TIMER->INTENSET & TIMER_INTENSET_COMPARE3_Msk) == 0) {
		PWM_TIMER->TASKS_STOP = 1;
		PWM_TIMER->INTENSET = TIMER_INTENSET_COMPARE3_Msk;
	}
	PWM_TIMER->TASKS_START = 1;
}

void PWM::switchOff() {
	LOGd("switch off all channels");
	if (_initialized) {
		PWM_TIMER->EVENTS_COMPARE[3] = 0;
		PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk | TIMER_SHORTS_COMPARE3_STOP_Msk;

	}

	for (uint32_t i = 0; i < _numChannels; ++i) {
		_nextValue[i] = 0;
		nrf_gpiote_unconfig(_gpioteChannel[i]);
		nrf_gpio_pin_write(_gpioPin[i], PWM_SWITCH_OFF);
		_running[i] = 0;
	}

	if (_initialized) {
		//! Reset timer
		if((PWM_TIMER->INTENSET & TIMER_INTENSET_COMPARE3_Msk) == 0) {
			PWM_TIMER->TASKS_STOP = 1;
			PWM_TIMER->INTENSET = TIMER_INTENSET_COMPARE3_Msk;
		}
		PWM_TIMER->TASKS_START = 1;
	}
}

uint32_t PWM::getValue(uint8_t channel) {
	return _nextValue[channel];
}

extern "C" void PWM_IRQHandler(void) {
	//! Only triggers for compare[3]
	PWM_TIMER->EVENTS_COMPARE[3] = 0;
	PWM_TIMER->INTENCLR = 0xFFFFFFFF;

	static uint32_t i;
	PWM &pwm = PWM::getInstance();
	for (i = 0; i < pwm._numChannels; i++) {
		if ((pwm._nextValue[i] != 0) && (pwm._nextValue[i] < pwm._maxValue)) {
//			PWM_TIMER->CC[i] = pwm._nextValue[i] * 2; //! TODO: why times 2?
			PWM_TIMER->CC[i] = pwm._nextValue[i];
			if (!pwm._running[i]) {
				nrf_gpiote_task_configure(pwm._gpioteChannel[i], pwm._gpioPin[i], NRF_GPIOTE_POLARITY_TOGGLE, PWM_GPIOTE_INITIAL_VALUE);
				pwm._running[i] = 1;
			}
		}
	}

	PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk;
	PWM_TIMER->TASKS_START = 1;
}
