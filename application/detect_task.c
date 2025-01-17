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
  
#include "detect_task.h"
#include "cmsis_os.h"
#include "cv_usart_task.h"
#include "chassis_task.h"

#define DETECT_TEST_MODE 0

/**
  * @brief          init error_list, assign  offline_time, online_time, priority.
  * @param[in]      time: system time
  * @retval         none
  */
static void detect_init(uint32_t time);




error_t error_list[ERROR_LIST_LENGTH + 1];

#if DETECT_TEST_MODE
uint16_t detect_intervals[ERROR_LIST_LENGTH];
static void J_scope_detect_test(uint16_t ulSystemTime)
{
    for (int i = 0; i < ERROR_LIST_LENGTH; i++)
    {
        detect_intervals[i] = ulSystemTime - error_list[i].new_time;
    }
}
#endif

#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t detect_task_stack;
#endif


/**
  * @brief          detect task
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
void detect_task(void const *pvParameters)
{
    uint32_t ulSystemTime = osKernelSysTick();
    detect_init(ulSystemTime);
    osDelay(DETECT_TASK_INIT_TIME);

    while (1)
    {        
        static uint8_t error_num_display = 0;
        error_num_display = ERROR_LIST_LENGTH;
        error_list[ERROR_LIST_LENGTH].is_lost = 0;
        error_list[ERROR_LIST_LENGTH].error_exist = 0;

        for (int i = 0; i < ERROR_LIST_LENGTH; i++)
        {
            if (error_list[i].enable == 0)
            {
                continue;
            }

            //judge offline
            if (ulSystemTime - error_list[i].new_time > error_list[i].set_offline_time)
            {
                if (error_list[i].error_exist == 0)
                {
                    //record error and offlined timestamp
                    error_list[i].is_lost = 1;
                    error_list[i].error_exist = 1;
                    error_list[i].lost_time = ulSystemTime;
                }
                //save the error code of the highest priority
                if (error_list[i].priority > error_list[error_num_display].priority)
                {
                    error_num_display = i;
                }
                

                error_list[ERROR_LIST_LENGTH].is_lost = 1;
                error_list[ERROR_LIST_LENGTH].error_exist = 1;
                //if solve_lost_fun != NULL, run it
                if (error_list[i].solve_lost_fun != NULL)
                {
                    error_list[i].solve_lost_fun();
                }
            }
            else if (ulSystemTime - error_list[i].work_time < error_list[i].set_online_time)
            {
                //just online, maybe unstable, only record
                error_list[i].is_lost = 0;
                error_list[i].error_exist = 1;
            }
            else
            {
                error_list[i].is_lost = 0;
                //judge if exist data error
                if (error_list[i].data_is_error != NULL)
                {
                    error_list[i].error_exist = 1;
                }
                else
                {
                    error_list[i].error_exist = 0;
                }
                //calc frequency
                if (error_list[i].new_time > error_list[i].last_time)
                {
                    error_list[i].frequency = configTICK_RATE_HZ / (fp32)(error_list[i].new_time - error_list[i].last_time);
                }
            }
        }
#if DETECT_TEST_MODE
        J_scope_detect_test(ulSystemTime);
#endif

        osDelayUntil(&ulSystemTime, DETECT_CONTROL_TIME_MS);

#if INCLUDE_uxTaskGetStackHighWaterMark
        detect_task_stack = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}


/**
  * @brief          get toe error status
  * @param[in]      toe: table of equipment
  * @retval         true (eror) or false (no error)
  */
bool_t toe_is_error(uint8_t toe)
{
    return (error_list[toe].error_exist == 1);
}

/**
  * @brief          record the time
  * @param[in]      toe: table of equipment
  * @retval         none
  */
void detect_hook(uint8_t toe)
{
    error_list[toe].last_time = error_list[toe].new_time;
    error_list[toe].new_time = xTaskGetTickCount();
    
    if (error_list[toe].is_lost)
    {
        error_list[toe].is_lost = 0;
        error_list[toe].work_time = error_list[toe].new_time;
    }
    
    if (error_list[toe].data_is_error_fun != NULL)
    {
        if (error_list[toe].data_is_error_fun())
        {
            error_list[toe].error_exist = 1;
            error_list[toe].data_is_error = 1;

            if (error_list[toe].solve_data_error_fun != NULL)
            {
                error_list[toe].solve_data_error_fun();
            }
        }
        else
        {
            error_list[toe].data_is_error = 0;
        }
    }
    else
    {
        error_list[toe].data_is_error = 0;
    }
}

