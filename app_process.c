/***************************************************************************//**
 * @file
 * @brief app_process.c
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include PLATFORM_HEADER
#include "sl_component_catalog.h"
#include "stack/include/ember.h"
#include "em_chip.h"
#include "app_log.h"
#include "poll.h"
#include "sl_app_common.h"
#include "app_process.h"
#include "sl_sleeptimer.h"
#include "app_framework_common.h"
#if defined(SL_CATALOG_LED0_PRESENT)
#include "sl_simple_led_instances.h"
#endif
#include "sl_simple_button_instances.h"
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_power_manager.h"
#endif
#include "API/LED/led.h"
#include "API/hNetwork.h"
#include "hplatform/hDriver/hDriver.h"
#include "API/battery/battery.h"
#include "privAPI/Radio.h"
#include "sl_power_manager.h"
#include "API/memory/memory.h"
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define MAX_TX_FAILURES     (10U)

#define APP_BUTTON_PRESS_DURATION_SHORT        0
#define APP_BUTTON_PRESS_DURATION_MEDIUM       1
#define APP_BUTTON_PRESS_DURATION_LONG         2
#define APP_BUTTON_PRESS_DURATION_VERYLONG     3
#define APP_BUTTON_PRESS_NONE                  4
#define APP_BUTTON_PRESS_PRESSED_DOWN          5

#define ALPHA 0.1f
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
/// Global flag set by a button push to allow or disallow entering to sleep
bool enable_sleep = false;
bool adc_read = false;
/// report timing event control
EmberEventControl *report_control;
EmberEventControl *EM4_timeout;
/// report timing period
uint16_t sensor_report_period_ms =  (1 * MILLISECOND_TICKS_PER_SECOND);
/// TX options set up for the network
EmberMessageOptions tx_options = EMBER_OPTIONS_ACK_REQUESTED | EMBER_OPTIONS_SECURITY_ENABLED;

packet_void_t sendRadio;

application_t application;
extern uint8_t tx_power;
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
/// Destination of the currently processed sink node
static EmberNodeId sink_node_id = EMBER_COORDINATOR_ADDRESS;
bool registrado = false;
bool Tx_cadastrado = false;
bool button_is_pressed = false;
uint32_t press_start_time = 0;
extern uint16_t Vbat;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

void app_button_press_cb(uint8_t button, uint8_t duration)
{
  if(button == 2 && duration < 3){
      join_sleepy(0);
      sl_led_turn_on(&sl_led_led_vermelho);
      emberEventControlSetInactive(*EM4_timeout);
  }else if(button == 2 && duration <= 3){
      leave();
      reset_parameters();
      emberEventControlSetDelayMS(*EM4_timeout,2000);
  }else{

      application.radio.LastCMD = TX_CMD_BT;
      application.tecla = button + 1;

      led_blink(VERMELHO, 10, VERY_FAST_SPEED_BLINK);

      emberEventControlSetDelayMS(*report_control,200);
  }
}

void reset_parameters(){
  memory_erase(TXPOWER_MEMORY_KEY);
  memory_erase(STATUSOP_MEMORY_KEY);

  tx_power = 150;
  set_tx(tx_power);
  application.Status_Operation = WAIT_REGISTRATION;

  memory_write(STATUSOP_MEMORY_KEY, &application.Status_Operation, sizeof(application.Status_Operation));
  memory_write(TXPOWER_MEMORY_KEY, &tx_power, sizeof(tx_power));

}

EmberStatus radio_send_packet(packet_void_t *pck){
  uint8_t buffer_send[8];
  EmberStatus status;

  buffer_send[0] = pck->cmd;
  for(uint8_t i = 0; i < (pck->len-1); i++){
      buffer_send[i+1] = pck->data[i];
  }
  status = radioMessageSend(0,pck->len,buffer_send);

  return status;
}

void battery_read(){
  uint16_t reading = 0;

  reading = calculateVdd();

  if(reading != 0){
      if(Vbat == 0){
          Vbat = reading;
      }else{
          Vbat = ALPHA * reading + (1.0f - ALPHA) * Vbat;
      }
  }

  memory_write(BATTERY_MEMORY_KEY, &Vbat, sizeof(Vbat));
}

void em4_handler(void){
  bool button_state = GPIO_PinInGet(gpioPortB, 1);

  if(adc_read){
      adc_read = false;
      if(button_state == false){
          battery_read();
          sl_power_manager_enter_em4();
          emberEventControlSetInactive(*EM4_timeout);
      }else{
          emberEventControlSetDelayMS(*EM4_timeout,2000);
      }
  }else{
      adc_read = true;
      battery_read();
      hGpio_ledTurnOff(&sl_led_led_vermelho);
      emberEventControlSetDelayMS(*EM4_timeout,10);
  }

}

/**************************************************************************//**
 * Here we print out the first two bytes reported by the sinks as a little
 * endian 16-bits decimal.
 *****************************************************************************/
