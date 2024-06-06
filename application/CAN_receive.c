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
#include "CAN_receive.h"

#include "cmsis_os.h"

#include "main.h"
#include "bsp_rng.h"

#include "detect_task.h"
#include "chassis_task.h"
#include "string.h"
#include "referee.h"

// Warning: for safety, PLEASE ALWAYS keep those default values as 0 when you commit
// Warning: because #if directive will assume the expression as 0 even if the macro is not defined, positive logic, for example, ENABLE_MOTOR_POWER, is safer that if and only if it's defined and set to 1 that the power is enabled
#define ENABLE_DRIVE_MOTOR_POWER 0
#define ENABLE_STEER_MOTOR_POWER 0
#define ENABLE_YAW_MOTOR_POWER 0
#define ENABLE_PITCH_MOTOR_POWER 0
#define ENABLE_TRIGGER_MOTOR_POWER 0
#define ENABLE_FRICTION_1_MOTOR_POWER 0
#define ENABLE_FRICTION_2_MOTOR_POWER 0
#define ENABLE_UPPER_HEAD_POWER 0


#define REVERSE_M3508_1 0
#define REVERSE_M3508_2 0
#define REVERSE_M3508_3 0
#define REVERSE_M3508_4 0

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
//motor data read
#define get_motor_measure(ptr, data)                                    \
    {                                                                   \
        (ptr)->last_ecd = (ptr)->ecd;                                   \
        (ptr)->ecd = (uint16_t)((data)[0] << 8 | (data)[1]);            \
        (ptr)->speed_rpm = (int16_t)((data)[2] << 8 | (data)[3]);      \
        (ptr)->given_current = (int16_t)((data)[4] << 8 | (data)[5]);  \
        (ptr)->temperate = (data)[6];                                   \
    }

uint8_t convertCanIdToMotorIndex(uint32_t canId);
void reverse_motor_feedback(uint8_t bMotorId);

/**
 * @brief motor feedback data
 * Chassis CAN:
 * 0:chassis motor1 3508; 1:chassis motor2 3508; 2:chassis motor3 3508; 3:chassis motor4 3508;
 * 6:trigger motor 2006; 4:yaw gimbal motor 6020;
 * 
 * Gimbal CAN:
 * 5:pitch gimbal motor 6020;
 */
