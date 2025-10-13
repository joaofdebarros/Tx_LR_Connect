/*
 * led.h
 *
 *  Created on: 13 de out. de 2025
 *      Author: joao.victor
 */

#ifndef API_LED_LED_H_
#define API_LED_LED_H_

#include "stdint.h"
#include "sl_sleeptimer.h"
#include "sl_simple_led_instances.h"

#define SLOW_SPEED_BLINK  1000
#define MED_SPEED_BLINK   200
#define FAST_SPEED_BLINK  100
#define VERY_FAST_SPEED_BLINK 50

typedef enum{
  VERMELHO = 0,
  VERDE,
  AZUL
}LED_t;

void led_blink(uint8_t led, uint8_t blinks, uint16_t speed);
void led_handler(sl_sleeptimer_timer_handle_t *handle, void *data);

#endif /* API_LED_LED_H_ */