/**
  * @brief          get error list
  * @param[in]      none
  * @retval         the point of error_list
  */
const error_t *get_error_list_point(void)
{
    return error_list;
}

// extern void OLED_com_reset(void);
static void detect_init(uint32_t time)
{
    // Important: config offlineTime onlinetime priority
    uint16_t set_item[ERROR_LIST_LENGTH][3] =
        {
            {50, 40, 15},   //SBUS
            {20, 10, 11},   //motor1
            {20, 10, 10},   //motor2
            {20, 10, 9},    //motor3
            {20, 10, 8},    //motor4
            {20, 3, 14},     //yaw
            {20, 3, 13},     //pitch
            {20, 10, 12},   //trigger
            {20, 10, 16},   //fric 1
            {20, 10, 17},   //fric 2
            {20, 3, 7},      //board gyro
            {20, 5, 7},      //board accel
            {40, 200, 7},   //board mag
            {200, 20, 5},  //referee
            {300, 0, 7},    //cv usart
            {100, 0, 11},    // super capacitor
            {50, 0, 8},    // swerve controller
            {25, 0, 8},    // biped controller feedback (normally < 3)
            // {100, 100, 1},  //oled
        };

    for (uint8_t i = 0; i < ERROR_LIST_LENGTH; i++)
    {
        error_list[i].set_offline_time = set_item[i][0];
        error_list[i].set_online_time = set_item[i][1];
        error_list[i].priority = set_item[i][2];
        error_list[i].data_is_error_fun = NULL;
        error_list[i].solve_lost_fun = NULL;
        error_list[i].solve_data_error_fun = NULL;

        error_list[i].enable = 1;
        error_list[i].error_exist = 1;
        error_list[i].is_lost = 1;
        error_list[i].data_is_error = 1;

		error_list[i].frequency = 0.0f;
        error_list[i].new_time = time;
        error_list[i].last_time = time;
        error_list[i].lost_time = time;
        error_list[i].work_time = time;
    }
#if CV_INTERFACE
    error_list[CV_TOE].solve_lost_fun = CvCmder_toe_solve_lost_fun;
#endif

#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
	error_list[SWERVE_CTRL_TOE].solve_lost_fun = swerve_chassis_params_reset;
#elif (ROBOT_TYPE == INFANTRY_2024_BIPED)
	error_list[BIPED_CTRL_TOE].solve_lost_fun = biped_chassis_params_reset;
#endif

	// error_list[OLED_TOE].data_is_error_fun = NULL;
    // error_list[OLED_TOE].solve_lost_fun = OLED_com_reset;
    // error_list[OLED_TOE].solve_data_error_fun = NULL;

//    error_list[DBUS_TOE].data_is_error_fun = RC_data_is_error;
//    error_list[DBUS_TOE].solve_lost_fun = solve_RC_lost;
//    error_list[DBUS_TOE].solve_data_error_fun = solve_data_error;

}

uint8_t ifToeStatusExist(uint8_t _start, uint8_t _end, toe_status_e _status_to_find, uint8_t* pbHitIndex)
{
    // range is inclusive = [_start, _end]
    // _status_to_find = TOE_STATUS_OFFLINE checks whether offline equipment exist, TOE_STATUS_ONLINE checks whether online equipment exist
    uint8_t fStatusExist = 1;
	if (_start <= _end)
	{
		fStatusExist = 0;
		uint8_t bToeIndex;
		for (bToeIndex = _start; bToeIndex <= _end; bToeIndex++)
		{
			if (toe_is_error(bToeIndex) == _status_to_find)
			{
				fStatusExist = 1;
				break;
			}
		}

        if (pbHitIndex != NULL)
        {
            *pbHitIndex = fStatusExist ? bToeIndex : 0;
        }
	}
	return fStatusExist;
}
