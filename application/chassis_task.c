/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis.c/h
  * @brief      chassis control task,
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Nov-11-2019     RM              1. add chassis power control
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "chassis_task.h"
#include "chassis_behaviour.h"

#include "cmsis_os.h"

#include "AHRS_middleware.h"
#include "pid.h"
#include "remote_control.h"
#include "CAN_receive.h"
#include "detect_task.h"
#include "INS_task.h"
#include "chassis_power_control.h"
#include "cv_usart_task.h"

#define STEER_MOTOR_UPSIDE_DOWN_MOUNTING 0

/**
  * @brief          "chassis_move" valiable initialization, include pid initialization, remote control data point initialization, 3508 chassis motors
  *                 data point initialization, gimbal motor data point initialization, and gyro sensor angle point initialization.
  * @param[out]     chassis_move_init: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_init(chassis_move_t *chassis_move_init);

/**
  * @brief          set chassis control mode, mainly call 'chassis_behaviour_mode_set' function
  * @param[out]     chassis_move_mode: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_set_mode(chassis_move_t *chassis_move_mode);

/**
  * @brief          when chassis mode change, some param should be changed, suan as chassis yaw_set should be now chassis yaw
  * @param[out]     chassis_move_transit: "chassis_move" valiable point
  * @retval         none
  */
void chassis_mode_change_control_transit(chassis_move_t *chassis_move_transit);
/**
  * @brief          chassis some measure data updata, such as motor speed, euler angle, and robot speed
  * @param[out]     chassis_move_update: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_feedback_update(chassis_move_t *chassis_move_update);
/**
  * @brief          set chassis control set-point, three movement control value is set by "chassis_behaviour_control_set".
  *                 
  * @param[out]     chassis_move_update: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_set_control(chassis_move_t *chassis_move_control);
/**
  * @brief          control loop, according to control set-point, calculate motor current, 
  *                 motor current will be sentto motor
  * @param[out]     chassis_move_control_loop: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_control_loop(chassis_move_t *chassis_move_control_loop);

#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
void sentry_upper_head_manager(void);
#endif

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
static uint16_t motor_angle_to_ecd_change(fp32 angle);
#endif

#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t chassis_high_water;
#endif
supcap_t cap_message_rx;

chassis_move_t chassis_move;

#if CHASSIS_TEST_MODE
static void J_scope_chassis_test(void)
{
    ;
}
#endif

/**
  * @brief          chassis task, osDelay CHASSIS_CONTROL_TIME_MS (2ms) 
  * @param[in]      pvParameters: null
  * @retval         none
  */