void report_handler(void)
{
//  Vbat = calculateVdd();
  volatile Register_Sensor_t Register_Sensor;

  switch (application.Status_Operation) {
      case WAIT_REGISTRATION:
        Register_Sensor.Status.Type = REMOTE_CONTROL;
        Register_Sensor.Status.range = LONG_RANGE;

        sendRadio.cmd = TX_REGISTRATION;
        sendRadio.len = 2;
        sendRadio.data[0] = Register_Sensor.Registerbyte;

        break;

      case OPERATION_MODE:
        if(application.radio.LastCMD == TX_CMD_BT){
            sendRadio.cmd = TX_CMD_BT;
            sendRadio.len = 4;
            sendRadio.data[0] = application.tecla;          //Tecla
            sendRadio.data[1] = Vbat;                  //Bateria
            sendRadio.data[2] = Vbat >> 8;
        }

        break;

      default:
        break;
    }

  radio_send_packet(&sendRadio);

  battery_read();

  emberEventControlSetInactive(*report_control);
  emberEventControlSetDelayMS(*EM4_timeout,2000);
}

/**************************************************************************//**
 * Entering sleep is approved or denied in this callback, depending on user
 * demand.
 *****************************************************************************/
bool emberAfCommonOkToEnterLowPowerCallback(bool enter_em2, uint32_t duration_ms)
{
  (void) enter_em2;
  (void) duration_ms;
  return enable_sleep;
}

/**************************************************************************//**
 * This function is called when a message is received.
 *****************************************************************************/
void emberAfIncomingMessageCallback(EmberIncomingMessage *message)
{
  if (message->endpoint == SL_SENSOR_SINK_ENDPOINT) {
    app_log_info("RX: Data from 0x%04X:", message->source);
    for (uint8_t i = SL_SENSOR_SINK_DATA_OFFSET; i < message->length; i++) {
      app_log_info(" %x", message->payload[i]);
    }
    app_log_info("\n");
  }
}

/**************************************************************************//**
 * This function is called to indicate whether an outgoing message was
 * successfully transmitted or to indicate the reason of failure.
 *****************************************************************************/
void emberAfMessageSentCallback(EmberStatus status,
                                EmberOutgoingMessage *message)
{
  if(message->payload[0] == TX_REGISTRATION && application.Status_Operation == WAIT_REGISTRATION && status == EMBER_SUCCESS){
      //Estado inicial do sensor apos cadastro
      application.Status_Operation = OPERATION_MODE;

      memory_write(STATUSOP_MEMORY_KEY, &application.Status_Operation, sizeof(application.Status_Operation));
  }
}

/**************************************************************************//**
 * This function is called when the stack status changes.
 *****************************************************************************/
void emberAfStackStatusCallback(EmberStatus status)
{
  switch (status) {
    case EMBER_NETWORK_UP:
      Tx_cadastrado = true;
      enable_sleep = true;
      led_blink(VERMELHO, 2, MED_SPEED_BLINK);

      if(application.Status_Operation == WAIT_REGISTRATION){
          emberEventControlSetDelayMS(*report_control, sensor_report_period_ms);
      }
      break;
    case EMBER_NETWORK_DOWN:
      Tx_cadastrado = false;
      led_blink(VERMELHO, 5, FAST_SPEED_BLINK);
      break;
    case EMBER_JOIN_SCAN_FAILED:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      break;
    case EMBER_JOIN_DENIED:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      break;
    case EMBER_JOIN_TIMEOUT:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      break;
    default:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      break;
  }

  emberEventControlSetDelayMS(*EM4_timeout,6000);
}

/**************************************************************************//**
 * This callback is called in each iteration of the main application loop and
 * can be used to perform periodic functions.
 *****************************************************************************/
void emberAfTickCallback(void)
{

}

/**************************************************************************//**
 * This function is called when a frequency hopping client completed the start
 * procedure.
 *****************************************************************************/
void emberAfFrequencyHoppingStartClientCompleteCallback(EmberStatus status)
{
  if (status != EMBER_SUCCESS) {
    app_log_error("FH Client sync failed, status=0x%02X\n", status);
  } else {
    app_log_info("FH Client Sync Success\n");
  }
}

/**************************************************************************//**
 * This function is called when a requested energy scan is complete.
 *****************************************************************************/
void emberAfEnergyScanCompleteCallback(int8_t mean,
                                       int8_t min,
                                       int8_t max,
                                       uint16_t variance)
{
  app_log_info("Energy scan complete, mean=%d min=%d max=%d var=%d\n",
               mean, min, max, variance);
}

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS) && defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1)

/**************************************************************************//**
 * This function is called from the Micrium RTOS plugin before the
 * Application (1) task is created.
 *****************************************************************************/
void emberAfPluginMicriumRtosAppTask1InitCallback(void)
{
  app_log_info("app task init\n");
}

#include <kernel/include/os.h>
#define TICK_INTERVAL_MS 1000

/**************************************************************************//**
 * This function implements the Application (1) task main loop.
 *****************************************************************************/
void emberAfPluginMicriumRtosAppTask1MainLoopCallback(void *p_arg)
{
  RTOS_ERR err;
  OS_TICK yield_time_ticks = (OSCfg_TickRate_Hz * TICK_INTERVAL_MS) / 1000;

  while (true) {
    app_log_info("app task tick\n");

    OSTimeDly(yield_time_ticks, OS_OPT_TIME_DLY, &err);
  }
}

#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS && EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1
