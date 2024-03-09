#include "robot_arm_task.h"
#include "INS_task.h"
#include "cmsis_os.h"
#include "detect_task.h"

void robot_arm_init(void);
void robot_arm_status_update(void);
void robot_arm_control(void);
void robot_arm_state_transition(void);

const fp32 joint_angle_min[7] = {ARM_JOINT_0_ANGLE_MIN, ARM_JOINT_1_ANGLE_MIN, ARM_JOINT_2_ANGLE_MIN, ARM_JOINT_3_ANGLE_MIN, ARM_JOINT_4_ANGLE_MIN, ARM_JOINT_5_ANGLE_MIN, ARM_JOINT_6_ANGLE_MIN};
const fp32 joint_angle_max[7] = {ARM_JOINT_0_ANGLE_MAX, ARM_JOINT_1_ANGLE_MAX, ARM_JOINT_2_ANGLE_MAX, ARM_JOINT_3_ANGLE_MAX, ARM_JOINT_4_ANGLE_MAX, ARM_JOINT_5_ANGLE_MAX, ARM_JOINT_6_ANGLE_MAX};
const fp32 joint_angle_home[7] = {ARM_JOINT_0_ANGLE_HOME, ARM_JOINT_1_ANGLE_HOME, ARM_JOINT_2_ANGLE_HOME, ARM_JOINT_3_ANGLE_HOME, ARM_JOINT_4_ANGLE_HOME, ARM_JOINT_5_ANGLE_HOME, ARM_JOINT_6_ANGLE_HOME};
const fp32 arm_end_min[6] = {ARM_END_EFFECTOR_ROLL_MIN, ARM_END_EFFECTOR_PITCH_MIN, ARM_END_EFFECTOR_YAW_MIN, ARM_END_EFFECTOR_X_MIN, ARM_END_EFFECTOR_Y_MIN, ARM_END_EFFECTOR_Z_MIN};
const fp32 arm_end_max[6] = {ARM_END_EFFECTOR_ROLL_MAX, ARM_END_EFFECTOR_PITCH_MAX, ARM_END_EFFECTOR_YAW_MAX, ARM_END_EFFECTOR_X_MAX, ARM_END_EFFECTOR_Y_MAX, ARM_END_EFFECTOR_Z_MAX};
const fp32 arm_end_home[6] = {ARM_END_EFFECTOR_ROLL_HOME, ARM_END_EFFECTOR_PITCH_HOME, ARM_END_EFFECTOR_YAW_HOME, ARM_END_EFFECTOR_X_HOME, ARM_END_EFFECTOR_Y_HOME, ARM_END_EFFECTOR_Z_HOME};

const fp32 yaw_offset = 0;
const fp32 pitch_offset = 0;
const fp32 roll_offset = 0;

#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t robot_arm_high_water;
#endif

robot_arm_t robot_arm;
fp32 robot_arm_task_loop_delay;

#if ROBOT_ARM_JSCOPE_DEBUG
uint32_t loop_delay = 0;
static void jscope_robot_arm_test(void)
{
	loop_delay = HAL_GetTick() - robot_arm.time_ms;
}
#endif

void robot_arm_task(void const *pvParameters)
{
	osDelay(ROBOT_ARM_TASK_INIT_TIME);

	robot_arm_init();
	osDelay(ROBOT_ARM_TASK_INIT_TIME);
	robot_arm_status_update();
	// wait_until_motors_online();

	while (1)
	{
		robot_arm_status_update();
		robot_arm_state_transition();
		robot_arm_control();

		robot_arm_task_loop_delay = xTaskGetTickCount() - robot_arm.time_ms;
		if (robot_arm_task_loop_delay < ROBOT_ARM_CONTROL_TIME_MS)
		{
			osDelay(ROBOT_ARM_CONTROL_TIME_MS - robot_arm_task_loop_delay);
		}

#if ROBOT_ARM_JSCOPE_DEBUG
		jscope_robot_arm_test();
#endif

#if INCLUDE_uxTaskGetStackHighWaterMark
		robot_arm_high_water = uxTaskGetStackHighWaterMark(NULL);
#endif
	}
}

void robot_arm_state_transition(void)
{
	// safety guard
	if (is_error_exist_in_range(CHASSIS_CONTROLLER_TOE, JOINT_6_TOE))
	{
		robot_arm.arm_state = ARM_STATE_ZERO_FORCE;
	}
	else if (robot_arm.fMovCmded)
	{
		robot_arm.arm_state = ARM_STATE_MOVING;
	}
	
	static robot_arm_state_e prev_arm_state = ARM_STATE_ZERO_FORCE;
	if (prev_arm_state != robot_arm.arm_state)
	{
		if ((prev_arm_state == ARM_STATE_ZERO_FORCE) || (robot_arm.arm_state == ARM_STATE_ZERO_FORCE))
		{
			CAN_cmd_switch_motor_power(prev_arm_state == ARM_STATE_ZERO_FORCE);
		}

		switch (robot_arm.arm_state)
		{
			case ARM_STATE_ZERO_FORCE:
			case ARM_STATE_FIXED:
			{
				robot_arm.fHoming = 0;
				break;
			}
			case ARM_STATE_MOVING:
			default:
			{
				break;
			}
		}
		prev_arm_state = robot_arm.arm_state;
	}
}