void chassis_task(void const *pvParameters)
{
    uint32_t ulSystemTime = ulSystemTime = osKernelSysTick();
    //wait a time 
    osDelay(CHASSIS_TASK_INIT_TIME);
    //chassis init
    chassis_init(&chassis_move);

    while (ifToeStatusExist(DBUS_TOE, CHASSIS_MOTOR4_TOE, TOE_STATUS_OFFLINE, NULL))
    {
        osDelay(CHASSIS_CONTROL_TIME_MS * 2);
    }

    while (1)
    {
        //set chassis control mode
        chassis_set_mode(&chassis_move);
        //when mode changes, save some data
        chassis_mode_change_control_transit(&chassis_move);
        //chassis data update
        chassis_feedback_update(&chassis_move);
        //set chassis control set-point 
        chassis_set_control(&chassis_move);
        //chassis control pid calculate
        chassis_control_loop(&chassis_move);

        CAN_cmd_chassis();

#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
        sentry_upper_head_manager();
#endif

		osDelayUntil(&ulSystemTime, CHASSIS_CONTROL_TIME_MS);

#if CHASSIS_TEST_MODE
        J_scope_chassis_test();
#endif

#if INCLUDE_uxTaskGetStackHighWaterMark
        chassis_high_water = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}

#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
void sentry_upper_head_manager(void)
{
    if (chassis_move.fUpperHeadEnabled)
	{
        osDelay(1);
		CAN_cmd_upper_head();
	}
}
#endif

/**
  * @brief          "chassis_move" valiable initialization, include pid initialization, remote control data point initialization, 3508 chassis motors
  *                 data point initialization, gimbal motor data point initialization, and gyro sensor angle point initialization.
  * @param[out]     chassis_move_init: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_init(chassis_move_t *chassis_move_init)
{
    if (chassis_move_init == NULL)
    {
        return;
    }

    //chassis drive motor (3508) speed PID
    const static fp32 motor_speed_pid[3] = {M3508_MOTOR_SPEED_PID_KP, M3508_MOTOR_SPEED_PID_KI, M3508_MOTOR_SPEED_PID_KD};
    
    //chassis angle PID
    const static fp32 chassis_yaw_pid[3] = {CHASSIS_FOLLOW_GIMBAL_PID_KP, CHASSIS_FOLLOW_GIMBAL_PID_KI, CHASSIS_FOLLOW_GIMBAL_PID_KD};
    
    const static fp32 chassis_x_order_filter[1] = {CHASSIS_ACCEL_X_NUM};
    const static fp32 chassis_y_order_filter[1] = {CHASSIS_ACCEL_Y_NUM};
    const static fp32 chassis_wz_order_filter[1] = {CHASSIS_ACCEL_WZ_NUM};
    uint8_t i;

    chassis_move_init->chassis_mode = CHASSIS_VECTOR_RAW;
    chassis_move_init->chassis_RC = get_remote_control_point();
    chassis_move_init->chassis_INS_angle = get_INS_angle_point();
    chassis_move_init->chassis_yaw_motor = get_yaw_motor_point();
    chassis_move_init->chassis_pitch_motor = get_pitch_motor_point();
    
    for (i = 0; i < 4; i++)
    {
        chassis_move_init->motor_chassis[i].chassis_motor_measure = get_chassis_motor_measure_point(i);
        PID_init(&chassis_move_init->motor_speed_pid[i], PID_POSITION, motor_speed_pid, M3508_MOTOR_SPEED_PID_MAX_OUT, M3508_MOTOR_SPEED_PID_MAX_IOUT, 0, &raw_err_handler);
    }
    PID_init(&chassis_move_init->chassis_angle_pid, PID_POSITION, chassis_yaw_pid, CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT, CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT, 0, &rad_err_handler);
    
    first_order_filter_init(&chassis_move_init->chassis_cmd_slow_set_vx, CHASSIS_CONTROL_TIME_S, chassis_x_order_filter);
    first_order_filter_init(&chassis_move_init->chassis_cmd_slow_set_vy, CHASSIS_CONTROL_TIME_S, chassis_y_order_filter);
    first_order_filter_init(&chassis_move_init->chassis_cmd_slow_set_wz, CHASSIS_CONTROL_TIME_S, chassis_wz_order_filter);

    chassis_move_init->vx_max_speed = NORMAL_MAX_CHASSIS_SPEED_X;
    chassis_move_init->vx_min_speed = -NORMAL_MAX_CHASSIS_SPEED_X;

    chassis_move_init->vy_max_speed = NORMAL_MAX_CHASSIS_SPEED_Y;
    chassis_move_init->vy_min_speed = -NORMAL_MAX_CHASSIS_SPEED_Y;

    chassis_move_init->vx_rc_sen = chassis_move_init->vx_max_speed / JOYSTICK_HALF_RANGE;
    chassis_move_init->vy_rc_sen = chassis_move_init->vy_max_speed / JOYSTICK_HALF_RANGE;

#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
    chassis_move_init->fUpperHeadEnabled = 0;
#endif

    //update data
    chassis_feedback_update(chassis_move_init);
}

/**
  * @brief          set chassis control mode, mainly call 'chassis_behaviour_mode_set' function
  * @param[out]     chassis_move_mode: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_set_mode(chassis_move_t *chassis_move_mode)
{
    if (chassis_move_mode == NULL)
    {
        return;
    }
    //in file "chassis_behaviour.c"
    chassis_behaviour_mode_set(chassis_move_mode);
}

/**
  * @brief          when chassis mode change, some param should be changed, suan as chassis yaw_set should be now chassis yaw
  * @param[out]     chassis_move_transit: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_mode_change_control_transit(chassis_move_t *chassis_move_transit)
{
    if (chassis_move_transit == NULL)
    {
        return;
    }

    if (chassis_move_transit->last_chassis_mode != chassis_move_transit->chassis_mode)
    {
        switch (chassis_move_transit->chassis_mode)
        {
        case CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW:
        {
          chassis_move_transit->chassis_relative_angle_set = 0.0f;
          break;
        }
        case CHASSIS_VECTOR_SPINNING:
        {
		  // Relative angle implementation for chassis spinning mode
		  // chassis_move_transit->chassis_relative_angle_set = chassis_move_transit->chassis_yaw_motor->relative_angle;
          break;
        }
        case CHASSIS_VECTOR_FOLLOW_CHASSIS_YAW:
        case CHASSIS_VECTOR_NO_FOLLOW_YAW:
        {
          chassis_move_transit->chassis_yaw_set = chassis_move_transit->chassis_yaw;
          break;
        }
        default:
        {
          break;
        }
        }
        chassis_move_transit->last_chassis_mode = chassis_move_transit->chassis_mode;
    }
}

/**
  * @brief          chassis some measure data updata, such as motor speed, euler angle, and robot speed
  * @param[out]     chassis_move_update: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_feedback_update(chassis_move_t *chassis_move_update)
{
    if (chassis_move_update == NULL)
    {
        return;
    }

    uint8_t i = 0;
    for (i = 0; i < 4; i++)
    {
        //update motor speed, accel is differential of speed PID
        chassis_move_update->motor_chassis[i].speed = CHASSIS_MOTOR_RPM_TO_VECTOR_SEN * chassis_move_update->motor_chassis[i].chassis_motor_measure->speed_rpm;
        chassis_move_update->motor_chassis[i].accel = chassis_move_update->motor_speed_pid[i].Dbuf[0] * CHASSIS_CONTROL_FREQUENCE;
    }

#if !(ROBOT_TYPE == INFANTRY_2023_SWERVE)
    //update chassis parameters: vertical speed x, horizontal speed y, rotation speed wz, right hand rule
    chassis_move_update->vx = (-chassis_move_update->motor_chassis[0].speed + chassis_move_update->motor_chassis[1].speed + chassis_move_update->motor_chassis[2].speed - chassis_move_update->motor_chassis[3].speed) * MOTOR_SPEED_TO_CHASSIS_SPEED_VX;
    chassis_move_update->vy = (-chassis_move_update->motor_chassis[0].speed - chassis_move_update->motor_chassis[1].speed + chassis_move_update->motor_chassis[2].speed + chassis_move_update->motor_chassis[3].speed) * MOTOR_SPEED_TO_CHASSIS_SPEED_VY;
    chassis_move_update->wz = (-chassis_move_update->motor_chassis[0].speed - chassis_move_update->motor_chassis[1].speed - chassis_move_update->motor_chassis[2].speed - chassis_move_update->motor_chassis[3].speed) * MOTOR_SPEED_TO_CHASSIS_SPEED_WZ / MOTOR_DISTANCE_TO_CENTER;
#endif

    //calculate chassis euler angle, if chassis have a new gyro sensor,please change this code
    chassis_move_update->chassis_yaw = rad_format(*(chassis_move_update->chassis_INS_angle + INS_YAW_ADDRESS_OFFSET) - chassis_move_update->chassis_yaw_motor->relative_angle);
    chassis_move_update->chassis_pitch = rad_format(*(chassis_move_update->chassis_INS_angle + INS_PITCH_ADDRESS_OFFSET) - chassis_move_update->chassis_pitch_motor->relative_angle);
    chassis_move_update->chassis_roll = *(chassis_move_update->chassis_INS_angle + INS_ROLL_ADDRESS_OFFSET);
}
/**
  * @brief          accroding to the channel value of remote control, calculate chassis vertical and horizontal speed set-point
  *                 
  * @param[out]     vx_set: vertical speed set-point
  * @param[out]     vy_set: horizontal speed set-point
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" valiable point
  * @retval         none
  */
void chassis_rc_to_control_vector(fp32 *vx_set, fp32 *vy_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (chassis_move_rc_to_vector == NULL || vx_set == NULL || vy_set == NULL)
    {
        return;
    }
    
    // change max speed
    if (chassis_move_rc_to_vector->chassis_RC->key.v & KEY_PRESSED_OFFSET_SHIFT)
    {
        chassis_move_rc_to_vector->vx_max_speed = SPRINT_MAX_CHASSIS_SPEED_X;
        chassis_move_rc_to_vector->vy_max_speed = SPRINT_MAX_CHASSIS_SPEED_Y;
    }
    else
    {
        chassis_move_rc_to_vector->vx_max_speed = NORMAL_MAX_CHASSIS_SPEED_X;
        chassis_move_rc_to_vector->vy_max_speed = NORMAL_MAX_CHASSIS_SPEED_Y;
    }
    chassis_move_rc_to_vector->vx_rc_sen = chassis_move_rc_to_vector->vx_max_speed / JOYSTICK_HALF_RANGE;
    chassis_move_rc_to_vector->vy_rc_sen = chassis_move_rc_to_vector->vy_max_speed / JOYSTICK_HALF_RANGE;

    int16_t vx_channel, vy_channel;
    fp32 vx_set_channel, vy_set_channel;
    //deadline, because some remote control need be calibrated,  the value of joystick is not zero in middle place,
    deadband_limit(chassis_move_rc_to_vector->chassis_RC->rc.ch[JOYSTICK_RIGHT_VERTICAL_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    deadband_limit(chassis_move_rc_to_vector->chassis_RC->rc.ch[JOYSTICK_RIGHT_HORIZONTAL_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);

    vx_set_channel = vx_channel * chassis_move_rc_to_vector->vx_rc_sen;
    vy_set_channel = vy_channel * -(chassis_move_rc_to_vector->vy_rc_sen);

    //keyboard set speed set-point
    if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_FRONT_KEY)
    {
        vx_set_channel = chassis_move_rc_to_vector->vx_max_speed;
    }
    else if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_BACK_KEY)
    {
        vx_set_channel = chassis_move_rc_to_vector->vx_min_speed;
    }

    if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_LEFT_KEY)
    {
        vy_set_channel = chassis_move_rc_to_vector->vy_max_speed;
    }
    else if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_RIGHT_KEY)
    {
        vy_set_channel = chassis_move_rc_to_vector->vy_min_speed;
    }

    //first order low-pass replace ramp function, calculate chassis speed set-point to improve control performance
    first_order_filter_cali(&chassis_move_rc_to_vector->chassis_cmd_slow_set_vx, vx_set_channel);
    first_order_filter_cali(&chassis_move_rc_to_vector->chassis_cmd_slow_set_vy, vy_set_channel);
    //stop command, need not slow change, set zero derectly
    if (fabs(vx_set_channel) < CHASSIS_RC_DEADLINE * chassis_move_rc_to_vector->vx_rc_sen)
    {
        chassis_move_rc_to_vector->chassis_cmd_slow_set_vx.out = 0.0f;
    }

    if (fabs(vy_set_channel) < CHASSIS_RC_DEADLINE * chassis_move_rc_to_vector->vy_rc_sen)
    {
        chassis_move_rc_to_vector->chassis_cmd_slow_set_vy.out = 0.0f;
    }

    *vx_set = chassis_move_rc_to_vector->chassis_cmd_slow_set_vx.out;
    *vy_set = chassis_move_rc_to_vector->chassis_cmd_slow_set_vy.out;
}

