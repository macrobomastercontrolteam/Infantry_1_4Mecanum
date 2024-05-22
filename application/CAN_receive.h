/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       can_receive.c/h
  * @brief      there is CAN interrupt function  to receive motor data,
  *             and CAN send function to send motor current to control motor.
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Nov-11-2019     RM              1. support hal lib
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#ifndef CAN_RECEIVE_H
#define CAN_RECEIVE_H

#include "global_inc.h"

#define CHASSIS_CAN hcan1
#define GIMBAL_CAN hcan2

/* CAN send and receive ID */
typedef enum
{
    /*******Chassis CAN IDs********/
    CAN_3508_M1_ID = 0x201,
    CAN_3508_M2_ID = 0x202,
    CAN_3508_M3_ID = 0x203,
    CAN_3508_M4_ID = 0x204,
    CAN_YAW_MOTOR_ID = 0x205,

    /********Gimbal CAN IDs********/
    CAN_PIT_MOTOR_ID = 0x206,

    /********Other CAN IDs: Location depends on Model********/
    // INFANTRY_2023_MECANUM: On gimbal
    // INFANTRY_2023_SWERVE: On chassis
    // SENTRY_2023_MECANUM: On chassis
    CAN_TRIGGER_MOTOR_ID = 0x207,

    CAN_FRICTION_MOTOR_LEFT_ID = 0x205, // friction1
    CAN_FRICTION_MOTOR_RIGHT_ID = 0x208, // friction2
} can_msg_id_e;

typedef enum
{
	MOTOR_INDEX_3508_M1 = 0,
	MOTOR_INDEX_3508_M2,
	MOTOR_INDEX_3508_M3,
	MOTOR_INDEX_3508_M4,
	MOTOR_INDEX_YAW,
	MOTOR_INDEX_PITCH,
	MOTOR_INDEX_TRIGGER,
  MOTOR_INDEX_FRICTION_LEFT,
  MOTOR_INDEX_FRICTION_RIGHT,
	MOTOR_LIST_LENGTH,
} can_motor_id_e;

typedef enum
{
	/*******Tx CAN IDs********/
	CAN_CHASSIS_M3508_TX_ID = 0x200,
	CAN_GIMBAL_ALL_TX_ID = 0x1FF,
  SUPCAP_RX_ID = 0x301,
#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
	CAN_CHASSIS_CONTROLLER_TX_ID = 0x112,
	CAN_CHASSIS_LOAD_SERVO_TX_ID = 0x113,
#endif
} can_other_msg_id_e;

//rm motor data
typedef struct
{
    uint16_t ecd;
    int16_t speed_rpm;
    int16_t given_current;
    uint8_t temperate;
    int16_t last_ecd;
} motor_measure_t;


/**
  * @brief          send control current of motor (0x205, 0x206, 0x207, 0x208)
  * @param[in]      yaw: (0x205) 6020 motor control current, range [-30000,30000] 
  * @param[in]      pitch: (0x206) 6020 motor control current, range [-30000,30000]
  * @param[in]      trigger: (0x207) 2006 motor control current, range [-10000,10000]
  * @param[in]      rev: (0x208) reserve motor control current
  * @retval         none
  */
extern void CAN_cmd_gimbal(int16_t yaw, int16_t pitch, int16_t trigger, int16_t fric1, int16_t fric2);

/**
  * @brief          send CAN packet of ID 0x700, it will set chassis motor 3508 to quick ID setting
  * @param[in]      none
  * @retval         none
  */
extern void CAN_cmd_chassis_reset_ID(void);

/**
  * @brief          send control current or voltage of motor. Refer to can_msg_id_e for motor IDs
  * @param[in]      motor1: (0x201) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor2: (0x202) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor3: (0x203) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor4: (0x204) 3508 motor control current, range [-16384,16384] 
  * Extra for INFANTRY_2023_SWERVE:
  * @param[in]      steer_motor1: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @param[in]      steer_motor2: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @param[in]      steer_motor3: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @param[in]      steer_motor4: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @retval         none
  */
extern void CAN_cmd_chassis(void);

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
extern void CAN_cmd_load_servo(uint8_t fServoSwitch, uint8_t bTrialTimes);
#endif

/**
  * @brief          return the yaw 6020 motor data point
  * @param[in]      none
  * @retval         motor data point
  */
extern const motor_measure_t *get_yaw_gimbal_motor_measure_point(void);

/**
  * @brief          return the pitch 6020 motor data point
  * @param[in]      none
  * @retval         motor data point
  */
extern const motor_measure_t *get_pitch_gimbal_motor_measure_point(void);

/**
  * @brief          return the chassis 3508 motor data point
  * @param[in]      i: motor number,range [0,3]
  * @retval         motor data point
  */
extern const motor_measure_t *get_chassis_motor_measure_point(uint8_t motor_index);

extern motor_measure_t motor_chassis[MOTOR_LIST_LENGTH];

#endif
