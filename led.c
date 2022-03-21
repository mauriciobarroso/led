/**
  ******************************************************************************
  * @file           : led.c
  * @author         : Mauricio Barroso Benavides
  * @date           : Mar 21, 2022
  * @brief          : This file provides code for the configuration and control
  *                   LEDs instances
  ******************************************************************************
  * @attention
  *
  * MIT License
  *
  * Copyright (c) 2022 Mauricio Barroso Benavides
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to
  * deal in the Software without restriction, including without limitation the
  * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  * sell copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  * IN THE SOFTWARE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "led.h"
#include "esp_log.h"

/* External variables --------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define LED_MAX_NUM		(8)
#define LED_TIMER_FREQ	(5000)
#define LED_TIMER		LEDC_TIMER_1
#define LED_SPEED_MODE	LEDC_LOW_SPEED_MODE

/* Private macro -------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static bool IRAM_ATTR fade_end_cb(const ledc_cb_param_t * param, void * arg);

/* Private variables ---------------------------------------------------------*/
static const char * TAG = "led";

static bool is_installed = false;
static uint8_t leds_num = 0;

/* Exported functions --------------------------------------------------------*/
esp_err_t led_init(led_t * const me, gpio_num_t gpio, led_mode_e mode, uint32_t time, uint8_t intensity) {
	ESP_LOGI(TAG, "Initializing led component...");

	/* Error code variable */
	esp_err_t ret;

	/* Check if the maximum number of LEDs was initialized */
	if(leds_num >= LED_MAX_NUM) {
		ESP_LOGE(TAG, "Maximum number of LEDS reached");

		return ESP_FAIL;
	}

	/* Allocate memory for led instance */
	me->ledc_config = malloc(sizeof(ledc_channel_config_t));

	if(me->ledc_config == NULL) {
		ESP_LOGE(TAG, "Error allocating memory for LEDC configuration");

		return ESP_ERR_NO_MEM;
	}

	/* Fill data structure */
	me->ledc_config->channel = leds_num;
	me->ledc_config->flags.output_invert = 0;
	me->ledc_config->hpoint = 0;

	if(intensity > 100) {
		ESP_LOGE(TAG, "Error in intensity argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->ledc_config->duty = intensity * 80;	/* todo: define in macros */

	if(gpio < GPIO_NUM_0 || gpio >= GPIO_NUM_MAX) {
		ESP_LOGE(TAG, "Error in gpio number argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->ledc_config->gpio_num = gpio;

	if(me->mode == CONTINUOUS_MODE) {
		me->ledc_config->intr_type = LEDC_INTR_DISABLE;
	}
	else {
		me->ledc_config->intr_type = LEDC_INTR_FADE_END;
	}

	me->ledc_config->speed_mode = LED_SPEED_MODE;
	me->ledc_config->timer_sel = LED_TIMER;
	me->mode = mode;
	me->time = time;
	me->state = 0;

	/* Configure and initialize timer */
	if(!leds_num) {
		ledc_timer_config_t leds_timer = {
				.duty_resolution = LEDC_TIMER_13_BIT,
				.freq_hz = LED_TIMER_FREQ,
				.speed_mode = LED_SPEED_MODE,
				.timer_num = LED_TIMER,
				.clk_cfg = LEDC_AUTO_CLK,
		};

		ledc_timer_config(&leds_timer);
	}

	/* Set LED controller with its configuration */
	ret = ledc_channel_config(me->ledc_config);

	if(ret != ESP_OK) {
		return ret;
	}

	/* Turn off LED */
	ledc_stop(me->ledc_config->speed_mode, me->ledc_config->channel, 0);

	/* Install fade functionality service */
	if(me->ledc_config->intr_type == LEDC_INTR_FADE_END && !is_installed) {
		ret = ledc_fade_func_install(0);

		if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
			ESP_LOGE(TAG, "Error configuring fade functionality service");

			return ret;
		}

		is_installed = true;
	}

	/**/
	if(me->ledc_config->intr_type == LEDC_INTR_FADE_END && is_installed) {
		ledc_cbs_t callback = {
				.fade_cb = fade_end_cb
		};

		ESP_ERROR_CHECK(ledc_cb_register(me->ledc_config->speed_mode,
				me->ledc_config->channel,
				&callback,
				(void *)me));
	}

    /* Increment the LEDs counter */
	leds_num++;

	/* Return error code */
	return ret;
}

esp_err_t led_start(led_t * const me) {
	/* Error code variable */
	esp_err_t ret;

	switch(me->mode) {
		case FADE_MODE: {
		    ret = ledc_set_fade_with_time(me->ledc_config->speed_mode,
		    		me->ledc_config->channel,
					me->ledc_config->duty,
					me->time);

		    if(ret != ESP_OK) {
		    	ESP_LOGE(TAG, "Error configuring fade mode");

		    	return ret;
		    }

		    ret = ledc_fade_start(me->ledc_config->speed_mode,
		    		me->ledc_config->channel,
		    		LEDC_FADE_NO_WAIT);

		    if(ret != ESP_OK) {
		    	ESP_LOGE(TAG, "Error starting fade mode");

		    	return ret;
		    }

			break;
		}

		case CONTINUOUS_MODE: {
			ret = ledc_set_duty(me->ledc_config->speed_mode,
					me->ledc_config->channel,
					me->ledc_config->duty);

		    if(ret != ESP_OK) {
		    	ESP_LOGE(TAG, "Error configuring continuos mode");

		    	return ret;
		    }

			ret = ledc_update_duty(me->ledc_config->speed_mode,
					me->ledc_config->channel);

		    if(ret != ESP_OK) {
		    	ESP_LOGE(TAG, "Error starting continuos mode");

		    	return ret;
		    }

			break;
		}

		default:
			ESP_LOGE(TAG, "Error in mode argument");
			ret = ESP_ERR_INVALID_ARG;

			break;
	}

	/* Return error code */
	return ret;
}

esp_err_t led_stop(led_t * const me) {
	/* Error code variable */
	esp_err_t ret;

	ret = ledc_set_duty(me->ledc_config->speed_mode,
			me->ledc_config->channel,
			0);

    if(ret != ESP_OK) {
    	ESP_LOGE(TAG, "Error setting LEDC duty cycle");

    	return ret;
    }

	ret = ledc_update_duty(me->ledc_config->speed_mode,
			me->ledc_config->channel);

    if(ret != ESP_OK) {
    	ESP_LOGE(TAG, "Error updating LEDC duty cycle");

    	return ret;
    }

	ret = ledc_stop(me->ledc_config->speed_mode,
			me->ledc_config->channel,
			0);

    if(ret != ESP_OK) {
    	ESP_LOGE(TAG, "Error stopping LEDC duty cycle");

    	return ret;
    }

	/* Return error code */
	return ret;
}

esp_err_t led_set(led_t * const me, gpio_num_t gpio, led_mode_e mode, uint32_t time, uint8_t intensity) {
	/* Error code variable */
	esp_err_t ret;

	/* Overwrite data structure */
	if(gpio < GPIO_NUM_0 || gpio >= GPIO_NUM_MAX) {
		ESP_LOGE(TAG, "Error in gpio number argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->ledc_config->gpio_num = gpio;

	if(intensity > 100) {
		ESP_LOGE(TAG, "Error in intensity argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->ledc_config->duty = intensity * 80;

	if(mode != FADE_MODE && mode != CONTINUOUS_MODE) {
		ESP_LOGE(TAG, "Error in mode argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->mode = mode;

	if(me->mode == CONTINUOUS_MODE) {
		me->ledc_config->intr_type = LEDC_INTR_DISABLE;
	}
	else {
		me->ledc_config->intr_type = LEDC_INTR_FADE_END;
	}

	me->time = time;

	/* Turn off LED */
	ret = ledc_stop(me->ledc_config->speed_mode, me->ledc_config->channel, 0);

    if(ret != ESP_OK) {
    	ESP_LOGE(TAG, "Error stopping LEDC duty cycle");

    	return ret;
    }

	/* Install fade functionality service */
	if(me->ledc_config->intr_type == LEDC_INTR_FADE_END && !is_installed) {
		ret = ledc_fade_func_install(0);

	    if(ret != ESP_OK) {
	    	ESP_LOGE(TAG, "Error installing fade function");

	    	return ret;
	    }

		is_installed = true;
	}

	/* Register fade callback */
	if(me->ledc_config->intr_type == LEDC_INTR_FADE_END && is_installed) {
		ledc_cbs_t callback = {
				.fade_cb = fade_end_cb
		};

		ledc_cb_register(me->ledc_config->speed_mode,
				me->ledc_config->channel,
				&callback,
				(void *)me);

	    if(ret != ESP_OK) {
	    	ESP_LOGE(TAG, "Error registering fade callack function");

	    	return ret;
	    }
	}

	/* Start LED operation */
	ret = led_start(me);

    if(ret != ESP_OK) {
    	ESP_LOGE(TAG, "Error starting LED");

    	return ret;
    }

	/* Return error code */
	return ret;
}

/* Private functions ---------------------------------------------------------*/
static bool IRAM_ATTR fade_end_cb(const ledc_cb_param_t * param, void * arg) {
    portBASE_TYPE task_awoken = pdFALSE;

    led_t * led = (led_t *)arg;

    /* Set and start LED functionality */
    ledc_set_fade_with_time(led->ledc_config->speed_mode,
    				led->ledc_config->channel,
					led->state? 0 : led->ledc_config->duty,
					led->time);

    ledc_fade_start(led->ledc_config->speed_mode,
    		led->ledc_config->channel,
    		LEDC_FADE_NO_WAIT);

    /* Toggle LED state */
    led->state = !led->state;

    return (task_awoken == pdTRUE);
}

/***************************** END OF FILE ************************************/
