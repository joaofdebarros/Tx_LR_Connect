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
#include "hplatform/hDriver/hADC.h"
#include "callbacks/callbacks.h"
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
bool initialized = false;
/// report timing event control
EmberEventControl *report_control;
EmberEventControl *EM4_timeout;
EmberEventControl *Init_control;
EmberEventControl *radio_control;
/// report timing period
uint16_t sensor_report_period_ms =  (1 * MILLISECOND_TICKS_PER_SECOND);
/// TX options set up for the network
EmberMessageOptions tx_options = EMBER_OPTIONS_ACK_REQUESTED | EMBER_OPTIONS_SECURITY_ENABLED;

packet_void_t sendRadio;

application_t application;

uint8_t tx_power = 150;
uint16_t Vbat = 0;

bool registrado = false;
bool Tx_cadastrado = false;
bool button_is_pressed = false;
uint32_t press_start_time = 0;

bool reset_pressed = false;
uint8_t reset_time = 0;

bool associating = false;
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
/// Destination of the currently processed sink node
static EmberNodeId sink_node_id = EMBER_COORDINATOR_ADDRESS;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

void app_init(){
  emberAfAllocateEvent(&report_control, &report_handler);
  emberAfAllocateEvent(&EM4_timeout, &em4_handler);
  emberAfAllocateEvent(&Init_control, &Init_handler);
  emberAfAllocateEvent(&radio_control, &radio_handler);

  emberEventControlSetDelayMS(*Init_control, 50);
}

void Init_handler(){
  emberAfPluginPollEnableShortPolling(true);

  memory_read(STATUSOP_MEMORY_KEY, &application.Status_Operation);
  memory_read(TXPOWER_MEMORY_KEY, &tx_power);
  memory_read(BATTERY_MEMORY_KEY, &Vbat);
  memory_read(GATE_STATUS_MEMORY_KEY, &application.gate_status);
  memory_read(RSSI_MEMORY_KEY, &application.radio.RSSI);
  memory_read(LR_KEY_MEMORY_KEY, &application.LR_key);

  app_button_press_enable();

  set_tx(tx_power);
  iadcInit();

  initialized = true;

  bool button_state = GPIO_PinInGet(gpioPortA, 5);

  if(button_state){
      reset_pressed = true;
  }

  emberEventControlSetInactive(*Init_control);
  emberEventControlSetDelayMS(*EM4_timeout, 2000);
}

void app_button_press_cb(uint8_t button, uint8_t duration)
{
  if(button == 3 && duration < 3){
      reset_pressed = false;
      if(application.Status_Operation == WAIT_REGISTRATION){
          associating = true;
          sl_led_turn_on(&sl_led_led_vermelho);
          leave();
      }else{
          application.radio.LastCMD = TX_CMD_BT;
          application.tecla = button + 1;

          //      if(application.tecla != 2){
          led_blink(VERMELHO, 10, VERY_FAST_SPEED_BLINK);
          //      }

          emberEventControlSetDelayMS(*report_control,1);
      }

      emberEventControlSetDelayMS(*EM4_timeout,6000);
  }else if(button == 3 && duration >= 3 && duration != 5){
      reset_time = 0;
      reset_pressed = false;

      leave();
      reset_parameters();
      emberEventControlSetDelayMS(*EM4_timeout,2000);
  }else if(button == 3 && duration == 5){
      reset_pressed = true;
      emberEventControlSetDelayMS(*EM4_timeout,2000);
  }else{

      application.radio.LastCMD = TX_CMD_BT;
      application.tecla = button + 1;

//      if(application.tecla != 2){
          led_blink(VERMELHO, 10, VERY_FAST_SPEED_BLINK);
//      }

      emberEventControlSetDelayMS(*report_control,1);
  }
}

void reset_parameters(){
  memory_erase(TXPOWER_MEMORY_KEY);
  memory_erase(STATUSOP_MEMORY_KEY);
  memory_erase(LR_KEY_MEMORY_KEY);

  application.LR_key = 0;

  tx_power = 150;
  set_tx(tx_power);
  application.Status_Operation = WAIT_REGISTRATION;

  memory_write(STATUSOP_MEMORY_KEY, &application.Status_Operation, sizeof(application.Status_Operation));
  memory_write(TXPOWER_MEMORY_KEY, &tx_power, sizeof(tx_power));
  memory_write(LR_KEY_MEMORY_KEY, &application.LR_key, sizeof(application.LR_key));

}

EmberStatus radio_send_packet(packet_void_t *pck){
  uint8_t buffer_send[8];
  EmberStatus status;

  buffer_send[0] = pck->cmd;
  for(uint8_t i = 0; i < (pck->len-1); i++){
      buffer_send[i+1] = pck->data[i];
  }
  buffer_send[pck->len] = application.LR_key;
  buffer_send[(pck->len)+1] = application.LR_key >> 8;

  status = radioMessageSend(0,(pck->len)+2,buffer_send);

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
  bool button_state = GPIO_PinInGet(gpioPortA, 5);

  if(adc_read){
      adc_read = false;
      if(button_state == false){
          battery_read();
          sl_power_manager_enter_em4();
          emberEventControlSetInactive(*EM4_timeout);
      }else{
          if(reset_pressed){
              reset_time++;
              if(reset_time >= 3){
                  hGpio_ledTurnOn(&sl_led_led_vermelho);
              }
          }
          emberEventControlSetDelayMS(*EM4_timeout,2000);
      }
  }else{
      adc_read = true;
      battery_read();
      hGpio_ledTurnOff(&sl_led_led_vermelho);
      emberEventControlSetDelayMS(*EM4_timeout,10);
  }

}

