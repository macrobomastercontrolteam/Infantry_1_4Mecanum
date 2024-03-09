#ifndef _ROBOT_ARM_TASK_H
#define _ROBOT_ARM_TASK_H

#include "CAN_receive.h"
#include "user_lib.h"
#include "pid.h"

/*********** Robot Configs Start ***********/
// @TODO
#define DISABLE_JOINT_0_POWER 0
#define DISABLE_JOINT_1_POWER 0
#define DISABLE_JOINT_2_POWER 0
#define DISABLE_JOINT_3_POWER 0
#define DISABLE_JOINT_4_POWER 0
#define DISABLE_JOINT_5_POWER 0
#define DISABLE_JOINT_6_POWER 0

#define ROBOT_ARM_JSCOPE_DEBUG 1
/*********** Robot Configs End ***********/

#define ROBOT_ARM_TASK_INIT_TIME 500.0f
// task loop delay time
#define ROBOT_ARM_CONTROL_TIME_MS 5.0f
#define ROBOT_ARM_CONTROL_TIME_S (ROBOT_ARM_CONTROL_TIME_MS / 1000.0f)

#define ARM_JOINT_CLEARANCE 0.08f

#define ARM_JOINT_0_ANGLE_MIN (-PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_0_ANGLE_MAX (PI - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_0_ANGLE_HOME 0.0f

#define ARM_JOINT_1_ANGLE_MIN (-30.0f / 180.0f * PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_1_ANGLE_MAX (35.0f / 180.0f * PI - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_1_ANGLE_HOME ARM_JOINT_1_ANGLE_MAX

#define ARM_JOINT_2_ANGLE_MIN (-150.0f / 180.0f * PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_2_ANGLE_MAX (0.0f - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_2_ANGLE_HOME ARM_JOINT_2_ANGLE_MIN

#define ARM_JOINT_3_ANGLE_MIN (-80.0f / 180.0f * PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_3_ANGLE_MAX (80.0f / 180.0f * PI - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_3_ANGLE_HOME 0.0f

#define ARM_JOINT_4_ANGLE_MIN (-0.5f * PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_4_ANGLE_MAX (0.5f * PI - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_4_ANGLE_HOME 0.0f

#define ARM_JOINT_5_ANGLE_MIN (10.0f / 180.0f * PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_5_ANGLE_MAX (170.0f / 180.0f * PI - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_5_ANGLE_HOME ARM_JOINT_5_ANGLE_MAX

#define ARM_JOINT_6_ANGLE_MIN (-PI + ARM_JOINT_CLEARANCE)
#define ARM_JOINT_6_ANGLE_MAX (PI - ARM_JOINT_CLEARANCE)
#define ARM_JOINT_6_ANGLE_HOME 0.0f

#define ARM_END_EFFECTOR_ROLL_MIN (-PI + ARM_JOINT_CLEARANCE)
#define ARM_END_EFFECTOR_ROLL_MAX (PI - ARM_JOINT_CLEARANCE)
#define ARM_END_EFFECTOR_ROLL_HOME 0.0f

#define ARM_END_EFFECTOR_PITCH_MIN (-PI + ARM_JOINT_CLEARANCE)
#define ARM_END_EFFECTOR_PITCH_MAX (PI - ARM_JOINT_CLEARANCE)
#define ARM_END_EFFECTOR_PITCH_HOME 0.0f

#define ARM_END_EFFECTOR_YAW_MIN (-PI + ARM_JOINT_CLEARANCE)
#define ARM_END_EFFECTOR_YAW_MAX (PI - ARM_JOINT_CLEARANCE)
#define ARM_END_EFFECTOR_YAW_HOME 0.0f

#define ARM_END_EFFECTOR_X_MIN (-0.5f)
#define ARM_END_EFFECTOR_X_MAX (0.5f)
#define ARM_END_EFFECTOR_X_HOME 0.0f

#define ARM_END_EFFECTOR_Y_MIN (-0.5f)
#define ARM_END_EFFECTOR_Y_MAX (0.5f)
#define ARM_END_EFFECTOR_Y_HOME 0.0f

#define ARM_END_EFFECTOR_Z_MIN (-0.5f)
#define ARM_END_EFFECTOR_Z_MAX (0.5f)
#define ARM_END_EFFECTOR_Z_HOME 0.0f

#define JOINT_6_6020_ANGLE_PID_KP 15.0f
#define JOINT_6_6020_ANGLE_PID_KI 0.00f
#define JOINT_6_6020_ANGLE_PID_KD 0.0f
#define JOINT_6_6020_ANGLE_PID_MAX_OUT 10.0f
#define JOINT_6_6020_ANGLE_PID_MAX_IOUT 0.0f

#define JOINT_6_6020_SPEED_PID_KP 1000.0f // pitch starts shaking at 1200
#define JOINT_6_6020_SPEED_PID_KI 0.0f
#define JOINT_6_6020_SPEED_PID_KD 0.0f
#define JOINT_6_6020_SPEED_PID_MAX_OUT 30000.0f
#define JOINT_6_6020_SPEED_PID_MAX_IOUT 10000.0f

typedef enum
{
	ARM_STATE_ZERO_FORCE = 0,
	ARM_STATE_MOVING,
	ARM_STATE_FIXED,
} robot_arm_state_e;

typedef struct
{
	fp32 time_step_s; // second
	uint32_t time_ms; // millisecond
	pid_type_def joint_6_6020_angle_pid;
	pid_type_def joint_6_6020_speed_pid;

	fp32 joint_angle_target[7];
	robot_arm_state_e arm_state;
	uint8_t fMovCmded;
	uint8_t fHoming;
	
	const fp32 *arm_INS_angle;
	const fp32 *arm_INS_speed;
	// const fp32 *arm_INS_accel;
    variable_status_t yaw, pitch, roll;
    // fp32 accel_x, accel_y, accel_z;
} robot_arm_t;

extern const fp32 joint_angle_min[7];
extern const fp32 joint_angle_max[7];
extern const fp32 joint_angle_home[7];
extern void robot_arm_task(void const *pvParameters);
void robot_arm_return_to_center(uint8_t _start, uint8_t _end);

extern robot_arm_t robot_arm;

#endif /* _ROBOT_ARM_TASK_H */