void chassis_rc_to_swerve_control_vector(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (chassis_move_rc_to_vector == NULL || vx_set == NULL || vy_set == NULL)
    {
        return;
    }
    
    int16_t vx_channel, vy_channel, wz_channel;
    fp32 vx_set_channel, vy_set_channel, wz_set_channel;
    //deadline, because some remote control need be calibrated,  the value of joystick is not zero in middle place,
    deadband_limit(chassis_move_rc_to_vector->chassis_RC->rc.ch[JOYSTICK_RIGHT_VERTICAL_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    deadband_limit(chassis_move_rc_to_vector->chassis_RC->rc.ch[JOYSTICK_RIGHT_HORIZONTAL_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);
    deadband_limit(chassis_move_rc_to_vector->chassis_RC->rc.ch[RC_DIAL_CHANNEL], wz_channel, CHASSIS_RC_DEADLINE);

    // change max speed
    if (chassis_move_rc_to_vector->chassis_RC->key.v & KEY_PRESSED_OFFSET_SHIFT)
    {
        chassis_move_rc_to_vector->vx_max_speed = SPRINT_MAX_CHASSIS_SPEED_X;
        chassis_move_rc_to_vector->vy_max_speed = SPRINT_MAX_CHASSIS_SPEED_Y;
    }
    else
    {
        chassis_move_rc_to_vector->vx_max_speed = NORMAL_MAX_CHASSIS_SPEED_X;
        chassis_move_rc_to_vector->vy_max_speed = NORMAL_MAX_CHASSIS_SPEED_Y;
    }
    chassis_move_rc_to_vector->vx_rc_sen = chassis_move_rc_to_vector->vx_max_speed / JOYSTICK_HALF_RANGE;
    chassis_move_rc_to_vector->vy_rc_sen = chassis_move_rc_to_vector->vy_max_speed / JOYSTICK_HALF_RANGE;

    vx_set_channel = vx_channel * chassis_move_rc_to_vector->vx_rc_sen;
    vy_set_channel = vy_channel * -(chassis_move_rc_to_vector->vy_rc_sen);
    wz_set_channel = wz_channel * -CHASSIS_WZ_RC_SEN;

    //keyboard set speed set-point
    if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_FRONT_KEY)
    {
        vx_set_channel = chassis_move_rc_to_vector->vx_max_speed;
    }
    else if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_BACK_KEY)
    {
        vx_set_channel = chassis_move_rc_to_vector->vx_min_speed;
    }

    if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_LEFT_KEY)
    {
        vy_set_channel = chassis_move_rc_to_vector->vy_max_speed;
    }
    else if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_RIGHT_KEY)
    {
        vy_set_channel = chassis_move_rc_to_vector->vy_min_speed;
    }

    //first order low-pass replace ramp function, calculate chassis speed set-point to improve control performance
    first_order_filter_cali(&chassis_move_rc_to_vector->chassis_cmd_slow_set_vx, vx_set_channel);
    first_order_filter_cali(&chassis_move_rc_to_vector->chassis_cmd_slow_set_vy, vy_set_channel);
    first_order_filter_cali(&chassis_move_rc_to_vector->chassis_cmd_slow_set_wz, wz_set_channel);
    // Do not go back to zero immediately for swerve chassis, which would make steer cmd noisy
    // stop command, need not slow change, set zero derectly
    // if (fabs(vx_set_channel) < CHASSIS_RC_DEADLINE * chassis_move_rc_to_vector->vx_rc_sen)
    // {
    //     chassis_move_rc_to_vector->chassis_cmd_slow_set_vx.out = 0.0f;
    // }

    // if (fabs(vy_set_channel) < CHASSIS_RC_DEADLINE * chassis_move_rc_to_vector->vy_rc_sen)
    // {
    //     chassis_move_rc_to_vector->chassis_cmd_slow_set_vy.out = 0.0f;
    // }

    // wz doesn't need sudden brake

    *vx_set = chassis_move_rc_to_vector->chassis_cmd_slow_set_vx.out;
    *vy_set = chassis_move_rc_to_vector->chassis_cmd_slow_set_vy.out;
    *wz_set = chassis_move_rc_to_vector->chassis_cmd_slow_set_wz.out;
}

