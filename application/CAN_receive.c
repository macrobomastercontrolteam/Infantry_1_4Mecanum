/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       can_receive.c/h
  * @brief      there is CAN interrupt function  to receive motor data,
  *             and CAN send function to send motor current to control motor.
  *             ������CAN�жϽ��պ��������յ������?,CAN���ͺ������͵���������Ƶ��.
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

#pragma anon_unions

#include "CAN_receive.h"
#include "biped.h"
#include "bsp_rng.h"
#include "cmsis_os.h"
#include "detect_task.h"
#include "main.h"
#include "user_lib.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
// motor data read
#define get_rm_motor_measure(ptr, data)                                \
	{                                                                  \
		(ptr)->last_ecd = (ptr)->ecd;                                  \
		(ptr)->ecd = (uint16_t)((data)[0] << 8 | (data)[1]);           \
		(ptr)->speed_rpm = (uint16_t)((data)[2] << 8 | (data)[3]);     \
		(ptr)->given_current = (uint16_t)((data)[4] << 8 | (data)[5]); \
		(ptr)->temperature = (data)[6];                                \
	}

motor_measure_t motor_measure[CHASSIS_ID_LAST];

// static CAN_TxHeaderTypeDef gimbal_tx_message;
// static uint8_t gimbal_can_send_data[8];
static CAN_TxHeaderTypeDef chassis_tx_message;
static uint8_t chassis_can_send_data[8];
static uint32_t send_mail_box;

const float MOTOR_P_MAX[LAST_MOTOR_TYPE] = {12.5f, 12.5f};
const float MOTOR_P_MIN[LAST_MOTOR_TYPE] = {-12.5f, -12.5f};
const float MOTOR_V_MAX[LAST_MOTOR_TYPE] = {25.0f, 45.0f};
const float MOTOR_V_MIN[LAST_MOTOR_TYPE] = {-25.0f, -45.0f};
const float MOTOR_T_MAX[LAST_MOTOR_TYPE] = {20.0f, 24.0f};
const float MOTOR_T_MIN[LAST_MOTOR_TYPE] = {-20.0f, -24.0f};
const float MOTOR_KP_MAX[LAST_MOTOR_TYPE] = {500.0f, 500.0f};
const float MOTOR_KP_MIN[LAST_MOTOR_TYPE] = {0.0f, 0.0f};
const float MOTOR_KD_MAX[LAST_MOTOR_TYPE] = {5.0f, 5.0f};
const float MOTOR_KD_MIN[LAST_MOTOR_TYPE] = {0.0f, 0.0f};

HAL_StatusTypeDef decode_8006_motor_feedback(uint8_t *data, uint8_t *bMotorIdPtr);
HAL_StatusTypeDef decode_9015_motor_feedback(uint8_t *data, uint8_t *bMotorIdPtr);
float uint_to_float_motor(int x_int, float x_min, float x_max, int bits);
int float_to_uint_motor(float x, float x_min, float x_max, int bits);
void request_9015_multiangle_data(void);
void decode_9015_motor_multiangle_feedback(uint8_t *data, const uint8_t bMotorId);

uint8_t get_motor_array_index(can_msg_id_e _SINGLE_CAN_ID)
{
	switch (_SINGLE_CAN_ID)
	{
		case CAN_HIP1_TX_ID:
		case CAN_HIP2_TX_ID:
		case CAN_HIP3_TX_ID:
		case CAN_HIP4_TX_ID:
			return (_SINGLE_CAN_ID - CAN_HIP1_TX_ID + CHASSIS_ID_HIP_RF);
		case CAN_DRIVE1_PVT_TX_ID:
		case CAN_DRIVE2_PVT_TX_ID:
			return (_SINGLE_CAN_ID - CAN_DRIVE1_PVT_TX_ID + CHASSIS_ID_DRIVE_RIGHT);
		case CAN_YAW_MOTOR_FEEDBACK_ID:
		case CAN_PIT_MOTOR_FEEDBACK_ID:
		case CAN_TRIGGER_MOTOR_FEEDBACK_ID:
			return (_SINGLE_CAN_ID - CAN_YAW_MOTOR_FEEDBACK_ID + CHASSIS_ID_YAW);
		default:
			return 0;
	}
}

