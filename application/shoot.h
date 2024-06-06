/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. Done
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#ifndef SHOOT_H
#define SHOOT_H
#include "global_inc.h"

#include "CAN_receive.h"
#include "gimbal_task.h"
#include "remote_control.h"
#include "user_lib.h"
#include "chassis_task.h"

#define SHOOT_CONTROL_TIME_MS GIMBAL_CONTROL_TIME_MS
#define SHOOT_CONTROL_TIME_S GIMBAL_CONTROL_TIME_S

#define SHOOT_ON_KEYBOARD           KEY_PRESSED_OFFSET_Q
#define SHOOT_OFF_KEYBOARD          KEY_PRESSED_OFFSET_E

// After the shooting is enabled, the bullet is continuously fired for a period of time, used to clear the bullet
#define RC_S_LONG_TIME              800

#if (ROBOT_TYPE == INFANTRY_2023_MECANUM) || (ROBOT_TYPE == INFANTRY_2024_MECANUM)
#define TRIGGER_MOTOR_TO_WHEEL_GEAR_RATIO  1.0f
#elif (ROBOT_TYPE == INFANTRY_2023_SWERVE)
#define TRIGGER_MOTOR_TO_WHEEL_GEAR_RATIO  (58.0f / 24.0f)
#elif (ROBOT_TYPE == SENTRY_2023_MECANUM)
#define TRIGGER_MOTOR_TO_WHEEL_GEAR_RATIO  1.0f
#endif

#define TRIGGER_MOTOR_GEAR_RATIO  36.0f
#define TRIGGER_MOTOR_RPM_TO_SPEED  (2.0f * PI / 60.0f / TRIGGER_MOTOR_GEAR_RATIO)
#define TRIGGER_MOTOR_ECD_TO_ANGLE  (2.0f * PI / (float)ECD_RANGE / TRIGGER_MOTOR_GEAR_RATIO / TRIGGER_MOTOR_TO_WHEEL_GEAR_RATIO)
#define FULL_COUNT                  18

#define FRICTION_MOTOR_RADIUS 0.03f
#define SPEED_COMPENSATION_RATIO 0.87f
#define FRICTION_MOTOR_RPM_TO_SPEED (2.0f * PI / 60.0f * (FRICTION_MOTOR_RADIUS * SPEED_COMPENSATION_RATIO))
#define FRICTION_MOTOR_SPEED_TO_RPM (1.0f / FRICTION_MOTOR_RPM_TO_SPEED)
#define FRICTION_MOTOR_SPEED_THRESHOLD 0.9f // 10% tolerance

// max speed of M3508 is 26.99m/s for one motor, 26.2m/s for one motor during test
#if ENABLE_SHOOT_REDUNDANT_SWITCH
#define FRICTION_MOTOR_SPEED  25.0f
#else
#define FRICTION_MOTOR_SPEED  1.0f
#endif

#define SEMI_AUTO_FIRE_TRIGGER_SPEED 10.0f
#define AUTO_FIRE_TRIGGER_SPEED      15.0f
#define READY_TRIGGER_SPEED         5.0f

#define KEY_OFF_JUGUE_TIME          500
#define SWITCH_TRIGGER_ON           0
#define SWITCH_TRIGGER_OFF          1

#define BLOCK_TRIGGER_SPEED         1.0f
#define IDLE_TRIGGER_SPEED          2.0f
#define BLOCK_TIME                  700
#define REVERSE_TIME                500
#define REVERSE_SPEED_LIMIT         13.0f

#if (ROBOT_TYPE == INFANTRY_2023_MECANUM) || (ROBOT_TYPE == INFANTRY_2024_MECANUM)
#define TRIGGER_ANGLE_INCREMENT     (PI/8.0f)
#elif (ROBOT_TYPE == INFANTRY_2023_SWERVE)
#define TRIGGER_ANGLE_INCREMENT     (PI/12.0f)
#elif (ROBOT_TYPE == SENTRY_2023_MECANUM)
#define TRIGGER_ANGLE_INCREMENT     (PI/9.0f)
#endif

#define TRIGGER_ANGLE_PID_KP        800.0f
#define TRIGGER_ANGLE_PID_KI        500.0f
#define TRIGGER_ANGLE_PID_KD        0.002f

#define TRIGGER_BULLET_PID_MAX_OUT  10000.0f
#define TRIGGER_BULLET_PID_MAX_IOUT 9000.0f

#if (ROBOT_TYPE == INFANTRY_2023_MECANUM) || (ROBOT_TYPE == INFANTRY_2024_MECANUM)
#define REVERSE_TRIGGER_DIRECTION 1
#else
#define REVERSE_TRIGGER_DIRECTION 0
#endif

#define SHOOT_HEAT_REMAIN_VALUE     50

//Frictional wheel 1 PID
#define FRICTION_1_SPEED_PID_KP        20.0f
#define FRICTION_1_SPEED_PID_KI        0.0f
#define FRICTION_1_SPEED_PID_KD        0.0f
#define FRICTION_1_SPEED_PID_MAX_OUT   MAX_MOTOR_CAN_CURRENT
#define FRICTION_1_SPEED_PID_MAX_IOUT  200.0f

//Frictional wheel 2 PID
#define FRICTION_2_SPEED_PID_KP        20.0f
#define FRICTION_2_SPEED_PID_KI        0.0f
#define FRICTION_2_SPEED_PID_KD        0.0f
#define FRICTION_2_SPEED_PID_MAX_OUT   MAX_MOTOR_CAN_CURRENT
#define FRICTION_2_SPEED_PID_MAX_IOUT  200.0f

typedef enum
{
    SHOOT_STOP = 0,
    SHOOT_READY_FRIC,
    SHOOT_READY_TRIGGER,
    SHOOT_READY,
    SHOOT_SEMI_AUTO_FIRE,
    SHOOT_AUTO_FIRE,
} shoot_mode_e;


typedef struct
{
    shoot_mode_e shoot_mode;
    uint8_t fIsCvControl;
    const RC_ctrl_t *shoot_rc;

    int16_t fric1_given_current;
    int16_t fric2_given_current;

    pid_type_def friction_motor1_pid;
    fp32 friction_motor1_rpm_set;
    fp32 friction_motor1_rpm;
    // fp32 friction_motor1_angle;

    pid_type_def friction_motor2_pid;
    fp32 friction_motor2_rpm_set;
    fp32 friction_motor2_rpm;
    // fp32 friction_motor2_angle;

	pid_type_def trigger_motor_pid;
    fp32 trigger_speed_set;
    fp32 speed;
    fp32 speed_set;
    fp32 angle;
    fp32 set_angle;
    int16_t given_current;
    int8_t ecd_count;
    
    bool_t press_l;
    bool_t press_r;
    bool_t last_press_l;
    bool_t last_press_r;
    uint16_t shoot_hold_time;

    uint16_t block_time;
    uint16_t reverse_time;
    bool_t move_flag;

    bool_t key;
    // uint8_t key_time;

    uint16_t heat_limit;
    uint16_t heat;
} shoot_control_t;

// because the shooting and gimbal use the same can id, the shooting task is also executed in the gimbal task
extern void shoot_init(void);
extern int16_t shoot_control_loop(void);

extern shoot_control_t shoot_control;

#endif