void robot_arm_control(void)
{
	switch (robot_arm.arm_state)
	{
		case ARM_STATE_MOVING:
		{
			arm_joints_cmd_position(robot_arm.joint_angle_target, ROBOT_ARM_CONTROL_TIME_S);
			
			if (is_joint_target_reached(robot_arm.joint_angle_target, 0.05f))
			{
				robot_arm.fMovCmded = 0;
				if (robot_arm.fHoming)
				{
					robot_arm.arm_state = ARM_STATE_ZERO_FORCE;
				}
				else
				{
					robot_arm.arm_state = ARM_STATE_FIXED;
				}
			}
			break;
		}
		case ARM_STATE_FIXED:
		{
			arm_joints_cmd_position(robot_arm.joint_angle_target, ROBOT_ARM_CONTROL_TIME_S);
			break;
		}
		case ARM_STATE_ZERO_FORCE:
		default:
		{
			fp32 all_0_torque[7] = {0};
			arm_joints_cmd_torque(all_0_torque);
			break;
		}
	}
}

void robot_arm_status_update(void)
{
	robot_arm.time_ms = xTaskGetTickCount();

	robot_arm.roll.now = *(robot_arm.arm_INS_angle + INS_ROLL_ADDRESS_OFFSET);
	robot_arm.roll.now -= roll_offset;
	robot_arm.roll.dot = *(robot_arm.arm_INS_speed + INS_GYRO_X_ADDRESS_OFFSET);
	robot_arm.roll.last = robot_arm.roll.now;

	robot_arm.pitch.now = *(robot_arm.arm_INS_angle + INS_PITCH_ADDRESS_OFFSET);
	robot_arm.pitch.now -= pitch_offset;
	robot_arm.pitch.dot = *(robot_arm.arm_INS_speed + INS_GYRO_Y_ADDRESS_OFFSET);
	robot_arm.pitch.last = robot_arm.pitch.now;

	robot_arm.yaw.now = *(robot_arm.arm_INS_angle + INS_YAW_ADDRESS_OFFSET);
	robot_arm.yaw.now -= yaw_offset;
	robot_arm.yaw.dot = *(robot_arm.arm_INS_speed + INS_GYRO_Z_ADDRESS_OFFSET);
	robot_arm.yaw.last = robot_arm.yaw.now;

	// robot_arm.accel_x = *(robot_arm.arm_INS_accel + INS_ACCEL_X_ADDRESS_OFFSET);
	// robot_arm.accel_y = *(robot_arm.arm_INS_accel + INS_ACCEL_Y_ADDRESS_OFFSET);
	// robot_arm.accel_z = *(robot_arm.arm_INS_accel + INS_ACCEL_Z_ADDRESS_OFFSET);

	update_joint_6_6020_angle();
}

void robot_arm_init(void)
{
	robot_arm.arm_INS_angle = get_INS_angle_point();
	robot_arm.arm_INS_speed = get_gyro_data_point();
	// robot_arm.arm_INS_accel = get_accel_data_point();
	robot_arm.arm_state = ARM_STATE_ZERO_FORCE;
	robot_arm.fMovCmded = 0;
	robot_arm.fHoming = 0;

	const static fp32 joint_6_6020_angle_pid_coeffs[3] = {JOINT_6_6020_ANGLE_PID_KP, JOINT_6_6020_ANGLE_PID_KI, JOINT_6_6020_ANGLE_PID_KD};
	PID_init(&robot_arm.joint_6_6020_angle_pid, PID_POSITION, joint_6_6020_angle_pid_coeffs, JOINT_6_6020_ANGLE_PID_MAX_OUT, JOINT_6_6020_ANGLE_PID_MAX_IOUT, 0, &rad_err_handler);
	const static fp32 joint_6_6020_speed_pid_coeffs[3] = {JOINT_6_6020_SPEED_PID_KP, JOINT_6_6020_SPEED_PID_KI, JOINT_6_6020_SPEED_PID_KD};
	PID_init(&robot_arm.joint_6_6020_speed_pid, PID_POSITION, joint_6_6020_speed_pid_coeffs, JOINT_6_6020_SPEED_PID_MAX_OUT, JOINT_6_6020_SPEED_PID_MAX_IOUT, 0, &raw_err_handler);

	robot_arm_return_to_center(0, JOINT_ID_LAST - 1);
	CAN_cmd_switch_motor_power(1);
}

void robot_arm_return_to_center(uint8_t _start, uint8_t _end)
{
	// inclusively from index _start to index _end
	if (_start < _end)
	{
		memcpy(&robot_arm.joint_angle_target[_start], &joint_angle_home[_start], (_end - _start + 1) * sizeof(joint_angle_home[0]));
	}
}
