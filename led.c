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
#include "freertos/queue.h"

/* External variables --------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#if CONFIG_IDF_TARGET_ESP32
#define LED_SPEED_MODE	LEDC_HIGH_SPEED_MODE
#endif

#if !CONFIG_IDF_TARGET_ESP32
#define LED_SPEED_MODE	LEDC_LOW_SPEED_MODE
#endif

#define LED_MAX_NUM			SOC_LEDC_CHANNEL_NUM
#define LED_TIMER_FREQ	CONFIG_LED_TIMER_FREQ
#define LED_TIMER_NUM		CONFIG_LED_TIMER_NUM

/* Private function prototypes -----------------------------------------------*/
static bool fade_end_cb(const ledc_cb_param_t * param, void * arg);
static void led_control_task(void * arg);

/* Private variables ---------------------------------------------------------*/
static const char * TAG = "led";
static uint8_t led_num = 0;
static TaskHandle_t led_control_handle = NULL;
static QueueHandle_t led_control_queue = NULL;

/* Exported functions --------------------------------------------------------*/
esp_err_t led_init(led_t * const me, gpio_num_t gpio) {
	ESP_LOGI(TAG, "Initializing led component...");

	/* Error code variable */
	esp_err_t ret;

	/* Check if the maximum number of LEDs was reached */
	if(led_num >= LED_MAX_NUM) {
		ESP_LOGE(TAG, "Maximum number of LEDS reached");

		return ESP_FAIL;
	}

	/* Configure and initialize timer for the first instance */
	if(!led_num) {
		ledc_timer_config_t leds_timer = {
				.duty_resolution = LEDC_TIMER_13_BIT,
				.freq_hz = LED_TIMER_FREQ,
				.speed_mode = LED_SPEED_MODE,
				.timer_num = LED_TIMER_NUM,
				.clk_cfg = LEDC_AUTO_CLK,
		};

		ret = ledc_timer_config(&leds_timer);

		if(ret != ESP_OK) {
			ESP_LOGE(TAG, "Failed to configure timer");

			return ret;
		}
	}

	/* Allocate memory for led instance */
	me->ledc_config = malloc(sizeof(ledc_channel_config_t));

	if(me->ledc_config == NULL) {
		ESP_LOGE(TAG, "Error to allocate memory for LEDC configuration");

		return ESP_ERR_NO_MEM;
	}

	/* Fill data structure */
	me->ledc_config->channel = led_num;
	me->ledc_config->duty = 0;
	me->ledc_config->gpio_num = gpio;
	me->ledc_config->speed_mode = LED_SPEED_MODE;
	me->ledc_config->hpoint = 0;
	me->ledc_config->timer_sel = LED_TIMER_NUM;
	me->ledc_config->flags.output_invert = 0;
	me->ledc_config->intr_type = LEDC_INTR_DISABLE;

	/* Set LED controller with its configuration */
	ledc_channel_config(me->ledc_config);

	/* Install fade functionality for the first instance */
	if(!led_num) {
		ret = ledc_fade_func_install(0);

		if(ret != ESP_OK) {
			return ret;
		}
	}

	/* Register fade callback */
	ledc_cbs_t callback = {
			.fade_cb = fade_end_cb
	};

	ledc_cb_register(me->ledc_config->speed_mode,
			me->ledc_config->channel,
			&callback,
			(void *)me);

	/* Initialize other variables */
	me->time = 0;
	me->state = 0;
	me->mode = CONTINUOUS_MODE;

	/* Increment the LED counter */
	led_num++;

	/* Create queue to send data to control LEDs */
	if(led_control_queue == NULL) {
		led_control_queue = xQueueCreate(LED_MAX_NUM * 2, sizeof(led_t *));

		if(led_control_queue == NULL) {
			ESP_LOGE(TAG, "Failed to create queue");

			return ESP_FAIL;
		}
	}

	/* Create task to control LEDs */
	if(led_control_handle == NULL) {
		xTaskCreate(led_control_task,
				"LED control task",
				configMINIMAL_STACK_SIZE * 4,
				NULL,
				tskIDLE_PRIORITY + 1,
				&led_control_handle);

		if(led_control_handle == NULL) {
			ESP_LOGE(TAG, "Failed to create task");

			return ESP_FAIL;
		}
	}

	/* Return ESP_OK if successful */
	return ESP_OK;
}

esp_err_t led_set_continuous(led_t * const me, uint8_t intensity) {
	/* Set mode */
	me->mode = CONTINUOUS_MODE;

	/* Set duty value according the new intensity value*/
	if(intensity > 100) {
		ESP_LOGE(TAG, "Error in intensity argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->ledc_config->duty = intensity * 81;

	/* Send to queue */
	if(xQueueSend(led_control_queue, &me, 0) != pdPASS) {
		ESP_LOGE(TAG, "Failed to send to queue");

		return ESP_FAIL;
	}

	return ESP_OK;
}
esp_err_t led_set_fade(led_t * const me, uint8_t intensity, uint32_t time) {
	/* Set mode */
	me->mode = FADE_MODE;

	/* Set duty value according the new intensity value*/
	if(intensity > 100) {
		ESP_LOGE(TAG, "Error in intensity argument");

		return ESP_ERR_INVALID_ARG;
	}

	me->ledc_config->duty = intensity * 81;

	/* Set new time value */
	me->time = time;

	/* Send to queue */
	if(xQueueSend(led_control_queue, &me, 0) != pdPASS) {
		ESP_LOGE(TAG, "Failed to send to queue");

		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Private functions ---------------------------------------------------------*/
static bool fade_end_cb(const ledc_cb_param_t * param, void * arg) {
    portBASE_TYPE task_awoken = pdFALSE;

    if(param->event == LEDC_FADE_END_EVT) {
    	xQueueSendFromISR(led_control_queue, &arg, 0);
    }

    return (task_awoken == pdTRUE);
}

static void led_control_task(void * arg) {
	/* Declare led instance pointer */
	led_t * led;

	/* Inifinite loop */
	for(;;) {
		/* Try to read the queue */
		if(xQueueReceive(led_control_queue, &led, portMAX_DELAY) == pdPASS) {
			/* Set the functionality according the LED mode */
			switch(led->mode) {
				case CONTINUOUS_MODE:
					/* Set and update duty */
					if(ledc_set_duty(led->ledc_config->speed_mode,
							led->ledc_config->channel,
							led->ledc_config->duty) == ESP_OK) {

						ledc_update_duty(led->ledc_config->speed_mode,
							led->ledc_config->channel);
					}
					else {
						ESP_LOGE(TAG, "Failed to set duty");
					}

					break;

				case BLINK_MODE:
					/* todo: implement */
					break;

				case FADE_MODE:
					/* Toggle LED state */
					led->state = !led->state;

					/* Set and start fade functionality */
					if(ledc_set_fade_with_time(led->ledc_config->speed_mode,
							led->ledc_config->channel,
							led->state? 0 : led->ledc_config->duty,
							led->time) == ESP_OK) {

						ledc_fade_start(led->ledc_config->speed_mode,
								led->ledc_config->channel,
								LEDC_FADE_NO_WAIT);
					}
					else {
						ESP_LOGE(TAG, "Failed to set fade");
					}

					break;

				default:
					ESP_LOGW(TAG, "Unknown LED mode");

					break;
			}
		}
	}
}

/***************************** END OF FILE ************************************/
