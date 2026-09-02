/**
 * @file touch_mcu.c
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 * @brief
 * @version 0.1
 * @date 2026-09-02
 *
 * @copyright Copyright (c) 2026
 *
 */

/*==================[inclusions]=============================================*/
#include "touch_mcu.h"
#include <stdint.h>
#include "driver/touch_sensor.h"
/*==================[macros and definitions]=================================*/

/*==================[internal data declaration]==============================*/
static bool touch_initialized = false;
/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/*==================[external functions definition]==========================*/
bool TouchInit(touch_ch_t channel, uint32_t threshold){
	if(!touch_initialized){
		if(touch_pad_init() != ESP_OK){
			return false;
		}
		touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
		touch_initialized = true;
	}
	if(touch_pad_config((touch_pad_t)channel) != ESP_OK){
		return false;
	}
	touch_pad_set_thresh((touch_pad_t)channel, threshold);
	touch_pad_fsm_start();
	return true;
}

uint32_t TouchReadRaw(touch_ch_t channel){
	uint32_t raw = 0;
	touch_pad_read_raw_data((touch_pad_t)channel, &raw);
	return raw;
}

bool TouchDetect(touch_ch_t channel){
	uint32_t status = touch_pad_get_status();
	return (status & (1UL << channel)) != 0;
}

void TouchActivInt(void *ptr_int_func, void *args){
	touch_pad_isr_register((intr_handler_t)ptr_int_func, args, TOUCH_PAD_INTR_MASK_ACTIVE);
	touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE);
}

bool TouchShieldEnable(touch_ch_t guard_channel, uint8_t shield_level){
	if(!touch_initialized){
		return false;
	}
	touch_pad_waterproof_t waterproof = {
		.guard_ring_pad = (touch_pad_t)guard_channel,
		.shield_driver = (touch_pad_shield_driver_t)shield_level,
	};
	if(touch_pad_waterproof_set_config(&waterproof) != ESP_OK){
		return false;
	}
	return touch_pad_waterproof_enable() == ESP_OK;
}

void TouchShieldDisable(void){
	touch_pad_waterproof_disable();
}

void TouchDeinit(void){
	touch_pad_fsm_stop();
	touch_pad_deinit();
	touch_initialized = false;
}

/*==================[end of file]============================================*/
