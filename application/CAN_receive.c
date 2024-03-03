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

#include "main.h"
#include "bsp_rng.h"

#include "user_lib.h"
#include "detect_task.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

#define DISABLE_ARM_MOTOR_POWER 0

//motor data read
#define get_motor_measure(ptr, data)                                    \
    {                                                                   \
        (ptr)->last_ecd = (ptr)->ecd;                                   \
        (ptr)->ecd = (uint16_t)((data)[0] << 8 | (data)[1]);            \
        (ptr)->speed_rpm = (uint16_t)((data)[2] << 8 | (data)[3]);      \
        (ptr)->given_current = (uint16_t)((data)[4] << 8 | (data)[5]);  \
        (ptr)->temperate = (data)[6];                                   \
    }

uint8_t convertCanIdToMotorIndex(uint32_t canId);

/**
 * @brief motor feedback data
 * Chassis CAN:
 * 0:chassis motor1 3508; 1:chassis motor2 3508; 2:chassis motor3 3508; 3:chassis motor4 3508;
 * 6:trigger motor 2006; 4:yaw gimbal motor 6020;
 * 
 * Gimbal CAN:
 * 5:pitch gimbal motor 6020;
 */
static motor_measure_t motor_chassis[MOTOR_LIST_LENGTH];

static CAN_TxHeaderTypeDef  gimbal_tx_message;
static uint8_t              gimbal_can_send_data[8];
static CAN_TxHeaderTypeDef  chassis_tx_message;
static uint8_t              chassis_can_send_data[8];

uint8_t convertCanIdToMotorIndex(uint32_t canId)
{
	switch (canId)
	{
		case CAN_3508_M1_ID:
		case CAN_3508_M2_ID:
		case CAN_3508_M3_ID:
		case CAN_3508_M4_ID:
		{
			return (canId - CAN_3508_M1_ID);
		}
		default:
		{
			return 0;
		}
	}
}

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
    uint8_t bMotorValid = 0;

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);

    switch (rx_header.StdId)
    {
        case CAN_3508_M1_ID:
        case CAN_3508_M2_ID:
        case CAN_3508_M3_ID:
        case CAN_3508_M4_ID:
        {
            if (hcan == &hcan2)
            {
              bMotorValid = 1;
            }
            break;
        }
    }
    
    if (bMotorValid == 1) {
        uint8_t bMotorId = convertCanIdToMotorIndex(rx_header.StdId);
        get_motor_measure(&motor_chassis[bMotorId], rx_data);
        detect_hook(CHASSIS_MOTOR1_TOE + bMotorId);
    }
}

void CAN_cmd_robot_arm(int16_t cmd_roll, int16_t cmd_pitch, int16_t cmd_yaw, int16_t cmd_x, int16_t cmd_y, int16_t cmd_z)
{
    uint32_t send_mail_box;
    gimbal_tx_message.IDE = CAN_ID_STD;
    gimbal_tx_message.RTR = CAN_RTR_DATA;
    gimbal_tx_message.DLC = 0x08;
	  
    // position
    gimbal_tx_message.StdId = CAN_GIMBAL_CONTROLLER_POSITION_TX_ID;
    gimbal_can_send_data[0] = *(uint8_t *)(&cmd_roll);
    gimbal_can_send_data[1] = *((uint8_t *)(&cmd_roll) + 1);
    gimbal_can_send_data[2] = *(uint8_t *)(&cmd_pitch);
    gimbal_can_send_data[3] = *((uint8_t *)(&cmd_pitch) + 1);
    gimbal_can_send_data[4] = *(uint8_t *)(&cmd_yaw);
    gimbal_can_send_data[5] = *((uint8_t *)(&cmd_yaw) + 1);
    // redundant data
    gimbal_can_send_data[6] = gimbal_can_send_data[5];
    gimbal_can_send_data[7] = gimbal_can_send_data[5];
    HAL_CAN_AddTxMessage(&GIMBAL_CAN, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);

    osDelay(1);

    // orientation
    gimbal_tx_message.StdId = CAN_GIMBAL_CONTROLLER_ORIENTATION_TX_ID;
    gimbal_can_send_data[0] = *(uint8_t *)(&cmd_x);
    gimbal_can_send_data[1] = *((uint8_t *)(&cmd_x) + 1);
    gimbal_can_send_data[2] = *(uint8_t *)(&cmd_y);
    gimbal_can_send_data[3] = *((uint8_t *)(&cmd_y) + 1);
    gimbal_can_send_data[4] = *(uint8_t *)(&cmd_z);
    gimbal_can_send_data[5] = *((uint8_t *)(&cmd_z) + 1);
    HAL_CAN_AddTxMessage(&GIMBAL_CAN, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);
}