/**
 * @brief          hal CAN fifo call back, receive motor data
 * @param[in]      hcan, the point to CAN handle
 * @retval         none
 */
/**
 * @brief          hal��CAN�ص�����,���յ������?
 * @param[in]      hcan:CAN���ָ��?
 * @retval         none
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_RxHeaderTypeDef rx_header;
	uint8_t rx_data[8];
	uint8_t bMotorId = 0xFF;

	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);

	switch (rx_header.StdId)
	{
		case CAN_HIP_MOTOR_FEEDBACK_ID:
		{
			decode_8006_motor_feedback(rx_data, &bMotorId);
			break;
		}
		case CAN_DRIVE_MOTOR_PVT_FEEDBACK_ID1:
		case CAN_DRIVE_MOTOR_PVT_FEEDBACK_ID2:
		{
			decode_9015_motor_feedback(rx_data, &bMotorId);
			break;
		}
		case CAN_DRIVE_MOTOR_CMD_FEEDBACK_ID1:
		case CAN_DRIVE_MOTOR_CMD_FEEDBACK_ID2:
		{
			bMotorId = (rx_header.StdId == CAN_DRIVE_MOTOR_CMD_FEEDBACK_ID1) ? CHASSIS_ID_DRIVE_RIGHT : CHASSIS_ID_DRIVE_LEFT;
			if (rx_data[0] == CAN_9015_MULTIANGLE_MSG_ID)
			{
				decode_9015_motor_multiangle_feedback(rx_data, bMotorId);
			}
			break;
		}
		case CAN_YAW_MOTOR_FEEDBACK_ID:
		case CAN_PIT_MOTOR_FEEDBACK_ID:
		case CAN_TRIGGER_MOTOR_FEEDBACK_ID:
		{
			bMotorId = get_motor_array_index((can_msg_id_e)rx_header.StdId);
			get_rm_motor_measure(&motor_measure[bMotorId], rx_data);
			break;
		}
		default:
		{
			break;
		}
	}

	if (bMotorId < CHASSIS_ID_LAST)
	{
		detect_hook(bMotorId + CHASSIS_HIP_MOTOR1_TOE);
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

void decode_9015_motor_multiangle_feedback(uint8_t *data, const uint8_t bMotorId)
{
	int32_t multiangle_int = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4]; // deg
	// reverse direction, divide by 100, and convert to rad (1/100.0f/180.0f*PI)
	motor_measure[bMotorId].output_angle = multiangle_int * 0.00017453292519943296f;
	switch (bMotorId)
	{
		case CHASSIS_ID_DRIVE_RIGHT:
		{
#if REVERSE_RIGHT_DRIVE_MOTOR_DIRECTION
			motor_measure[bMotorId].output_angle *= -1.0f;
#endif
			if (biped.leg_R.fResetMultiAngleOffset)
			{
				biped.leg_R.fResetMultiAngleOffset = 0;
			}
			break;
		}
		case CHASSIS_ID_DRIVE_LEFT:
		{
#if REVERSE_LEFT_DRIVE_MOTOR_DIRECTION
			motor_measure[bMotorId].output_angle *= -1.0f;
#endif
			if (biped.leg_L.fResetMultiAngleOffset)
			{
				biped.leg_L.fResetMultiAngleOffset = 0;
			}
			break;
		}
	}
}

HAL_StatusTypeDef decode_8006_motor_feedback(uint8_t *data, uint8_t *bMotorIdPtr)
{
	uint8_t error_id = data[0] >> 4;
	if (error_id != 0)
	{
		biped.fBipedEnable = 0;
		return HAL_ERROR;
	}
	else
	{
		uint16_t p_int = (data[1] << 8) | data[2];         // rad
		uint16_t v_int = (data[3] << 4) | (data[4] >> 4);  // rad/s
		uint16_t t_int = ((data[4] & 0xF) << 8) | data[5]; // Nm

		*bMotorIdPtr = (data[0] & 0xF) - 1 + CHASSIS_ID_HIP_RF;
		motor_measure[*bMotorIdPtr].output_angle = uint_to_float_motor(p_int, MOTOR_P_MIN[DM_8006], MOTOR_P_MAX[DM_8006], 16);
		motor_measure[*bMotorIdPtr].velocity = uint_to_float_motor(v_int, MOTOR_V_MIN[DM_8006], MOTOR_V_MAX[DM_8006], 12);
		motor_measure[*bMotorIdPtr].torque = uint_to_float_motor(t_int, MOTOR_T_MIN[DM_8006], MOTOR_T_MAX[DM_8006], 12);
		motor_measure[*bMotorIdPtr].temperature = data[6];
	}
	return HAL_OK;
}

HAL_StatusTypeDef decode_9015_motor_feedback(uint8_t *data, uint8_t *bMotorIdPtr)
{
	uint16_t p_int = (data[1] << 8) | data[2];         // rad
	uint16_t v_int = (data[3] << 4) | (data[4] >> 4);  // rad/s
	uint16_t t_int = ((data[4] & 0xF) << 8) | data[5]; // Nm

	*bMotorIdPtr = data[0] - 1 + CHASSIS_ID_DRIVE_RIGHT;
	motor_measure[*bMotorIdPtr].input_angle = uint_to_float_motor(p_int, MOTOR_P_MIN[MA_9015], MOTOR_P_MAX[MA_9015], 16);
	motor_measure[*bMotorIdPtr].velocity = uint_to_float_motor(v_int, MOTOR_V_MIN[MA_9015], MOTOR_V_MAX[MA_9015], 12);
	motor_measure[*bMotorIdPtr].torque = uint_to_float_motor(t_int, MOTOR_T_MIN[MA_9015], MOTOR_T_MAX[MA_9015], 12);
	motor_measure[*bMotorIdPtr].temperature = data[6];

#if REVERSE_RIGHT_DRIVE_MOTOR_DIRECTION || REVERSE_LEFT_DRIVE_MOTOR_DIRECTION
	switch (*bMotorIdPtr)
	{
		case CHASSIS_ID_DRIVE_RIGHT:
		{
#if REVERSE_RIGHT_DRIVE_MOTOR_DIRECTION
			motor_measure[*bMotorIdPtr].input_angle *= -1.0f;
			motor_measure[*bMotorIdPtr].velocity *= -1.0f;
			motor_measure[*bMotorIdPtr].torque *= -1.0f;
#endif
			break;
		}
		case CHASSIS_ID_DRIVE_LEFT:
		{
#if REVERSE_LEFT_DRIVE_MOTOR_DIRECTION
			motor_measure[*bMotorIdPtr].input_angle *= -1.0f;
			motor_measure[*bMotorIdPtr].velocity *= -1.0f;
			motor_measure[*bMotorIdPtr].torque *= -1.0f;
#endif
			break;
		}
		default:
		{
			return HAL_ERROR;
		}
	}
#endif
	return HAL_OK;
}

uint8_t hip_motor_set_position(float RF_pos, float LF_pos, float LB_pos, float RB_pos, float pos_Kp, float pos_Kd)
{
	uint8_t fValidInput = 0;
	if ((RF_pos != RF_pos) || (LF_pos != LF_pos) || (LB_pos != LB_pos) || (RB_pos != RB_pos))
	{
		RF_pos = 0;
		LF_pos = 0;
		LB_pos = 0;
		RB_pos = 0;
		fValidInput = 0;
	}
	else
	{
		fValidInput = 1;
	}
	
	uint8_t blocking_call = 0;
	// @TODO: reenable it after hip motor is fixed
	encode_motor_control(CAN_HIP1_TX_ID, 0, 0, 0, 0, 0, blocking_call, DM_8006);
	// encode_motor_control(CAN_HIP1_TX_ID, RF_pos, 0, pos_Kp, pos_Kd, 0, blocking_call, DM_8006);
	osDelay(1);
	encode_motor_control(CAN_HIP2_TX_ID, LF_pos, 0, pos_Kp, pos_Kd, 0, blocking_call, DM_8006);
	osDelay(1);
	encode_motor_control(CAN_HIP3_TX_ID, LB_pos, 0, pos_Kp, pos_Kd, 0, blocking_call, DM_8006);
	osDelay(1);
	// @TODO: reenable it after hip motor is fixed
	encode_motor_control(CAN_HIP4_TX_ID, 0, 0, 0, 0, 0, blocking_call, DM_8006);
	// encode_motor_control(CAN_HIP4_TX_ID, RB_pos, 0, pos_Kp, pos_Kd, 0, blocking_call, DM_8006);
	osDelay(1);
	return fValidInput;
}

uint8_t hip_motor_set_torque(float RF_torq, float LF_torq, float LB_torq, float RB_torq)
{
	uint8_t fValidInput = 0;
	if ((RF_torq != RF_torq) || (LF_torq != LF_torq) || (LB_torq != LB_torq) || (RB_torq != RB_torq))
	{
		RF_torq = 0;
		LF_torq = 0;
		LB_torq = 0;
		RB_torq = 0;
		fValidInput = 0;
	}
	else
	{
		fValidInput = 1;
	}

#if REVERSE_LB_HIP_MOTOR_DIRECTION
	LB_torq *= -1.0f;
#endif

#if REVERSE_LF_HIP_MOTOR_DIRECTION
	LF_torq *= -1.0f;
#endif

#if REVERSE_RB_HIP_MOTOR_DIRECTION
	RB_torq *= -1.0f;
#endif

#if REVERSE_RF_HIP_MOTOR_DIRECTION
	RF_torq *= -1.0f;
#endif

	uint8_t blocking_call = 0;
	encode_motor_control(CAN_HIP1_TX_ID, 0, 0, 0, 0, RF_torq, blocking_call, DM_8006);
	osDelay(1);
	encode_motor_control(CAN_HIP2_TX_ID, 0, 0, 0, 0, LF_torq, blocking_call, DM_8006);
	osDelay(1);
	encode_motor_control(CAN_HIP3_TX_ID, 0, 0, 0, 0, LB_torq, blocking_call, DM_8006);
	osDelay(1);
	encode_motor_control(CAN_HIP4_TX_ID, 0, 0, 0, 0, RB_torq, blocking_call, DM_8006);
	// osDelay(1);
	return fValidInput;
}

uint8_t drive_motor_set_torque(float R_torq, float L_torq)
{
	uint8_t fValidInput = 0;
	if ((R_torq != R_torq) || (L_torq != L_torq))
	{
		R_torq = 0;
		L_torq = 0;
		fValidInput = 0;
	}
	else
	{
		fValidInput = 1;
	}

#if REVERSE_LEFT_DRIVE_MOTOR_DIRECTION
	L_torq *= -1.0f;
#endif

#if REVERSE_RIGHT_DRIVE_MOTOR_DIRECTION
	R_torq *= -1.0f;
#endif

	uint8_t blocking_call = 0;
	encode_motor_control(CAN_DRIVE1_PVT_TX_ID, 0, 0, 0, 0, R_torq, blocking_call, MA_9015);
	osDelay(2);
	encode_motor_control(CAN_DRIVE2_PVT_TX_ID, 0, 0, 0, 0, L_torq, blocking_call, MA_9015);
	osDelay(2);
	request_9015_multiangle_data();
	// osDelay(2);
	return fValidInput;
}

void request_9015_multiangle_data(void)
{
	static uint8_t can_9015_multiangle_data[8] = {CAN_9015_MULTIANGLE_MSG_ID, 0, 0, 0, 0, 0, 0, 0};
	chassis_tx_message.StdId = CAN_DRIVE_MOTOR_SINGLECMD_TX_ID + 1;
	chassis_tx_message.IDE = CAN_ID_STD;
	chassis_tx_message.RTR = CAN_RTR_DATA;
	chassis_tx_message.DLC = 0x08;
	HAL_CAN_AddTxMessage(&hcan1, &chassis_tx_message, can_9015_multiangle_data, &send_mail_box);

	osDelay(2);

	chassis_tx_message.StdId = CAN_DRIVE_MOTOR_SINGLECMD_TX_ID + 2;
	HAL_CAN_AddTxMessage(&hcan1, &chassis_tx_message, can_9015_multiangle_data, &send_mail_box);
}

void enable_all_motor_control(uint8_t _enable)
{
	// for (uint8_t trial = 0; trial <= 3; trial++)
	{
		hip_motor_set_torque(0, 0, 0, 0);
		osDelay(1);
		drive_motor_set_torque(0, 0);
		osDelay(1);

		// for (uint8_t motor_id = CAN_HIP1_TX_ID; motor_id <= CAN_HIP4_TX_ID; motor_id++)
		// {
		// 	enable_motor_control_8006(motor_id, _enable);
		// 	if (motor_id != CAN_HIP4_TX_ID)
		// 	{
		// 		osDelay(1);
		// 	}
		// }
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

HAL_StatusTypeDef encode_motor_control(uint16_t id, float _pos, float _vel, float _KP, float _KD, float _torq, uint8_t blocking_call, motor_type_e motor_type)
{
	chassis_tx_message.StdId = id;
	chassis_tx_message.IDE = CAN_ID_STD;
	chassis_tx_message.RTR = CAN_RTR_DATA;
	chassis_tx_message.DLC = 0x08;

	uint16_t pos_tmp, vel_tmp, kp_tmp, kd_tmp, tor_tmp;
	pos_tmp = float_to_uint_motor(_pos, MOTOR_P_MIN[motor_type], MOTOR_P_MAX[motor_type], 16);
	vel_tmp = float_to_uint_motor(_vel, MOTOR_V_MIN[motor_type], MOTOR_V_MAX[motor_type], 12);
	kp_tmp = float_to_uint_motor(_KP, MOTOR_KP_MIN[motor_type], MOTOR_KP_MAX[motor_type], 12);
	kd_tmp = float_to_uint_motor(_KD, MOTOR_KD_MIN[motor_type], MOTOR_KD_MAX[motor_type], 12);
	tor_tmp = float_to_uint_motor(_torq, MOTOR_T_MIN[motor_type], MOTOR_T_MAX[motor_type], 12);

	chassis_can_send_data[0] = (pos_tmp >> 8);
	chassis_can_send_data[1] = pos_tmp;
	chassis_can_send_data[2] = (vel_tmp >> 4);
	chassis_can_send_data[3] = ((vel_tmp & 0xF) << 4) | (kp_tmp >> 8);
	chassis_can_send_data[4] = kp_tmp;
	chassis_can_send_data[5] = (kd_tmp >> 4);
	chassis_can_send_data[6] = ((kd_tmp & 0xF) << 4) | (tor_tmp >> 8);
	chassis_can_send_data[7] = tor_tmp;

	CAN_HandleTypeDef *hcan_ptr;
	if (motor_type == MA_9015)
	{
		hcan_ptr = &hcan1;
	}
	else if (motor_type == DM_8006)
	{
		hcan_ptr = &hcan2;
	}

	if (blocking_call)
	{
		return blocking_can_send(hcan_ptr, &chassis_tx_message, chassis_can_send_data);
	}
	else
	{
		return HAL_CAN_AddTxMessage(hcan_ptr, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
	}
}

HAL_StatusTypeDef enable_motor_control_8006(uint32_t id, uint8_t _enable)
{
	chassis_tx_message.StdId = id;
	chassis_tx_message.IDE = CAN_ID_STD;
	chassis_tx_message.RTR = CAN_RTR_DATA;
	chassis_tx_message.DLC = 0x08;

	for (uint8_t i = 0; i < 7; i++)
	{
		chassis_can_send_data[i] = 0xFF;
	}

	if (_enable)
	{
		chassis_can_send_data[7] = 0xFC;
	}
	else
	{
		// disable
		chassis_can_send_data[7] = 0xFD;
	}

	return blocking_can_send(&hcan2, &chassis_tx_message, chassis_can_send_data);
}
