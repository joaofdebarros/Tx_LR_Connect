/***************************************************************************//**
 * @file
 * @brief app_process.h
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
#ifndef APP_PROCESS_H
#define APP_PROCESS_H

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

typedef enum{
  WAIT_REGISTRATION = 0,
  PERIOD_INSTALATION,
  OPERATION_MODE,
  BOOT,
  RESETTING
}Status_Operation_t;

typedef enum{
  REMOTE_CONTROL = 0,
  MOTION_DETECT,
  OPEN_CLOSE_DETECT,
  GATE
}Type;

//typedef enum{
//  DISARMED = 0,
//  ARMED
//}Status_Central_t;

typedef enum{
  SUCCESS,
  FAIL
}TX_config_error_e;

typedef enum{
  HARDWARE_FULL_RESET,
  LR_FULL_RESET,
  LR_DISCONNECT
}leaving_method_t;

typedef enum{
  CLOSED = 0,
  ABERTO,
  ABRINDO,
  FECHADO,
  FECHANDO,
  SEMIABERTO,
  TRAVADO,
  LENDO_ABRE,
  LENDO_FECHA,
  INICIAL,
  ERROR = 0xFF,
}gate_status_t;

typedef enum SensorCmd_e{
  TX_REGISTRATION = 15,
  TX_CMD_BT = 16,
  IVP_REGISTRATION = 21,
  MOTION_DETECTED = 22,
  CHANGE_STATUS = 30,
  STATUS_GATE = 31,
  SETUP_IVP,
  KEEP_ALIVE,
  STATUS_CENTRAL = 43,
  TAMPER,

  CMD_UNKNOWN = 0xFF
}SensorCmd_e;

typedef union
{
    uint8_t Registerbyte;

    struct
    {
        uint8_t Type                  :5;
        uint8_t range                 :2;
        uint8_t reserved              :1;
    } Status;

}
Register_Sensor_t;

typedef enum{
  LONG_RANGE = 0,
  MID_RANGE
}Range_t;

typedef struct{
    SensorCmd_e cmd;
    uint8_t data[8];
    uint8_t len;
}packet_void_t;

//typedef union
//{
//    uint8_t Statusbyte;
//
//    struct
//    {
//        Status_Operation_t operation          :3;
//        uint8_t reserved                      :5;
//    } Status;
//
//}
//SensorStatus_t;

typedef enum{
  PACKET_OK,
  PACKET_FAIL
}packet_error_e;

typedef struct{
  packet_void_t Packet;
  SensorCmd_e LastCMD;
  uint8_t RSSI;
}application_radio_t;

typedef struct{
  application_radio_t radio;
  Status_Operation_t Status_Operation;
  gate_status_t gate_status;
  uint8_t tecla;
}application_t;

extern application_t application;
/**************************************************************************//**
 * The function is used for Application logic.
 *
 * @param None
 * @returns None
 *
 * Here we print out the first two bytes reported by the sinks as a little
 * endian 16-bits decimal.
 *****************************************************************************/
void report_handler(void);
void em4_handler(void);
void reset_parameters(void);
void battery_read();
void app_init();
void Init_handler();
void radio_handler(void);
packet_error_e packet_data_demount(uint8_t *inData, uint8_t inLen, packet_void_t *packet);
#endif  // APP_PROCESS_H