void CAN_cmd_robot_arm_individual_motors(fp32 motor_pos[7])
{
    uint32_t send_mail_box;
    gimbal_tx_message.IDE = CAN_ID_STD;
    gimbal_tx_message.RTR = CAN_RTR_DATA;
    gimbal_tx_message.DLC = 0x08;

    uint8_t motor_pos_index;
    int16_t motor_pos_int16[7];
    for (motor_pos_index = 0; motor_pos_index < sizeof(motor_pos_int16) / sizeof(motor_pos_int16[0]); motor_pos_index++)
    {
#if DISABLE_ARM_MOTOR_POWER
      motor_pos_int16[motor_pos_index] = 0;
#else
      motor_pos_int16[motor_pos_index] = motor_pos[motor_pos_index] * RAD_TO_INT16_SCALE;
#endif
    }

	  // position
    gimbal_tx_message.StdId = CAN_GIMBAL_CONTROLLER_INDIVIDUAL_MOTOR_1_TX_ID;
    memcpy(gimbal_can_send_data, motor_pos_int16, sizeof(gimbal_can_send_data));
    HAL_CAN_AddTxMessage(&GIMBAL_CAN, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);

    osDelay(1);

    // orientation
    gimbal_tx_message.StdId = CAN_GIMBAL_CONTROLLER_INDIVIDUAL_MOTOR_2_TX_ID;
    memcpy(gimbal_can_send_data, &motor_pos_int16[4], sizeof(motor_pos_int16) - sizeof(gimbal_can_send_data));
    // redundant data
    gimbal_can_send_data[6] = gimbal_can_send_data[4];
    gimbal_can_send_data[7] = gimbal_can_send_data[5];
    HAL_CAN_AddTxMessage(&GIMBAL_CAN, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);
}

/**
  * @brief          send CAN packet of ID 0x700, it will set chassis motor 3508 to quick ID setting
  * @param[in]      none
  * @retval         none
  */
/**
  * @brief          发送ID为0x700的CAN包,它会设置3508电机进入快速设置ID
  * @param[in]      none
  * @retval         none
  */
void CAN_cmd_chassis_reset_ID(void)
{
    uint32_t send_mail_box;
    chassis_tx_message.StdId = 0x700;
    chassis_tx_message.IDE = CAN_ID_STD;
    chassis_tx_message.RTR = CAN_RTR_DATA;
    chassis_tx_message.DLC = 0x08;
    chassis_can_send_data[0] = 0;
    chassis_can_send_data[1] = 0;
    chassis_can_send_data[2] = 0;
    chassis_can_send_data[3] = 0;
    chassis_can_send_data[4] = 0;
    chassis_can_send_data[5] = 0;
    chassis_can_send_data[6] = 0;
    chassis_can_send_data[7] = 0;

    HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
}

/**
  * @brief          send control current of motor (0x201, 0x202, 0x203, 0x204)
  * @param[in]      motor1: (0x201) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor2: (0x202) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor3: (0x203) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor4: (0x204) 3508 motor control current, range [-16384,16384] 
  * @retval         none
  */
/**
  * @brief          发送电机控制电流(0x201,0x202,0x203,0x204)
  * @param[in]      motor1: (0x201) 3508电机控制电流, 范围 [-16384,16384]
  * @param[in]      motor2: (0x202) 3508电机控制电流, 范围 [-16384,16384]
  * @param[in]      motor3: (0x203) 3508电机控制电流, 范围 [-16384,16384]
  * @param[in]      motor4: (0x204) 3508电机控制电流, 范围 [-16384,16384]
  * @retval         none
  */
void CAN_cmd_chassis(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4)
{
    uint32_t send_mail_box;
    // driver motors (M3508)
    chassis_tx_message.StdId = CAN_CHASSIS_M3508_TX_ID;
    chassis_tx_message.IDE = CAN_ID_STD;
    chassis_tx_message.RTR = CAN_RTR_DATA;
    chassis_tx_message.DLC = 0x08;
    chassis_can_send_data[0] = motor1 >> 8;
    chassis_can_send_data[1] = motor1;
    chassis_can_send_data[2] = motor2 >> 8;
    chassis_can_send_data[3] = motor2;
    chassis_can_send_data[4] = motor3 >> 8;
    chassis_can_send_data[5] = motor3;
    chassis_can_send_data[6] = motor4 >> 8;
    chassis_can_send_data[7] = motor4;
    HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
}

/**
  * @brief          return the chassis 3508 motor data point
  * @param[in]      i: motor number,range [0,3]
  * @retval         motor data point
  */
/**
  * @brief          返回底盘电机 3508电机数据指针
  * @param[in]      i: 电机编号,范围[0,3]
  * @retval         电机数据指针
  */
const motor_measure_t *get_chassis_motor_measure_point(uint8_t motor_index)
{
	if (motor_index >= MOTOR_LIST_LENGTH)
	{
		return NULL;
	}
	else
	{
		return &motor_chassis[motor_index];
	}
}
