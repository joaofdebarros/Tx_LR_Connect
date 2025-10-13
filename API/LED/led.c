/*
 * led.c
 *
 *  Created on: 13 de out. de 2025
 *      Author: joao.victor
 */
#include "led.h"

sl_sleeptimer_timer_handle_t periodic_timer;

uint8_t blink_count = 0;
uint8_t blink_target = 0;
uint8_t led_target;

void led_blink(uint8_t led, uint8_t blinks, uint16_t speed){
  led_target = led;
  blink_target = blinks;

  sl_sleeptimer_start_periodic_timer_ms(&periodic_timer,speed,led_handler, NULL,0,SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
}

void led_handler(sl_sleeptimer_timer_handle_t *handle, void *data){

  (void)&handle;
  (void)&data;

  switch (led_target) {
    case VERMELHO:

      if(blink_count < (blink_target * 2)){
          blink_count++;
          hGpio_ledToggle(&sl_led_led_vermelho);
      }else{
          blink_count = 0;
          hGpio_ledTurnOff(&sl_led_led_vermelho);
          sl_sleeptimer_stop_timer(&periodic_timer);
      }
      break;
    case VERDE:

      if(blink_count < (blink_target * 2)){
          blink_count++;
//          hGpio_ledToggle(&sl_led_led_verde, application.IVP.SensorStatus.Status.led_enabled);
      }else{
          blink_count = 0;
//          hGpio_ledTurnOff(&sl_led_led_verde, application.IVP.SensorStatus.Status.led_enabled);
          sl_sleeptimer_stop_timer(&periodic_timer);
      }
      break;
    case AZUL:

      if(blink_count < (blink_target * 2)){
          blink_count++;
//          hGpio_ledToggle(&sl_led_led_azul, application.IVP.SensorStatus.Status.led_enabled);
      }else{
          blink_count = 0;
//          hGpio_ledTurnOff(&sl_led_led_azul, application.IVP.SensorStatus.Status.led_enabled);
          sl_sleeptimer_stop_timer(&periodic_timer);
      }
      break;
    default:
      break;
  }

}