/**
  * @brief          set chassis control set-point, three movement control value is set by "chassis_behaviour_control_set".
  * @param[out]     chassis_move_update: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_set_control(chassis_move_t *chassis_move_control)
{

    if (chassis_move_control == NULL)
    {
        return;
    }


    fp32 vx_set = 0.0f, vy_set = 0.0f, angle_set = 0.0f;
    //get three control set-point
    chassis_behaviour_control_set(&vx_set, &vy_set, &angle_set, chassis_move_control);

    //follow gimbal mode
    if (chassis_move_control->chassis_mode == CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW)
    {
        fp32 sin_yaw = 0.0f, cos_yaw = 0.0f;
        //rotate chassis direction, make sure vertial direction follow gimbal 
        sin_yaw = AHRS_sinf(-chassis_move_control->chassis_yaw_motor->relative_angle);
        cos_yaw = AHRS_cosf(-chassis_move_control->chassis_yaw_motor->relative_angle);
        chassis_move_control->vx_set = cos_yaw * vx_set + sin_yaw * vy_set;
        chassis_move_control->vy_set = -sin_yaw * vx_set + cos_yaw * vy_set;
        //set control relative angle  set-point
        chassis_move_control->chassis_relative_angle_set = rad_format(angle_set);
        //calculate ratation speed
        chassis_move_control->wz_set = -PID_calc(&chassis_move_control->chassis_angle_pid, chassis_move_control->chassis_yaw_motor->relative_angle, chassis_move_control->chassis_relative_angle_set, CHASSIS_CONTROL_TIME_S);
    }
    // spinning mode is no-follow-gimbal mode with non-zero angular speed
    else if ((chassis_move_control->chassis_mode == CHASSIS_VECTOR_SPINNING) || (chassis_move_control->chassis_mode == CHASSIS_VECTOR_NO_FOLLOW_YAW))
    {
        //rotate chassis direction, make sure vertial direction follow gimbal 
        fp32 sin_yaw = AHRS_sinf(-chassis_move_control->chassis_yaw_motor->relative_angle);
        fp32 cos_yaw = AHRS_cosf(-chassis_move_control->chassis_yaw_motor->relative_angle);
        chassis_move_control->vx_set = cos_yaw * vx_set + sin_yaw * vy_set;
        chassis_move_control->vy_set = -sin_yaw * vx_set + cos_yaw * vy_set;
        //calculate ratation speed
        chassis_move_control->wz_set = angle_set;
    }
    else if (chassis_move_control->chassis_mode == CHASSIS_VECTOR_FOLLOW_CHASSIS_YAW)
    {
        fp32 delat_angle = 0.0f;
        //set chassis yaw angle set-point
        chassis_move_control->chassis_yaw_set = rad_format(angle_set);
        delat_angle = rad_format(chassis_move_control->chassis_yaw_set - chassis_move_control->chassis_yaw);
        //calculate rotation speed
        chassis_move_control->wz_set = PID_calc(&chassis_move_control->chassis_angle_pid, 0.0f, delat_angle, CHASSIS_CONTROL_TIME_S);
    }
    else if (chassis_move_control->chassis_mode == CHASSIS_VECTOR_RAW)
    {
        //in raw mode, set-point is sent to CAN bus
        chassis_move_control->vx_set = vx_set;
        chassis_move_control->vy_set = vy_set;
        chassis_move_control->wz_set = angle_set;
        chassis_move_control->chassis_cmd_slow_set_vx.out = 0.0f;
        chassis_move_control->chassis_cmd_slow_set_vy.out = 0.0f;
        chassis_move_control->chassis_cmd_slow_set_wz.out = 0.0f;
    }
    //speed limit
    chassis_move_control->vx_set = fp32_constrain(vx_set, chassis_move_control->vx_min_speed, chassis_move_control->vx_max_speed);
    chassis_move_control->vy_set = fp32_constrain(vy_set, chassis_move_control->vy_min_speed, chassis_move_control->vy_max_speed);
}

#if (ROBOT_TYPE == INFANTRY_2023_MECANUM) || (ROBOT_TYPE == INFANTRY_2024_MECANUM) || (ROBOT_TYPE == SENTRY_2023_MECANUM)
/**
  * @brief          four mecanum wheels speed is calculated by three param. 
  * @param[in]      vx_set: vertial speed
  * @param[in]      vy_set: horizontal speed
  * @param[in]      wz_set: rotation speed
  * @param[out]     wheel_speed: four mecanum wheels speed
  * @retval         none
  */