motor_measure_t motor_chassis[MOTOR_LIST_LENGTH];

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
		case CAN_YAW_MOTOR_ID:
		case CAN_PIT_MOTOR_ID:
		case CAN_TRIGGER_MOTOR_ID:
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
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    uint8_t fIsMotor = 0;
    uint8_t bMotorId = 0;
    uint8_t fIdIdentified = 0;

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
    
    if (hcan == &GIMBAL_CAN) {
        switch (rx_header.StdId) {
            case CAN_PIT_MOTOR_ID:
#if (ROBOT_TYPE == INFANTRY_2023_MECANUM) || (ROBOT_TYPE == INFANTRY_2024_MECANUM)
            case CAN_TRIGGER_MOTOR_ID:
#endif
            {
                fIsMotor = 1;
                fIdIdentified = 0;
                break;
            }
            case CAN_FRICTION_MOTOR_LEFT_ID:
            {
                fIsMotor = 1;
                bMotorId = MOTOR_INDEX_FRICTION_LEFT;
                fIdIdentified = 1;
                break;
            }
            case CAN_FRICTION_MOTOR_RIGHT_ID:
            {
                fIsMotor = 1;
                bMotorId = MOTOR_INDEX_FRICTION_RIGHT;
                fIdIdentified = 1;
                break;
            }
            default:
            {
                break;
            }
        }
    } else if (hcan == &CHASSIS_CAN) {
        switch (rx_header.StdId) {
            case CAN_3508_M1_ID:
            case CAN_3508_M2_ID:
            case CAN_3508_M3_ID:
            case CAN_3508_M4_ID:
    #if (ROBOT_TYPE == INFANTRY_2023_SWERVE) || (ROBOT_TYPE == SENTRY_2023_MECANUM)
            case CAN_TRIGGER_MOTOR_ID:
    #endif
            case CAN_YAW_MOTOR_ID:
            {
                fIsMotor = 1;
                fIdIdentified = 0;
                break;
            }
            case SUPCAP_RX_ID:
            {
                fIsMotor = 0;
                memcpy(cap_message_rx.can_buf, rx_data, sizeof(rx_data));
                detect_hook(SUPCAP_TOE);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if (fIsMotor == 1)
    {
        if (fIdIdentified == 0)
        {
            bMotorId = convertCanIdToMotorIndex(rx_header.StdId);
        }
        get_motor_measure(&motor_chassis[bMotorId], rx_data);
        reverse_motor_feedback(bMotorId);
        detect_hook(CHASSIS_MOTOR1_TOE + bMotorId);
    }
}

void reverse_motor_feedback(uint8_t bMotorId)
{
#if (REVERSE_M3508_1 || REVERSE_M3508_2 || REVERSE_M3508_3 || REVERSE_M3508_4)
	switch (bMotorId)
	{
#if REVERSE_M3508_1
		case MOTOR_INDEX_3508_M1:
#endif
#if REVERSE_M3508_2
		case MOTOR_INDEX_3508_M2:
#endif
#if REVERSE_M3508_3
		case MOTOR_INDEX_3508_M3:
#endif
#if REVERSE_M3508_4
		case MOTOR_INDEX_3508_M4:
#endif
		{
	        motor_chassis[bMotorId].ecd = (motor_chassis[bMotorId].ecd + HALF_ECD_RANGE) % ECD_RANGE;
	        motor_chassis[bMotorId].speed_rpm = -motor_chassis[bMotorId].speed_rpm;
			break;
		}
		default:
		{
			break;
		}
	}
#endif
}

/**
  * @brief          send control current of motor (0x205, 0x206, 0x207, 0x208)
  * @param[in]      yaw: (0x205) 6020 motor control current, range [-30000,30000] 
  * @param[in]      pitch: (0x206) 6020 motor control current, range [-30000,30000]
  * @param[in]      trigger: (0x207) 2006 motor control current, range [-10000,10000]
  * @param[in]      fric_left: 3508 motor control current when used as friction motor
  * @param[in]      fric_right: 3508 motor control current when used as friction motor
  * @retval         none
  */
void CAN_cmd_gimbal(int16_t yaw, int16_t pitch, int16_t trigger, int16_t fric_left, int16_t fric_right)
{
    uint32_t send_mail_box;
    gimbal_tx_message.StdId = CAN_GIMBAL_ALL_TX_ID;
    gimbal_tx_message.IDE = CAN_ID_STD;
    gimbal_tx_message.RTR = CAN_RTR_DATA;
    gimbal_tx_message.DLC = 0x08;

#if (ENABLE_YAW_MOTOR_POWER == 0)
    yaw = 0;
#endif
#if (ENABLE_TRIGGER_MOTOR_POWER == 0)
    trigger = 0;
#endif
#if (ENABLE_PITCH_MOTOR_POWER == 0)
    pitch = 0;
#endif
#if ((ENABLE_FRICTION_1_MOTOR_POWER == 0) || (ENABLE_SHOOT_REDUNDANT_SWITCH == 0))
    fric_left = 0;
#endif
#if ((ENABLE_FRICTION_2_MOTOR_POWER == 0) || (ENABLE_SHOOT_REDUNDANT_SWITCH == 0))
    fric_right = 0;
#endif

    // control yaw motor and trigger motor
    gimbal_can_send_data[0] = (yaw >> 8);
    gimbal_can_send_data[1] = yaw;
    // gimbal_can_send_data[2] = (rev >> 8);
    // gimbal_can_send_data[3] = rev;
    gimbal_can_send_data[4] = (trigger >> 8);
    gimbal_can_send_data[5] = trigger;
    // gimbal_can_send_data[6] = (rev >> 8);
    // gimbal_can_send_data[7] = rev;
    HAL_CAN_AddTxMessage(&CHASSIS_CAN, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);

    // control pitch motor and fric_left and fric_right
    gimbal_can_send_data[0] = (fric_left >> 8);
    gimbal_can_send_data[1] = fric_left;
    gimbal_can_send_data[2] = (pitch >> 8);
    gimbal_can_send_data[3] = pitch;
    // gimbal_can_send_data[4] = (rev >> 8);
    // gimbal_can_send_data[5] = rev;
    gimbal_can_send_data[6] = (fric_right >> 8);
    gimbal_can_send_data[7] = fric_right;
    HAL_CAN_AddTxMessage(&GIMBAL_CAN, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);
}

/**
  * @brief          send CAN packet of ID 0x700, it will set chassis motor 3508 to quick ID setting
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

#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
void CAN_cmd_upper_head(void)
{
#if ENABLE_UPPER_HEAD_POWER
    uint32_t send_mail_box;
    chassis_tx_message.StdId = CAN_LOWER_HEAD_TX_ID;
    chassis_tx_message.IDE = CAN_ID_STD;
    chassis_tx_message.RTR = CAN_RTR_DATA;
    chassis_tx_message.DLC = 0x08;

    uint16_t shoot_heat_limit = 0;
    uint16_t shoot_heat1 = 0;
    get_shoot_heat1_limit_and_heat(&shoot_heat_limit, &shoot_heat1);

    chassis_can_send_data[0] = (shoot_heat_limit >> 8);
    chassis_can_send_data[1] = shoot_heat_limit;
    chassis_can_send_data[2] = (shoot_heat1 >> 8);
    chassis_can_send_data[3] = shoot_heat1;
    // chassis_can_send_data[4] = rev;
    // chassis_can_send_data[5] = rev;
    // chassis_can_send_data[6] = rev;
    // chassis_can_send_data[7] = rev;
    HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
#endif
}
#endif

/**
  * @brief          send control current or voltage of motor. Refer to can_msg_id_e for motor IDs
  * @param[in]      motor1: (0x201) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor2: (0x202) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor3: (0x203) 3508 motor control current, range [-16384,16384] 
  * @param[in]      motor4: (0x204) 3508 motor control current, range [-16384,16384] 
  * @param[in]      steer_motor1: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @param[in]      steer_motor2: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @param[in]      steer_motor3: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @param[in]      steer_motor4: target encoder value of 6020 motor; it's moved to a bus only controlled by chassis controller to reduce bus load
  * @retval         none
  */
void CAN_cmd_chassis(void)
{
    uint32_t send_mail_box;
    // driver motors (M3508)
    chassis_tx_message.StdId = CAN_CHASSIS_M3508_TX_ID;
    chassis_tx_message.IDE = CAN_ID_STD;
    chassis_tx_message.RTR = CAN_RTR_DATA;
    chassis_tx_message.DLC = 0x08;

#if ENABLE_DRIVE_MOTOR_POWER
    int16_t motor1 = chassis_move.motor_chassis[0].give_current;
    int16_t motor2 = chassis_move.motor_chassis[1].give_current;
    int16_t motor3 = chassis_move.motor_chassis[2].give_current;
    int16_t motor4 = chassis_move.motor_chassis[3].give_current;
#else
    int16_t motor1 = 0;
    int16_t motor2 = 0;
    int16_t motor3 = 0;
    int16_t motor4 = 0;
#endif

#if REVERSE_M3508_1
    motor1 = -motor1;
#endif

#if REVERSE_M3508_2
    motor2 = -motor2;
#endif

#if REVERSE_M3508_3
    motor3 = -motor3;
#endif

#if REVERSE_M3508_4
    motor4 = -motor4;
#endif

    chassis_can_send_data[0] = motor1 >> 8;
    chassis_can_send_data[1] = motor1;
    chassis_can_send_data[2] = motor2 >> 8;
    chassis_can_send_data[3] = motor2;
    chassis_can_send_data[4] = motor3 >> 8;
    chassis_can_send_data[5] = motor3;
    chassis_can_send_data[6] = motor4 >> 8;
    chassis_can_send_data[7] = motor4;
    HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
    // Send target encoder value of steering motors (GM6020) to chassis controller
    chassis_tx_message.StdId = CAN_CHASSIS_CONTROLLER_TX_ID;

#if ENABLE_STEER_MOTOR_POWER
    uint16_t steer_motor1 = chassis_move.steer_motor_chassis[0].target_ecd;
    uint16_t steer_motor2 = chassis_move.steer_motor_chassis[1].target_ecd;
    uint16_t steer_motor3 = chassis_move.steer_motor_chassis[2].target_ecd;
    uint16_t steer_motor4 = chassis_move.steer_motor_chassis[3].target_ecd;

    chassis_can_send_data[0] = steer_motor1 >> 8;
    chassis_can_send_data[1] = steer_motor1;
    chassis_can_send_data[2] = steer_motor2 >> 8;
    chassis_can_send_data[3] = steer_motor2;
    chassis_can_send_data[4] = steer_motor3 >> 8;
    chassis_can_send_data[5] = steer_motor3;
    chassis_can_send_data[6] = steer_motor4 >> 8;
    chassis_can_send_data[7] = steer_motor4;
#else
    memset(chassis_can_send_data, 0xFF, sizeof(chassis_can_send_data));
#endif
    HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
#endif
}

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
void CAN_cmd_load_servo(uint8_t fServoSwitch, uint8_t bTrialTimes)
{
    // Turn on/off loading servo motor, by commanding Type-A board on chassis
    uint32_t send_mail_box;
    chassis_tx_message.StdId = CAN_CHASSIS_LOAD_SERVO_TX_ID;
    chassis_tx_message.IDE = CAN_ID_STD;
    chassis_tx_message.RTR = CAN_RTR_DATA;
    chassis_tx_message.DLC = 0x08;
    chassis_can_send_data[0] = fServoSwitch;
    for (uint8_t i = 0; i < bTrialTimes; i++)
    {
        HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
        osDelay(1);
    }
}
#endif

/**
  * @brief          return the yaw 6020 motor data point
  * @param[in]      none
  * @retval         motor data point
  */
const motor_measure_t *get_yaw_gimbal_motor_measure_point(void)
{
    return &motor_chassis[MOTOR_INDEX_YAW];
}

/**
  * @brief          return the pitch 6020 motor data point
  * @param[in]      none
  * @retval         motor data point
  */
const motor_measure_t *get_pitch_gimbal_motor_measure_point(void)
{
    return &motor_chassis[MOTOR_INDEX_PITCH];
}

/**
  * @brief          return the chassis 3508 motor data point
  * @param[in]      i: motor number,range [0,3]
  * @retval         motor data point
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
