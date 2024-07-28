/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       can_receive.c/h
  * @brief      there is CAN interrupt function  to receive motor data,
  *             and CAN send function to send motor current to control motor.
  *             这里是CAN中断接收函数，接收电机数据,CAN发送函数发送电机电流控制电机.
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

#include "CAN_receive.h"
#include "cmsis_os.h"
#include "detect_task.h"
#include "main.h"
#include "pid.h"
#include "robot_arm_task.h"
#include "user_lib.h"

#define ENABLE_ARM_MOTOR_POWER 0

#define REVERSE_JOINT_0_DIRECTION 1

#define MOTOR_6012_GEAR_RATIO 36.0f
#define MOTOR_6012_INPUT_TORQUE_TO_MAIN_CURRENT_RATIO 0.225146199f
#define MOTOR_6012_MAIN_CURRENT_TO_ROTOR_CURRENT_RATIO 0.212f
#define MOTOR_6012_CMD_TO_TORQUE_RATIO (1.0f / MOTOR_6012_GEAR_RATIO / MOTOR_6012_INPUT_TORQUE_TO_MAIN_CURRENT_RATIO / MOTOR_6012_MAIN_CURRENT_TO_ROTOR_CURRENT_RATIO / 33.0f * 2048.0f)
#define MOTOR_6012_BROADCAST_CMD_TO_TORQUE_RATIO (1.0f / MOTOR_6012_GEAR_RATIO / MOTOR_6012_INPUT_TORQUE_TO_MAIN_CURRENT_RATIO / MOTOR_6012_MAIN_CURRENT_TO_ROTOR_CURRENT_RATIO / 32.0f * 2000.0f)

#define MOTOR_4010_GEAR_RATIO 36.0f
#define MOTOR_4010_INPUT_TORQUE_TO_MAIN_CURRENT_RATIO 0.225146199f // @TODO
#define MOTOR_4010_MAIN_CURRENT_TO_ROTOR_CURRENT_RATIO 0.212f	   // @TODO
#define MOTOR_4010_CMD_TO_TORQUE_RATIO (1.0f / MOTOR_4010_GEAR_RATIO / MOTOR_4010_INPUT_TORQUE_TO_MAIN_CURRENT_RATIO / MOTOR_4010_MAIN_CURRENT_TO_ROTOR_CURRENT_RATIO / 33.0f * 2048.0f)
#define MOTOR_4010_BROADCAST_CMD_TO_TORQUE_RATIO (1.0f / MOTOR_4010_GEAR_RATIO / MOTOR_4010_INPUT_TORQUE_TO_MAIN_CURRENT_RATIO / MOTOR_4010_MAIN_CURRENT_TO_ROTOR_CURRENT_RATIO / 32.0f * 2000.0f)

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
// motor data read
// #define get_rm_motor_measure(ptr, data)                                \
// 	{                                                                  \
// 		(ptr)->last_ecd = (ptr)->ecd;                                  \
// 		(ptr)->ecd = (uint16_t)((data)[0] << 8 | (data)[1]);           \
// 		(ptr)->speed_rpm = (uint16_t)((data)[2] << 8 | (data)[3]);     \
// 		(ptr)->given_current = (uint16_t)((data)[4] << 8 | (data)[5]); \
// 		(ptr)->temperature = (data)[6];                                \
// 	}

motor_measure_t motor_measure[JOINT_ID_LAST];

static CAN_TxHeaderTypeDef can_tx_message;
static uint8_t can_send_data[8];
uint32_t send_mail_box;