void radio_handler(void){
  packet_void_t *receive;

  receive = &application.radio.Packet;

  switch(receive->cmd){
    case STATUS_GATE:
      application.gate_status = receive->data[0];
      memory_write(GATE_STATUS_MEMORY_KEY, &application.gate_status, sizeof(application.gate_status));
      switch (application.gate_status) {
        case CLOSED:
          led_blink(VERMELHO, 1, MED_SPEED_BLINK);
          break;
        case ABERTO:
          led_blink(VERMELHO, 2, MED_SPEED_BLINK);
          break;
        case FECHADO:
          led_blink(VERMELHO, 1, MED_SPEED_BLINK);
          break;
        default:
          led_blink(VERMELHO, 2, MED_SPEED_BLINK);
          break;
      }
      break;

    case CHANGE_STATUS:
      application.gate_status = receive->data[0];
      led_blink(VERMELHO, 1, MED_SPEED_BLINK);

      memory_write(GATE_STATUS_MEMORY_KEY, &application.gate_status, sizeof(application.gate_status));

      break;

    case LR_KEY:
      application.LR_key = (receive->data[1] << 8) | (receive->data[0]);

      memory_write(LR_KEY_MEMORY_KEY, &application.LR_key, sizeof(application.LR_key));
      break;
    default:
      break;
  }
  emberEventControlSetInactive(*radio_control);
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
            sendRadio.len = 5;
            sendRadio.data[0] = application.tecla;          //Tecla
            sendRadio.data[1] = Vbat;                  //Bateria
            sendRadio.data[2] = Vbat >> 8;
            sendRadio.data[3] = application.radio.RSSI;
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
  privcallback_Radio_Receive(message->payload,message->length);
  application.radio.RSSI = -(message->rssi);

  memory_write(RSSI_MEMORY_KEY, &application.radio.RSSI,sizeof(application.radio.RSSI));

  if(application.Status_Operation == WAIT_REGISTRATION){
      privcallback_Radio_Receive(message->payload,message->length);
      application.radio.RSSI = -(message->rssi);
  }else if(application.Status_Operation == PERIOD_INSTALATION || application.Status_Operation == OPERATION_MODE){
      uint16_t received_key = 0;

      received_key = (message->payload[message->length - 1] << 8) | (message->payload[message->length - 2]);

      if(application.LR_key != 0){
          if(received_key == application.LR_key){
              privcallback_Radio_Receive(message->payload,message->length - 2);
              application.radio.RSSI = -(message->rssi);
          }
      }else{
          privcallback_Radio_Receive(message->payload,message->length - 2);
          application.radio.RSSI = -(message->rssi);
      }
  }

  memory_write(RSSI_MEMORY_KEY, &application.radio.RSSI,sizeof(application.radio.RSSI));
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

  application.radio.RSSI = -(message->ackRssi);
  memory_write(RSSI_MEMORY_KEY, &application.radio.RSSI,sizeof(application.radio.RSSI));
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
//      EmberStatus FH_success;
//      FH_success = emberFrequencyHoppingStartClient(0, SL_SENSOR_SINK_PAN_ID_JB);
//
//      if(FH_success == 0){
//          led_blink(VERMELHO, 2, MED_SPEED_BLINK);
//      }
      if(initialized){
          led_blink(VERMELHO, 2, MED_SPEED_BLINK);
      }

      if(application.Status_Operation == WAIT_REGISTRATION && initialized){
          emberEventControlSetDelayMS(*report_control, sensor_report_period_ms);
      }
      break;
    case EMBER_NETWORK_DOWN:
      Tx_cadastrado = false;
      if(initialized){
          if(associating){
              join_sleepy(0);
              associating = false;
          }else{
              led_blink(VERMELHO, 5, FAST_SPEED_BLINK);
          }
      }
      break;
    case EMBER_JOIN_SCAN_FAILED:
      if(initialized){
          led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      }
      break;
    case EMBER_JOIN_DENIED:
      if(initialized){
          led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      }
      break;
    case EMBER_JOIN_TIMEOUT:
      if(initialized){
          led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      }
      break;
    default:
      if(initialized){
          led_blink(VERMELHO, 2, SLOW_SPEED_BLINK);
      }
      break;
  }
  if(initialized){
      emberEventControlSetDelayMS(*EM4_timeout,6000);
  }

}

packet_error_e packet_data_demount(uint8_t *inData, uint8_t inLen, packet_void_t *packet){
  uint8_t i;
  packet->cmd = inData[0];
  for(i = 0; i < inLen; i++){
      packet->data[i] = inData[i+1];
  }

  return PACKET_OK;
}