static void chassis_vector_to_mecanum_wheel_speed(const fp32 vx_set, const fp32 vy_set, const fp32 wz_set, fp32 wheel_speed[4])
{
    //because the gimbal is in front of chassis, when chassis rotates, wheel 0 and wheel 1 should be slower and wheel 2 and wheel 3 should be faster
    wheel_speed[0] = -vx_set - vy_set + (CHASSIS_WZ_SET_SCALE - 1.0f) * MOTOR_DISTANCE_TO_CENTER * wz_set;
    wheel_speed[1] = vx_set - vy_set + (CHASSIS_WZ_SET_SCALE - 1.0f) * MOTOR_DISTANCE_TO_CENTER * wz_set;
    wheel_speed[2] = vx_set + vy_set + (-CHASSIS_WZ_SET_SCALE - 1.0f) * MOTOR_DISTANCE_TO_CENTER * wz_set;
    wheel_speed[3] = -vx_set + vy_set + (-CHASSIS_WZ_SET_SCALE - 1.0f) * MOTOR_DISTANCE_TO_CENTER * wz_set;
}
#elif (ROBOT_TYPE == INFANTRY_2023_SWERVE)
/**
  * @brief          four drive wheels' speeds and four steering wheels' angles are calculated by three chassis param. 
  * @param[in]      vx_set: vertial speed (up is positive)
  * @param[in]      vy_set: horizontal speed (remote controller: left is positive; internal calculation: right is positive)
  * @param[in]      wz_set: rotation speed (counter-clockwise is positive)
  * @param[out]     wheel_speed: four drive wheels speed
  * @param[out]     steer_wheel_angle: four steering wheels angle. Angle between positive y-axis and total velocity
  * @retval         none
  */