const float MIT_CONTROL_P_MAX[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {12.5f, 12.5f, 12.5f};
const float MIT_CONTROL_P_MIN[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {-12.5f, -12.5f, -12.5f};
const float MIT_CONTROL_V_MAX[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {25.0f, 45.0f, 30.0f};
const float MIT_CONTROL_V_MIN[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {-25.0f, -45.0f, -30.0f};
const float MIT_CONTROL_T_MAX[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {20.0f, 24.0f, 10.0f};
const float MIT_CONTROL_T_MIN[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {-20.0f, -24.0f, -10.0f};
const float MIT_CONTROL_KP_MAX[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {500.0f, 500.0f, 500.0f};
const float MIT_CONTROL_KP_MIN[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {0.0f, 0.0f, 0.0f};
const float MIT_CONTROL_KD_MAX[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {5.0f, 5.0f, 5.0f};
const float MIT_CONTROL_KD_MIN[LAST_MIT_CONTROLLED_MOTOR_TYPE] = {0.0f, 0.0f, 0.0f};

// @TODO: measure 6020 offset angle
const uint16_t joint_6_6020_offset_ecd = 2876;

HAL_StatusTypeDef encode_MIT_motor_control(uint16_t id, float _pos, float _vel, float _KP, float _KD, float _torq, uint8_t blocking_call, MIT_controlled_motor_type_e motor_type, CAN_HandleTypeDef *hcan_ptr);
HAL_StatusTypeDef encode_ktech_broadcast_current_control(const fp32 joint_torques[7], uint8_t blocking_call, CAN_HandleTypeDef *hcan_ptr);

void decode_MG4005_motor_torque_feedback(uint8_t *data, uint8_t bMotorId);
void decode_MS4005_motor_torque_feedback(uint8_t *data, uint8_t bMotorId);
HAL_StatusTypeDef decode_4310_motor_feedback(uint8_t *data, uint8_t bMotorId);

HAL_StatusTypeDef enable_DaMiao_motor(uint32_t id, uint8_t _enable, CAN_HandleTypeDef *hcan_ptr, uint8_t blocking_call);
HAL_StatusTypeDef soft_disable_Ktech_motor(uint32_t id, CAN_HandleTypeDef *hcan_ptr, uint8_t blocking_call);
HAL_StatusTypeDef blocking_can_send(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header, uint8_t *tx_data);
float uint_to_float_motor(int x_int, float x_min, float x_max, int bits);
int float_to_uint_motor(float x, float x_min, float x_max, int bits);
HAL_StatusTypeDef Send_CAN_Cmd(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header, uint8_t *tx_data, uint8_t blocking_call);

/**
 * @brief          hal CAN fifo call back, receive motor data
 * @param[in]      hcan, the point to CAN handle
 * @retval         none
 */
/**
 * @brief          hal库CAN回调函数,接收电机数据
 * @param[in]      hcan:CAN句柄指针
 * @retval         none
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_RxHeaderTypeDef rx_header;
	uint8_t rx_data[8];
	uint8_t bMotorId = 0xFF;

	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);

	if (hcan == &LOWER_MOTORS_CAN)
	{
		switch (rx_header.StdId)
		{
		case CAN_JOINT_MOTOR_0_4310_RX_ID:
		{
			bMotorId = JOINT_ID_0_4310;
			decode_4310_motor_feedback(rx_data, bMotorId);
			break;
		}
		case CAN_JOINT_MOTOR_1_4310_RX_ID:
		{
			bMotorId = JOINT_ID_1_4310;
			decode_4310_motor_feedback(rx_data, bMotorId);
			break;
		}
		case CAN_JOINT_MOTOR_2_4310_RX_ID:
		{
			bMotorId = JOINT_ID_2_4310;
			decode_4310_motor_feedback(rx_data, bMotorId);
			break;
		}
		default:
		{
			break;
		}
		}
	}
	else if (hcan == &UPPER_MOTORS_CAN)
	{
		switch (rx_header.StdId)
		{
		case CAN_JOINT_MOTOR_3_4005_RX_ID:
		{
			if (rx_data[0] == CAN_KTECH_TORQUE_ID)
			{
				// @Warning: motor 3 is MG4005, different from MS4005
				bMotorId = JOINT_ID_3_4005;
				decode_MG4005_motor_torque_feedback(rx_data, bMotorId);
			}
			break;
		}
		case CAN_JOINT_MOTOR_4_4005_RX_ID:
		{
			if (rx_data[0] == CAN_KTECH_TORQUE_ID)
			{
				bMotorId = JOINT_ID_4_4005;
				decode_MS4005_motor_torque_feedback(rx_data, bMotorId);
			}
			break;
		}
		case CAN_JOINT_MOTOR_5_4005_RX_ID:
		{
			if (rx_data[0] == CAN_KTECH_TORQUE_ID)
			{
				bMotorId = JOINT_ID_5_4005;
				decode_MS4005_motor_torque_feedback(rx_data, bMotorId);
			}
			break;
		}
		case CAN_JOINT_MOTOR_6_4005_RX_ID:
		{
			if (rx_data[0] == CAN_KTECH_TORQUE_ID)
			{
				bMotorId = JOINT_ID_6_4005;
				decode_MS4005_motor_torque_feedback(rx_data, bMotorId);
			}
			break;
		}
		default:
		{
			break;
		}
		}
	}

	if (bMotorId < JOINT_ID_LAST)
	{
		detect_hook(bMotorId + JOINT_0_TOE);
	}
}

float uint_to_float_motor(int x_int, float x_min, float x_max, int bits)
{
	/// converts unsigned int to float, given range and number of bits ///
	float span = x_max - x_min;
	float offset = x_min;
	return ((float)x_int) * span / ((float)((1 << bits) - 1)) + offset;
}

int float_to_uint_motor(float x, float x_min, float x_max, int bits)
{
	/// Converts a float to an unsigned int, given range and number of bits///
	float span = x_max - x_min;
	float offset = x_min;
	if (x >= x_max)
	{
		return ((1 << bits) - 1);
	}
	else if (x <= x_min)
	{
		return 0;
	}
	else
	{
		return (int)((x - offset) * ((float)((1 << bits) - 1)) / span);
	}
}

void decode_MG4005_motor_torque_feedback(uint8_t *data, uint8_t bMotorId)
{
	int16_t current_int = (data[3] << 8) | data[2]; // A
	int16_t v_int = (data[5] << 8) | data[4];		// deg/s
	int16_t p_int = ((data[7] << 8) | data[6]);		// 16bit abs encoder

	motor_measure[bMotorId].feedback_current = (fp32)current_int;
	motor_measure[bMotorId].velocity = ((fp32)v_int) / MOTOR_MG4005_GEAR_RATIO / 180.0f * PI;
	motor_measure[bMotorId].output_angle = ((fp32)p_int) / (1 << 16) * 2.0f * PI;
	motor_measure[bMotorId].temperature = data[1];
}

void decode_MS4005_motor_torque_feedback(uint8_t *data, uint8_t bMotorId)
{
	int16_t current_int = (data[3] << 8) | data[2]; // A
	int16_t v_int = (data[5] << 8) | data[4];		// deg/s
	int16_t p_int = ((data[7] << 8) | data[6]);		// 16bit abs encoder

	motor_measure[bMotorId].feedback_current = (fp32)current_int;
	motor_measure[bMotorId].velocity = ((fp32)v_int) / MOTOR_MS4005_GEAR_RATIO / 180.0f * PI;
	motor_measure[bMotorId].output_angle = ((fp32)p_int) / (1 << 16) * 2.0f * PI;
	motor_measure[bMotorId].temperature = data[1];
}

HAL_StatusTypeDef decode_4310_motor_feedback(uint8_t *data, uint8_t bMotorId)
{
	HAL_StatusTypeDef ret_value = HAL_ERROR;
	uint8_t error_id = data[0] >> 4;
	if ((error_id != 0) && (error_id != 1))
	{
		// Note: error_id = 0， 1 means motor power is disabled/enabled
		ret_value = HAL_ERROR;
	}
	else
	{
		uint16_t p_int = (data[1] << 8) | data[2];		   // rad
		uint16_t v_int = (data[3] << 4) | (data[4] >> 4);  // rad/s
		uint16_t t_int = ((data[4] & 0xF) << 8) | data[5]; // Nm

		fp32 temp_output_angle = uint_to_float_motor(p_int, MIT_CONTROL_P_MIN[DM_4310], MIT_CONTROL_P_MAX[DM_4310], 16);
		fp32 temp_velocity = uint_to_float_motor(v_int, MIT_CONTROL_V_MIN[DM_4310], MIT_CONTROL_V_MAX[DM_4310], 12);
		fp32 temp_torque = uint_to_float_motor(t_int, MIT_CONTROL_T_MIN[DM_4310], MIT_CONTROL_T_MAX[DM_4310], 12);
		motor_measure[bMotorId].temperature = data[6];

#if REVERSE_JOINT_0_DIRECTION
		if (bMotorId == JOINT_ID_0_4310)
		{
			temp_output_angle *= -1;
			temp_velocity *= -1;
			temp_torque *= -1;
		}
#endif
		motor_measure[bMotorId].output_angle = temp_output_angle;
		motor_measure[bMotorId].velocity = temp_velocity;
		motor_measure[bMotorId].torque = temp_torque;
		ret_value = HAL_OK;
	}
	return ret_value;
}

uint8_t arm_joints_cmd_position(float joint_angle_target_ptr[7], fp32 dt, uint8_t fIsTeaching)
{
	uint8_t fValidInput = 1;
	uint8_t pos_index;
	for (pos_index = 0; pos_index < 7; pos_index++)
	{
		if (joint_angle_target_ptr[pos_index] != joint_angle_target_ptr[pos_index])
		{
			robot_arm.fMasterSwitch = 0;
			fValidInput = 0;
			break;
		}
	}

	fp32 joint_torques[7] = {0};
	if (fValidInput && robot_arm.fMasterSwitch)
	{
		if (fIsTeaching)
		{
			for (uint8_t i = 0; i < 7; i++)
			{
				joint_torques[i] = PID_calc(&robot_arm.joint_angle_teach_pid[i], motor_measure[i].output_angle, joint_angle_target_ptr[i], dt);
			}
		}
		else
		{
			for (uint8_t i = 0; i < 7; i++)
			{
				joint_torques[i] = PID_calc(&robot_arm.joint_angle_pid[i], motor_measure[i].output_angle, joint_angle_target_ptr[i], dt);
			}
		}
	}
	else
	{
		memset(joint_torques, 0, sizeof(joint_torques));
	}
	arm_joints_cmd_torque(joint_torques);
	return fValidInput;
}

void arm_joints_cmd_torque(const fp32 joint_torques[7])
{
	uint8_t blocking_call = 1;
#if REVERSE_JOINT_0_DIRECTION
	encode_MIT_motor_control(CAN_JOINT_MOTOR_0_4310_TX_ID, 0, 0, 0, 0, -joint_torques[0], blocking_call, DM_4310, &LOWER_MOTORS_CAN);
#else
	encode_MIT_motor_control(CAN_JOINT_MOTOR_0_4310_TX_ID, 0, 0, 0, 0, joint_torques[0], blocking_call, DM_4310, &LOWER_MOTORS_CAN);
#endif
	osDelay(1);
	encode_MIT_motor_control(CAN_JOINT_MOTOR_1_4310_TX_ID, 0, 0, 0, 0, joint_torques[1], blocking_call, DM_4310, &LOWER_MOTORS_CAN);
	osDelay(1);
	encode_ktech_broadcast_current_control(joint_torques, blocking_call, &UPPER_MOTORS_CAN);
	encode_MIT_motor_control(CAN_JOINT_MOTOR_2_4310_TX_ID, 0, 0, 0, 0, joint_torques[2], blocking_call, DM_4310, &LOWER_MOTORS_CAN);
	osDelay(1);
}

HAL_StatusTypeDef enable_DaMiao_motor(uint32_t id, uint8_t _enable, CAN_HandleTypeDef *hcan_ptr, uint8_t blocking_call)
{
	can_tx_message.StdId = id;
	can_tx_message.IDE = CAN_ID_STD;
	can_tx_message.RTR = CAN_RTR_DATA;
	can_tx_message.DLC = 0x08;

	memset(can_send_data, 0xFF, sizeof(can_send_data));

	if (_enable)
	{
		can_send_data[7] = 0xFC;
	}
	else
	{
		// disable
		can_send_data[7] = 0xFD;
	}
	return Send_CAN_Cmd(hcan_ptr, &can_tx_message, can_send_data, blocking_call);
}

void CAN_cmd_switch_motor_power(uint8_t _enable)
{
	uint8_t blocking_call = 1;
	if (_enable == 0)
	{
		soft_disable_Ktech_motor(CAN_JOINT_MOTOR_3_4005_RX_ID, &UPPER_MOTORS_CAN, blocking_call);
		osDelay(2);
		soft_disable_Ktech_motor(CAN_JOINT_MOTOR_4_4005_RX_ID, &UPPER_MOTORS_CAN, blocking_call);
		osDelay(2);
		soft_disable_Ktech_motor(CAN_JOINT_MOTOR_5_4005_RX_ID, &UPPER_MOTORS_CAN, blocking_call);
		osDelay(2);
		soft_disable_Ktech_motor(CAN_JOINT_MOTOR_6_4005_RX_ID, &UPPER_MOTORS_CAN, blocking_call);
		osDelay(2);

		for (uint8_t i = 0; i < 7; i++)
		{
			PID_clear(&robot_arm.joint_angle_pid[i]);
			PID_clear(&robot_arm.joint_angle_teach_pid[i]);
		}
	}

	enable_DaMiao_motor(CAN_JOINT_MOTOR_0_4310_TX_ID, _enable, &LOWER_MOTORS_CAN, blocking_call);
	osDelay(2);
	enable_DaMiao_motor(CAN_JOINT_MOTOR_1_4310_TX_ID, _enable, &LOWER_MOTORS_CAN, blocking_call);
	osDelay(2);
	enable_DaMiao_motor(CAN_JOINT_MOTOR_2_4310_TX_ID, _enable, &LOWER_MOTORS_CAN, blocking_call);
	osDelay(2);
}

HAL_StatusTypeDef soft_disable_Ktech_motor(uint32_t id, CAN_HandleTypeDef *hcan_ptr, uint8_t blocking_call)
{
	can_tx_message.StdId = id;
	can_tx_message.IDE = CAN_ID_STD;
	can_tx_message.RTR = CAN_RTR_DATA;
	can_tx_message.DLC = 0x08;

	memset(can_send_data, 0, sizeof(can_send_data));
	can_send_data[0] = CAN_KTECH_TEMP_DISABLE_MOTOR_ID;

	return Send_CAN_Cmd(hcan_ptr, &can_tx_message, can_send_data, blocking_call);
}

HAL_StatusTypeDef encode_ktech_broadcast_current_control(const fp32 current_cmd[4], uint8_t blocking_call, CAN_HandleTypeDef *hcan_ptr)
{
	can_tx_message.StdId = CAN_KTECH_BROADCAST_CURRENT_TX_ID;
	can_tx_message.ExtId = 0x00;
	can_tx_message.IDE = CAN_ID_STD;
	can_tx_message.RTR = CAN_RTR_DATA;
	can_tx_message.DLC = 8;

#if ENABLE_ARM_MOTOR_POWER
	int16_t iqControl_1 = fp32_abs_constrain(current_cmd[3], MOTOR_MG4005_MAX_CMD);
	int16_t iqControl_2 = fp32_abs_constrain(current_cmd[4], MOTOR_MS4005_MAX_CMD);
	int16_t iqControl_3 = fp32_abs_constrain(current_cmd[5], MOTOR_MS4005_MAX_CMD);
	int16_t iqControl_4 = fp32_abs_constrain(current_cmd[6], MOTOR_MS4005_MAX_CMD);
	can_send_data[0] = *(uint8_t *)(&iqControl_1);
	can_send_data[1] = *((uint8_t *)(&iqControl_1) + 1);
	can_send_data[2] = *(uint8_t *)(&iqControl_2);
	can_send_data[3] = *((uint8_t *)(&iqControl_2) + 1);
	can_send_data[4] = *(uint8_t *)(&iqControl_3);
	can_send_data[5] = *((uint8_t *)(&iqControl_3) + 1);
	can_send_data[6] = *(uint8_t *)(&iqControl_4);
	can_send_data[7] = *((uint8_t *)(&iqControl_4) + 1);
#else
	memset(can_send_data, 0, sizeof(can_send_data));
#endif
	return Send_CAN_Cmd(hcan_ptr, &can_tx_message, can_send_data, blocking_call);
}

HAL_StatusTypeDef encode_MIT_motor_control(uint16_t id, float _pos, float _vel, float _KP, float _KD, float _torq, uint8_t blocking_call, MIT_controlled_motor_type_e motor_type, CAN_HandleTypeDef *hcan_ptr)
{
	can_tx_message.StdId = id;
	can_tx_message.IDE = CAN_ID_STD;
	can_tx_message.RTR = CAN_RTR_DATA;
	can_tx_message.DLC = 0x08;

#if (ENABLE_ARM_MOTOR_POWER == 0)
	_pos = 0;
	_vel = 0;
	_KP = 0;
	_KD = 0;
	_torq = 0;
#endif

	uint16_t pos_tmp, vel_tmp, kp_tmp, kd_tmp, tor_tmp;
	pos_tmp = float_to_uint_motor(_pos, MIT_CONTROL_P_MIN[motor_type], MIT_CONTROL_P_MAX[motor_type], 16);
	vel_tmp = float_to_uint_motor(_vel, MIT_CONTROL_V_MIN[motor_type], MIT_CONTROL_V_MAX[motor_type], 12);
	kp_tmp = float_to_uint_motor(_KP, MIT_CONTROL_KP_MIN[motor_type], MIT_CONTROL_KP_MAX[motor_type], 12);
	kd_tmp = float_to_uint_motor(_KD, MIT_CONTROL_KD_MIN[motor_type], MIT_CONTROL_KD_MAX[motor_type], 12);
	tor_tmp = float_to_uint_motor(_torq, MIT_CONTROL_T_MIN[motor_type], MIT_CONTROL_T_MAX[motor_type], 12);

	can_send_data[0] = (pos_tmp >> 8);
	can_send_data[1] = pos_tmp;
	can_send_data[2] = (vel_tmp >> 4);
	can_send_data[3] = ((vel_tmp & 0xF) << 4) | (kp_tmp >> 8);
	can_send_data[4] = kp_tmp;
	can_send_data[5] = (kd_tmp >> 4);
	can_send_data[6] = ((kd_tmp & 0xF) << 4) | (tor_tmp >> 8);
	can_send_data[7] = tor_tmp;

	return Send_CAN_Cmd(hcan_ptr, &can_tx_message, can_send_data, blocking_call);
}

HAL_StatusTypeDef Send_CAN_Cmd(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header, uint8_t *tx_data, uint8_t blocking_call)
{
	if (blocking_call)
	{
		return blocking_can_send(hcan, tx_header, tx_data);
	}
	else
	{
		return HAL_CAN_AddTxMessage(hcan, tx_header, tx_data, &send_mail_box);
	}
}

HAL_StatusTypeDef blocking_can_send(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header, uint8_t *tx_data)
{
	HAL_StatusTypeDef CAN_status = HAL_TIMEOUT;
	uint16_t try_cnt = 0;
	const uint16_t retry_delay_ms = 1;
	const uint16_t retry_timeout_ms = 5000;
	while (1)
	{
		if ((hcan->State == HAL_CAN_STATE_READY) || (hcan->State == HAL_CAN_STATE_LISTENING))
		{
			CAN_status = HAL_CAN_AddTxMessage(hcan, tx_header, tx_data, &send_mail_box);
		}

		if (CAN_status == HAL_OK)
		{
			break;
		}
		else if (try_cnt > retry_timeout_ms / retry_delay_ms)
		{
			CAN_status = HAL_TIMEOUT;
			break;
		}
		try_cnt++;
		osDelay(retry_delay_ms);
	}
	return CAN_status;
}
