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
#include "API/packet/pckDataStructure.h"
#include "privAPI/Radio.h"
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
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
/// Global flag set by a button push to allow or disallow entering to sleep
bool enable_sleep = false;
/// report timing event control
EmberEventControl *report_control;
/// report timing period
uint16_t sensor_report_period_ms =  (1 * MILLISECOND_TICKS_PER_SECOND);
/// TX options set up for the network
EmberMessageOptions tx_options = EMBER_OPTIONS_ACK_REQUESTED | EMBER_OPTIONS_SECURITY_ENABLED;

packet_void_t sendRadio;
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
/// Destination of the currently processed sink node
static EmberNodeId sink_node_id = EMBER_COORDINATOR_ADDRESS;
bool registrado = false;
bool Tx_cadastrado = false;
bool button_is_pressed = false;
uint32_t press_start_time = 0;
uint16_t Vbat = 0;
// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
void sl_button_on_change(const sl_button_t *handle)
{
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
       if(&sl_button_btn0 == handle){
           press_start_time = sl_sleeptimer_get_tick_count();
           button_is_pressed = true;
           if(Tx_cadastrado){
               led_blink(VERMELHO, 10, VERY_FAST_SPEED_BLINK);
           }
       }
    }

    if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED){
        uint32_t current_time = sl_sleeptimer_get_tick_count();
        button_is_pressed = false;
//        if((current_time - press_start_time) > 100000){
//            leave();
//            sl_led_turn_on(&sl_led_led_vermelho);
//        }else

        if(current_time < 60000 && Tx_cadastrado){
            leave();
        }

        if((current_time - press_start_time) < 50000){
            if(!Tx_cadastrado){
                join_sleepy(0);
                hGpio_ledTurnOn(&sl_led_led_vermelho);
            }else{
//                  led_blink(VERMELHO, 1, SLOW_SPEED_BLINK);
                emberEventControlSetDelayMS(*report_control, sensor_report_period_ms);
            }

        }
    }
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

/**************************************************************************//**
 * Here we print out the first two bytes reported by the sinks as a little
 * endian 16-bits decimal.
 *****************************************************************************/
void report_handler(void)
{
  EmberStatus status;
  static bool registrado = false;
  uint8_t buffer[SL_SENSOR_SINK_DATA_LENGTH];

  volatile Register_Sensor_t Register_Sensor;

  Register_Sensor.Status.Type = CONTROL;
  Register_Sensor.Status.range = LONG_RANGE;

  if(!registrado){
      sendRadio.cmd = TX_REGISTRATION;
      sendRadio.len = 2;
      sendRadio.data[0] = Register_Sensor.Registerbyte;
      Vbat = calculateVdd();

      registrado = true;
  }
  else{

      sendRadio.cmd = TX_CMD;
      sendRadio.len = 4;
      sendRadio.data[0] = 1;                          //Estado de Operação
      sendRadio.data[1] = Vbat >> 8;                  //Bateria
      sendRadio.data[2] = Vbat;                       //Bateria
  }
  radio_send_packet(&sendRadio);

  Vbat = calculateVdd();
  emberEventControlSetInactive(*report_control);
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
  (void) message;
  if (status != EMBER_SUCCESS) {
    app_log_info("TX: 0x%02X\n", status);
  }
}

/**************************************************************************//**
 * This function is called when the stack status changes.
 *****************************************************************************/
void emberAfStackStatusCallback(EmberStatus status)
{
  switch (status) {
    case EMBER_NETWORK_UP:
      app_log_info("Network up\n");
      app_log_info("Joined to Sink with node ID: 0x%04X\n", emberGetNodeId());
      // Schedule start of periodic sensor reporting to the Sink
      Tx_cadastrado = true;
      enable_sleep = true;
      led_blink(VERMELHO, 2, MED_SPEED_BLINK);
      emberEventControlSetDelayMS(*report_control, sensor_report_period_ms);
      break;
    case EMBER_NETWORK_DOWN:
      app_log_info("Network down\n");
      Tx_cadastrado = false;
      led_blink(VERMELHO, 5, FAST_SPEED_BLINK);
      break;
    case EMBER_JOIN_SCAN_FAILED:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      app_log_error("Scanning during join failed\n");
      break;
    case EMBER_JOIN_DENIED:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      app_log_error("Joining to the network rejected!\n");
      break;
    case EMBER_JOIN_TIMEOUT:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      app_log_info("Join process timed out!\n");
      break;
    default:
      led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      app_log_info("Stack status: 0x%02X\n", status);
      break;
  }
}

/**************************************************************************//**
 * This callback is called in each iteration of the main application loop and
 * can be used to perform periodic functions.
 *****************************************************************************/
void emberAfTickCallback(void)
{

  uint32_t current_time = sl_sleeptimer_get_tick_count();

    if(button_is_pressed){
        if((current_time - press_start_time) > 100000){
            sl_led_turn_on(&sl_led_led_vermelho);
        }
    }
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