void chassis_vector_to_wheel_vector(fp32 vx_set, fp32 vy_set, fp32 wz_set, fp32 wheel_speed[4], fp32 steer_wheel_angle[4])
{
    // remote controller: left is positive; internal calculation: right is positive
    vy_set = -vy_set;
    //because the gimbal is behind chassis, when chassis rotates, wheel 0 and wheel 1 should be faster and wheel 2 and wheel 3 should be slower
    //CHASSIS_WZ_SET_SCALE makes a coarse adjustment for that
    fp32 wz_set_adjusted_front_wheels = (1.0f + CHASSIS_WZ_SET_SCALE) * wz_set;
    fp32 tangential_speed_x_front_wheels = wz_set_adjusted_front_wheels * CHASSIS_Y_DIRECTION_HALF_LENGTH; // wz_set * R * cos(theta)
    fp32 tangential_speed_y_front_wheels = wz_set_adjusted_front_wheels * CHASSIS_X_DIRECTION_HALF_LENGTH; // wz_set * R * sin(theta)

    fp32 wz_set_adjusted_rear_wheels = (1.0f - CHASSIS_WZ_SET_SCALE) * wz_set;
    fp32 tangential_speed_x_rear_wheels = wz_set_adjusted_rear_wheels * CHASSIS_Y_DIRECTION_HALF_LENGTH; // wz_set * R * cos(theta)
    fp32 tangential_speed_y_rear_wheels = wz_set_adjusted_rear_wheels * CHASSIS_X_DIRECTION_HALF_LENGTH; // wz_set * R * sin(theta)

    //pairs of velocities represented by (x,y)
    fp32 wheel_velocity[4][2] = {
      {vx_set + tangential_speed_x_front_wheels, vy_set - tangential_speed_y_front_wheels},
      {vx_set - tangential_speed_x_front_wheels, vy_set - tangential_speed_y_front_wheels},
      {vx_set - tangential_speed_x_rear_wheels, vy_set + tangential_speed_y_rear_wheels},
      {vx_set + tangential_speed_x_rear_wheels, vy_set + tangential_speed_y_rear_wheels},
    };

#if STEER_MOTOR_UPSIDE_DOWN_MOUNTING
    wheel_velocity[0][1] = -wheel_velocity[0][1];
    wheel_velocity[1][1] = -wheel_velocity[1][1];
    wheel_velocity[2][1] = -wheel_velocity[2][1];
    wheel_velocity[3][1] = -wheel_velocity[3][1];
#endif

    static fp32 last_steer_wheel_angle_target[4];
    static uint8_t reverse_flag[4];
    uint8_t i;
    uint8_t fNoChange = (fabs(vy_set) <= STEER_TURN_X_SPEED_DEADZONE) && (fabs(vx_set) < STEER_TURN_X_SPEED_DEADZONE) && (fabs(wz_set) < STEER_TURN_W_SPEED_DEADZONE);

    for (i=0;i<4;i++)
    {
      // drive wheel speed
      wheel_speed[i] = sqrt(pow(wheel_velocity[i][0],2) +	pow(wheel_velocity[i][1],2));

      //steering wheel angle
      if (fNoChange){
        steer_wheel_angle[i] = last_steer_wheel_angle_target[i];
      }
      else
      {
        // (https://en.cppreference.com/w/c/numeric/math/atan2)
        // steer_wheel_angle: unit rad; range is [-PI, PI]; positive direction is clockwise
        steer_wheel_angle[i] = atan2f(wheel_velocity[i][1],wheel_velocity[i][0]);

        reverse_flag[i] = (fabs(rad_format(steer_wheel_angle[i] - last_steer_wheel_angle_target[i])) > PI/2);
        if (reverse_flag[i])
        {
          // if angle between last and target is greater than 90 deg, simply reverse drive wheel reduces time to turn
          steer_wheel_angle[i] = rad_format(steer_wheel_angle[i] + PI);
        }
        
        last_steer_wheel_angle_target[i] = steer_wheel_angle[i];
      }

      if (reverse_flag[i])
      {
        wheel_speed[i] = -wheel_speed[i];
      }
    }

    // reverse direction because of special initial direction
    wheel_speed[0] = -wheel_speed[0];
    wheel_speed[3] = -wheel_speed[3];
}
#endif

