/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis.c/h
  * @brief      chassis control task,
  *             ���̿�������
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. ���
  *  V1.1.0     Nov-11-2019     RM              1. add chassis power control
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#ifndef CHASSIS_TASK_H
#define CHASSIS_TASK_H
#include "struct_typedef.h"
#include "CAN_receive.h"
#include "gimbal_task.h"
#include "pid.h"
#include "remote_control.h"
#include "user_lib.h"

//in the beginning of task ,wait a time
//����ʼ����һ��ʱ��
#define CHASSIS_TASK_INIT_TIME 357.0f
#define BIPED_CHASSIS_TASK_INIT_TIME 1000.0f

//chassis forward or back max speed
#define NORMAL_MAX_CHASSIS_SPEED_X 2.5f
#define NORMAL_MAX_CHASSIS_SPEED_YAW 2.5f
#define MAX_CHASSIS_ROLL 0.4f

#define CHASSIS_ACCEL_X_NUM 0.1666666667f
#define CHASSIS_ACCEL_Y_NUM 0.3333333333f

//rocker value deadline
//ҡ������
#define CHASSIS_RC_DEADLINE 10.0f

#define CHASSIS_JSCOPE_DEBUG 1

//chassis task control time  2ms
//����������Ƽ�� 2ms
#define CHASSIS_CONTROL_TIME_MS 11.0f
//chassis task control time 0.002s
//����������Ƽ�� 0.002s
#define CHASSIS_CONTROL_TIME 0.01f
//chassis control frequence, no use now.
//�����������Ƶ�ʣ���δʹ�������
#define CHASSIS_CONTROL_FREQUENCE 100.0f
//chassis 3508 max motor control current
//����3508���can���͵���ֵ
#define MAX_MOTOR_CAN_CURRENT 16000.0f
//chassis 6020 max motor control voltage
//����6020���can���͵�ѹֵ
#define MAX_MOTOR_CAN_VOLTAGE 20000.0f

//rocker value (max 660) change to vertial speed (m/s) 
//ң����ǰ��ҡ�ˣ�max 660��ת���ɳ���ǰ���ٶȣ�m/s���ı���
#define CHASSIS_DIS_RC_SPEED 1.0f // 1m/s
#define CHASSIS_DIS_KEYBOARD_INC (CHASSIS_DIS_RC_SPEED/1000.0f*CHASSIS_CONTROL_TIME_MS)
#define CHASSIS_DIS_RC_SEN_INC (CHASSIS_DIS_RC_SPEED/1000.0f*CHASSIS_CONTROL_TIME_MS/JOYSTICK_HALF_RANGE)

#define CHASSIS_YAW_RC_SEN_INC (NORMAL_MAX_CHASSIS_SPEED_YAW/1000.0f*CHASSIS_CONTROL_TIME_MS/JOYSTICK_HALF_RANGE)

// rise speed is L0 range per CHASSIS_CONTROL_TIME_MS per joystick_range
#define LEG_L0_RC_RISE_TIME_MS 1000.0f
#define LEG_L0_KEYBOARD_INC (LEG_L0_RANGE/LEG_L0_RC_RISE_TIME_MS*CHASSIS_CONTROL_TIME_MS)
#define LEG_L0_RC_SEN_INC (LEG_L0_RANGE/LEG_L0_RC_RISE_TIME_MS*CHASSIS_CONTROL_TIME_MS/JOYSTICK_FULL_RANGE)

#define CHASSIS_ROLL_RC_CHANGE_TIME_MS 2000.0f
#define CHASSIS_ROLL_KEYBOARD_INC (MAX_CHASSIS_ROLL/CHASSIS_ROLL_RC_CHANGE_TIME_MS*CHASSIS_CONTROL_TIME_MS)
#define CHASSIS_ROLL_RC_SEN_INC (MAX_CHASSIS_ROLL/CHASSIS_ROLL_RC_CHANGE_TIME_MS*CHASSIS_CONTROL_TIME_MS/JOYSTICK_FULL_RANGE)

//press the key, chassis will swing
//����ҡ�ڰ���
#define SWING_KEY KEY_PRESSED_OFFSET_CTRL
//chassi forward, back, left, right key
//����ǰ�����ҿ��ư���
#define CHASSIS_RISE_PLATFORM_KEY KEY_PRESSED_OFFSET_W
#define CHASSIS_LOWER_PLATFORM_KEY KEY_PRESSED_OFFSET_S
#define CHASSIS_LEAN_LEFT_KEY KEY_PRESSED_OFFSET_A
#define CHASSIS_LEAN_RIGHT_KEY KEY_PRESSED_OFFSET_D

//single chassis motor max speed
//�������̵������ٶ�
#define MAX_WHEEL_SPEED 4.0f

// Arbitrary offsets between chassis rotational center and centroid
#if defined(INFANTRY_1) || defined(INFANTRY_2) || defined(INFANTRY_3) || defined(SENTRY_1)
// slip ring is at the center of chassis
#define CHASSIS_WZ_SET_SCALE 0.0f
#else
// Offset for the official model
#define CHASSIS_WZ_SET_SCALE 0.1f
#endif

//when chassis is not set to move, swing max angle
//ҡ��ԭ�ز���ҡ�����Ƕ�(rad)
#define SWING_NO_MOVE_ANGLE 0.7f
//when chassis is set to move, swing max angle
//ҡ�ڹ��̵����˶����Ƕ�(rad)
#define SWING_MOVE_ANGLE 0.31415926535897932384626433832795f

