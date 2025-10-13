/*
 * pckDataStructure.h
 *
 *  Created on: 9 de ago de 2024
 *      Author: diego.marinho
 */

#ifndef ET001_PCKDATASTRUCTURE_H_
#define ET001_PCKDATASTRUCTURE_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * defines
 */

#define PROP_ASYNC				(1 << 0)
#define PROP_DISCONNECT 		(1 << 1)

/*
 * Enumerates
 */
typedef enum SensorCmd_e{
  TX = 16,
  REGISTRATION = 21,
  MOTION_DETECTED = 22,
  SETUP_IVP,
	KEEP_ALIVE,
	STATUS_CENTRAL,
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
  CONTROL = 0,
  MOTION_DETECT,
  OPEN_CLOSE_DETECT,
  GATE
}Type;

typedef enum{
  LONG_RANGE = 0,
  MID_RANGE
}Range_t;

typedef struct{
		SensorCmd_e cmd;
		uint8_t data[8];
		uint8_t len;
}packet_void_t;

#endif /* ET001_PCKDATASTRUCTURE_H_ */
