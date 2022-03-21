/**
  ******************************************************************************
  * @file           : led.h
  * @author         : Mauricio Barroso Benavides
  * @date           : Mar 21, 2022
  * @brief          : This file contains all the definitios, data types and
  *                   function prototypes for led.c file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LED_H_
#define LED_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
	FADE_MODE,	/*!< Fade light intensity operating mode */
	CONTINUOUS_MODE	/*!< Continuos light intensity operating mode */
	/* Add more effects here */
} led_mode_e;

typedef struct {
	ledc_channel_config_t * ledc_config;	/*!< LEDC channel configuration */
	led_mode_e mode;						/*!< LED operating mode */
	uint32_t time;							/*!< LED operating mode time */
	bool state;								/*!< LED current state */
} led_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief Create a LED instance
  *
  * @param me Pointer to led_t structure
  * @param gpio GPIO number to attach LED
  * @param mode LED operating mode. Can be FADE_MODE or CONTINUOS_MODE
  * @param time LED Operating mode time
  * @param intensity LED initial light intensity. Must be a value between 0 and
  * 100.
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t led_init(led_t * const me, gpio_num_t gpio, led_mode_e mode, uint32_t time, uint8_t intensity);

/**
  * @brief Start LED instance
  *
  * @param me Pointer to led_t structure
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t led_start(led_t * const me);

/**
  * @brief Stop LED instance
  *
  * @param me Pointer to led_t structure
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t led_stop(led_t * const me);

/**
  * @brief Set LED instance configuration
  *
  * @param me Pointer to led_t structure
  * @param gpio GPIO number to attach LED
  * @param mode LED operating mode. Can be FADE_MODE or CONTINUOS_MODE
  * @param time LED Operating mode time
  * @param intensity LED initial light intensity. Must be a value between 0 and
  * 100.
  *
  * @retval
  * 	- ESP_OK on success
  * 	- ESP_ERR_INVALID_ARG if the argument is invalid
  */
esp_err_t led_set(led_t * const me, gpio_num_t gpio, led_mode_e mode, uint32_t time, uint8_t intensity);

#ifdef __cplusplus
}
#endif

#endif /* LED_H_ */

/***************************** END OF FILE ************************************/