/**
  * @brief          control loop, according to control set-point, calculate motor current, 
  *                 motor current will be sentto motor
  * @param[out]     chassis_move_control_loop: "chassis_move" valiable point
  * @retval         none
  */
static void chassis_control_loop(chassis_move_t *chassis_move_control_loop)
{
    fp32 max_vector = 0.0f, vector_rate = 0.0f;
    fp32 temp = 0.0f;
    fp32 wheel_speed[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // unit m/s
    uint8_t i = 0;

#if (ROBOT_TYPE == INFANTRY_2023_MECANUM) || (ROBOT_TYPE == INFANTRY_2024_MECANUM) || (ROBOT_TYPE == SENTRY_2023_MECANUM)
    // mecanum chassis inverse kinematics
    chassis_vector_to_mecanum_wheel_speed(chassis_move_control_loop->vx_set,
                                          chassis_move_control_loop->vy_set, chassis_move_control_loop->wz_set, wheel_speed);
#elif (ROBOT_TYPE == INFANTRY_2023_SWERVE)
    // @TODO: hip motor
    // swerve chassis inverse kinematics
    fp32 steer_wheel_angle[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // unit rad
    chassis_vector_to_wheel_vector(chassis_move_control_loop->vx_set, chassis_move_control_loop->vy_set, chassis_move_control_loop->wz_set, wheel_speed, steer_wheel_angle);
#endif

    if (chassis_move_control_loop->chassis_mode == CHASSIS_VECTOR_RAW)
    {
        
        for (i = 0; i < 4; i++)
        {
            chassis_move_control_loop->motor_chassis[i].give_current = (int16_t)(wheel_speed[i]);
        }
        //in raw mode, derectly return
        return;
    }

    for (i = 0; i < 4; i++)
    {
        //calculate the max speed in four wheels, limit the max speed
        chassis_move_control_loop->motor_chassis[i].speed_set = wheel_speed[i];
        temp = fabs(chassis_move_control_loop->motor_chassis[i].speed_set);
        if (max_vector < temp)
        {
            max_vector = temp;
        }

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
        // steer motor encoder value set
        chassis_move_control_loop->steer_motor_chassis[i].target_ecd = motor_angle_to_ecd_change(steer_wheel_angle[i]);
#endif
    }

    if (max_vector > MAX_WHEEL_SPEED)
    {
        vector_rate = MAX_WHEEL_SPEED / max_vector;
        for (i = 0; i < 4; i++)
        {
            chassis_move_control_loop->motor_chassis[i].speed_set *= vector_rate;
        }
    }

    //calculate pid
    for (i = 0; i < 4; i++)
    {
        PID_calc(&chassis_move_control_loop->motor_speed_pid[i], chassis_move_control_loop->motor_chassis[i].speed, chassis_move_control_loop->motor_chassis[i].speed_set, CHASSIS_CONTROL_TIME_S);
    }

    chassis_power_control(chassis_move_control_loop);

    for (i = 0; i < 4; i++)
    {
        chassis_move_control_loop->motor_chassis[i].give_current = (int16_t)(chassis_move_control_loop->motor_speed_pid[i].out);
    }
}

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
/**
 * @brief Convert motor angle from radian to encoder unit
 * Requirements:
 *    0 rad = 0 ecd
 *    input and output increase in the same clockwise direction
 * @param[in] angle range [-PI, PI]
 * @param[in] ecd range [0, ECD_RANGE-1]
 */
static uint16_t motor_angle_to_ecd_change(fp32 angle)
{
    return (uint16_t)(loop_fp32_constrain(angle, 0.0f, 2 * PI) * MOTOR_RAD_TO_ECD);
}
#endif
