/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       detect_task.c/h
  * @brief      detect error task, judged by receiving data time. provide detect
                hook function, error exist function.
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Nov-11-2019     RM              1. add oled, gyro accel and mag sensors
  *
  @verbatim
  ==============================================================================
    add a sensor 
    1. in detect_task.h, add the sensor name at the end of errorList,like
    enum errorList
    {
        ...
        XXX_TOE,    //new sensor
        ERROR_LIST_LENGTH,
    };
    2.in detect_init function, add the offlineTime, onlinetime, priority params,like
        uint16_t set_item[ERROR_LIST_LENGTH][3] =
        {
            ...
            {n,n,n}, //XX_TOE
        };
    3. if XXX_TOE has data_is_error_fun ,solve_lost_fun,solve_data_error_fun function, 
        please assign to function pointer.
    4. when XXX_TOE sensor data come, add the function detect_hook(XXX_TOE) function.
  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
  
#ifndef DETECT_TASK_H
#define DETECT_TASK_H
#include "global_inc.h"


#define DETECT_TASK_INIT_TIME 57
#define DETECT_CONTROL_TIME_MS 10

// TOE: table of equipment
enum errorList
{
    // chassis related TOEs (make sure this group is continuous)
    DBUS_TOE = 0,
    // M3508 control speed
    CHASSIS_MOTOR1_TOE,
    CHASSIS_MOTOR2_TOE,
    CHASSIS_MOTOR3_TOE,
    CHASSIS_MOTOR4_TOE,
    // "gimbal" motors
    YAW_GIMBAL_MOTOR_TOE,
    PITCH_GIMBAL_MOTOR_TOE,
    // shoot motors
    TRIGGER_MOTOR_TOE,
    FRIC1_MOTOR_TOE,
    FRIC2_MOTOR_TOE,
    // on-board IMU
    BOARD_GYRO_TOE,
    BOARD_ACCEL_TOE,
    BOARD_MAG_TOE,
    // others
    REFEREE_TOE,
    CV_TOE,
    SUPCAP_TOE,
    SWERVE_CTRL_TOE,
    BIPED_CTRL_TOE,
    // OLED_TOE,
    ERROR_LIST_LENGTH,
};

typedef enum
{
  TOE_STATUS_ONLINE = 0,
  TOE_STATUS_OFFLINE = 1,
} toe_status_e;

typedef __packed struct
{
    uint32_t new_time;
    uint32_t last_time;
    uint32_t lost_time;
    uint32_t work_time;
    uint16_t set_offline_time : 12;
    uint16_t set_online_time : 12;
    uint8_t enable : 1;
    uint8_t priority : 4;
    uint8_t is_lost : 1;
    uint8_t data_is_error : 1;
    uint8_t error_exist;

    fp32 frequency;
    bool_t (*data_is_error_fun)(void);
    void (*solve_lost_fun)(void);
    void (*solve_data_error_fun)(void);
} error_t;


/**
  * @brief          detect task
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
extern void detect_task(void const *pvParameters);

/**
  * @brief          get toe error status
  * @param[in]      toe: table of equipment
  * @retval         true (eror) or false (no error)
  */
extern bool_t toe_is_error(uint8_t err);

/**
  * @brief          record the time
  * @param[in]      toe: table of equipment
  * @retval         none
  */
extern void detect_hook(uint8_t toe);

/**
  * @brief          get error list
  * @param[in]      none
  * @retval         the point of error_list
  */
extern const error_t *get_error_list_point(void);

uint8_t ifToeStatusExist(uint8_t _start, uint8_t _end, toe_status_e _status_to_find, uint8_t* pbHitIndex);

extern uint32_t toe_check(void);
#endif