// //chassis motor speed PID
// //���̵���ٶȻ�PID
// #define M3508_MOTOR_SPEED_PID_KP 15000.0f
// #define M3508_MOTOR_SPEED_PID_KI 10.0f
// #define M3508_MOTOR_SPEED_PID_KD 0.0f
// #define M3508_MOTOR_SPEED_PID_MAX_OUT MAX_MOTOR_CAN_CURRENT
// #define M3508_MOTOR_SPEED_PID_MAX_IOUT 2000.0f

// //chassis follow angle PID
// //������ת����PID
// #define CHASSIS_FOLLOW_GIMBAL_PID_KP 12.0f
// #define CHASSIS_FOLLOW_GIMBAL_PID_KI 0.0f
// #define CHASSIS_FOLLOW_GIMBAL_PID_KD 0.1f
// #define CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT 6.0f
// #define CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT 0.2f

typedef enum
{
  CHASSIS_VECTOR_NO_FOLLOW_YAW,       //chassis will have rotation speed control. ��������ת�ٶȿ���
  CHASSIS_VECTOR_RAW,                 //control-current will be sent to CAN bus derectly.
  CHASSIS_VECTOR_SPINNING,            //spinning chassis
} chassis_mode_e;

typedef struct
{
  const motor_measure_t *chassis_motor_measure;
  fp32 accel;
  fp32 speed;
  fp32 speed_set;
  fp32 give_torque;
} chassis_motor_t;

#if defined(INFANTRY_3)
typedef struct
{
  uint16_t target_ecd; ///< unit encoder unit; range is [0, 8191]; positive direction is clockwise; forward direction of chassis is 0 ecd
} chassis_steer_motor_t;
#endif

typedef struct
{
  const RC_ctrl_t *chassis_RC;               //����ʹ�õ�ң����ָ��, the point to remote control
  // const gimbal_motor_t *chassis_yaw_motor;   //will use the relative angle of yaw gimbal motor to calculate the euler angle.����ʹ�õ�yaw��̨�������ԽǶ���������̵�ŷ����.
  // const gimbal_motor_t *chassis_pitch_motor; //will use the relative angle of pitch gimbal motor to calculate the euler angle.����ʹ�õ�pitch��̨�������ԽǶ���������̵�ŷ����
  const fp32 *chassis_INS_angle;             //the point to the euler angle of gyro sensor.��ȡ�����ǽ������ŷ����ָ��
  const fp32 *chassis_INS_speed;             //the point to the euler angular speed of gyro sensor
  // const fp32 *chassis_INS_accel;
  chassis_mode_e chassis_mode;               //state machine. ���̿���״̬��
  chassis_mode_e last_chassis_mode;          //last state machine.�����ϴο���״̬��
  // pid_type_def chassis_angle_pid;              //follow angle PID.���̸���Ƕ�pid

  fp32 vx;                          //chassis vertical speed, positive means forward,unit m/s. �����ٶ� ǰ������ ǰΪ������λ m/s
  fp32 vy;                          //chassis horizontal speed, positive means letf,unit m/s.�����ٶ� ���ҷ��� ��Ϊ��  ��λ m/s
  fp32 wz;                          //chassis rotation speed, positive means counterclockwise,unit rad/s.������ת���ٶȣ���ʱ��Ϊ�� ��λ rad/s
  fp32 vx_set;                      //chassis set vertical speed,positive means forward,unit m/s.�����趨�ٶ� ǰ������ ǰΪ������λ m/s
  // fp32 vy_set;                      //chassis set horizontal speed,positive means left,unit m/s.�����趨�ٶ� ���ҷ��� ��Ϊ������λ m/s
  fp32 wz_set;                      //chassis set rotation speed,positive means counterclockwise,unit rad/s.�����趨��ת���ٶȣ���ʱ��Ϊ�� ��λ rad/s
  fp32 chassis_relative_angle;      //the relative angle between chassis and gimbal.��������̨����ԽǶȣ���λ rad
  fp32 chassis_relative_angle_set;  //the set relative angle.���������̨���ƽǶ�

  // fp32 vx_max_speed;  //max forward speed, unit m/s.ǰ����������ٶ� ��λm/s
  // fp32 vx_min_speed;  //max backward speed, unit m/s.���˷�������ٶ� ��λm/s
  // fp32 vy_max_speed;  //max letf speed, unit m/s.��������ٶ� ��λm/s
  // fp32 vy_min_speed;  //max right speed, unit m/s.�ҷ�������ٶ� ��λm/s

} chassis_move_t;

/**
  * @brief          chassis task, osDelay CHASSIS_CONTROL_TIME_MS (2ms) 
  * @param[in]      pvParameters: null
  * @retval         none
  */
/**
  * @brief          �������񣬼�� CHASSIS_CONTROL_TIME_MS 2ms
  * @param[in]      pvParameters: ��
  * @retval         none
  */
extern void chassis_task(void const *pvParameters);

/**
  * @brief          accroding to the channel value of remote control, calculate chassis vertical and horizontal speed set-point
  *                 
  * @param[out]     vx_set: vertical speed set-point
  * @param[out]     vy_set: horizontal speed set-point
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" valiable point
  * @retval         none
  */
/**
  * @brief          ����ң����ͨ��ֵ����������ͺ����ٶ�
  *                 
  * @param[out]     vx_set: �����ٶ�ָ��
  * @param[out]     vy_set: �����ٶ�ָ��
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" ����ָ��
  * @retval         none
  */
extern void chassis_rc_to_control_vector(chassis_move_t *chassis_move_rc_to_vector);

fp32 abs_err_handler(fp32 set, fp32 ref);

extern chassis_move_t chassis_move;

#endif
