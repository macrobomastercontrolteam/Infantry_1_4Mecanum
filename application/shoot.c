/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#include "shoot.h"
#include "main.h"

#include "cmsis_os.h"

#include "bsp_laser.h"
#include "bsp_fric.h"
#include "arm_math.h"
#include "user_lib.h"
#include "referee.h"

#include "CAN_receive.h"
#include "gimbal_behaviour.h"
#include "detect_task.h"
#include "pid.h"
#include "cv_usart_task.h"

#if DISABLE_SHOOT_MOTOR_POWER
#define shoot_fric1_on(pwm) fric_off()
#define shoot_fric2_on(pwm) fric_off()
#else
#define shoot_fric1_on(pwm) fric1_on((pwm))
#define shoot_fric2_on(pwm) fric2_on((pwm))
#endif

#define shoot_fric_off() fric_off()

#define shoot_laser_on() laser_on()
#define shoot_laser_off() laser_off()
//microswitch
#define BUTTEN_TRIG_PIN HAL_GPIO_ReadPin(BUTTON_TRIG_GPIO_Port, BUTTON_TRIG_Pin)

/**
 * @brief Set the mode of the shoot control state machine
*/
static void shoot_set_mode(void);
/**
  * @brief          Update shoot data
  * @param[in]      void
  * @retval         void
  */
static void shoot_feedback_update(void);

/**
  * @brief          Handle stall by oscillating the trigger motor
  * @param[in]      void
  * @retval         void
  */
static void trigger_motor_turn_back(void);

/**
  * @brief          Shoot control, control the angle of the trigger motor to complete a single burst shot
  * @param[in]      void
  * @retval         void
  */
static void shoot_bullet_control(void);

shoot_control_t shoot_control;

/**
  * @brief          Initialize the shoot control, including PID, remote control pointer, and motor pointer
  * @param[in]      void
  */
void shoot_init(void)
{

    static const fp32 Trigger_speed_pid[3] = {TRIGGER_ANGLE_PID_KP, TRIGGER_ANGLE_PID_KI, TRIGGER_ANGLE_PID_KD};
    shoot_control.shoot_mode = SHOOT_STOP_INIT;
    shoot_control.shoot_rc = get_remote_control_point();
    shoot_control.shoot_motor_measure = get_trigger_motor_measure_point();
    PID_init(&shoot_control.trigger_motor_pid, PID_POSITION, Trigger_speed_pid, TRIGGER_READY_PID_MAX_OUT, TRIGGER_READY_PID_MAX_IOUT, &raw_err_handler);
    shoot_feedback_update();
    ramp_init(&shoot_control.fric1_ramp, SHOOT_CONTROL_TIME * 0.001f, FRIC_DOWN, FRIC_OFF);
    ramp_init(&shoot_control.fric2_ramp, SHOOT_CONTROL_TIME * 0.001f, FRIC_DOWN, FRIC_OFF);
    shoot_control.fric_pwm1 = FRIC_OFF;
    shoot_control.fric_pwm2 = FRIC_OFF;
    shoot_control.ecd_count = 0;
    shoot_control.angle = shoot_control.shoot_motor_measure->ecd * MOTOR_ECD_TO_RAD / TRIGGER_MOTOR_GEAR_RATIO;
    shoot_control.given_current = 0;
    shoot_control.move_flag = 0;
    shoot_control.set_angle = shoot_control.angle;
    shoot_control.speed = 0.0f;
    shoot_control.speed_set = 0.0f;
    shoot_control.key_time = 0;
    shoot_control.fIsCvControl = 0;
}

int16_t shoot_control_loop(void)
{

    shoot_set_mode();
    shoot_feedback_update();

    shoot_control.speed_set = 0.0f;
    switch (shoot_control.shoot_mode)
    {
    case SHOOT_STOP_INIT:
    {
#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
        CAN_cmd_load_servo(0);
        CAN_cmd_load_servo(0);
        CAN_cmd_load_servo(0);
#endif
        shoot_control.shoot_mode = SHOOT_STOP;
        // no break; directly go to next state
    }
    case SHOOT_STOP:
    {
        break;
    }
    case SHOOT_READY_FRIC:
    {
        if ((shoot_control.fric1_ramp.out == shoot_control.fric1_ramp.max_value) && (shoot_control.fric2_ramp.out == shoot_control.fric2_ramp.max_value))
        {
            shoot_control.shoot_mode = SHOOT_READY_BULLET_INIT;
        }
        break;
    }
    case SHOOT_READY_BULLET_INIT:
    {
#if (ROBOT_TYPE == INFANTRY_2023_SWERVE)
        CAN_cmd_load_servo(1);
        CAN_cmd_load_servo(1);
        CAN_cmd_load_servo(1);
#endif
		shoot_control.trigger_motor_pid.max_out = TRIGGER_READY_PID_MAX_OUT;
        shoot_control.trigger_motor_pid.max_iout = TRIGGER_READY_PID_MAX_IOUT;
        shoot_control.shoot_mode = SHOOT_READY_BULLET;
        // no break; directly go to next state
    }
    case SHOOT_READY_BULLET:
    {
        if (shoot_control.key == SWITCH_TRIGGER_OFF)
        {
            // set the speed of the trigger motor, and enable stall reverse processing
            shoot_control.trigger_speed_set = READY_TRIGGER_SPEED;
            trigger_motor_turn_back();
        }
        // else
        // {
        //     shoot_control.trigger_speed_set = 0.0f;
        //     shoot_control.shoot_mode = SHOOT_READY;
        // }
        break;
    }
    case SHOOT_READY:
    {
        if (shoot_control.key == SWITCH_TRIGGER_OFF)
        {
            shoot_control.shoot_mode = SHOOT_READY_BULLET_INIT;
        }
//         else if (shoot_control.fIsCvControl == 0)
//         {
//             // long press mouse to rapid fire
//             if ((shoot_control.press_l && shoot_control.last_press_l == 0) || (shoot_control.press_r && shoot_control.last_press_r == 0))
//             {
//                 shoot_control.shoot_mode = SHOOT_BULLET;
//             }
//         }
        break;
    }
    case SHOOT_BULLET:
    {
        shoot_control.trigger_motor_pid.max_out = TRIGGER_BULLET_PID_MAX_OUT;
        shoot_control.trigger_motor_pid.max_iout = TRIGGER_BULLET_PID_MAX_IOUT;
        shoot_bullet_control();

        get_shoot_heat0_limit_and_heat0(&shoot_control.heat_limit, &shoot_control.heat);
        if (!toe_is_error(REFEREE_TOE) && (shoot_control.heat + SHOOT_HEAT_REMAIN_VALUE > shoot_control.heat_limit))
        {
            shoot_control.shoot_mode = SHOOT_READY_BULLET_INIT;
        }
        break;
    }
    case SHOOT_CONTINUE_BULLET:
    {
        // set the speed of the trigger motor, and enable stall reverse processing
        shoot_control.trigger_speed_set = CONTINUE_TRIGGER_SPEED;
        trigger_motor_turn_back();
        shoot_control.shoot_mode = SHOOT_READY_BULLET_INIT;
        break;
    }
    case SHOOT_DONE:
    {
        if (shoot_control.key == SWITCH_TRIGGER_OFF)
        {
            shoot_control.key_time++;
            if (shoot_control.key_time > SHOOT_DONE_KEY_OFF_TIME)
            {
                shoot_control.key_time = 0;
                shoot_control.shoot_mode = SHOOT_READY_BULLET_INIT;
            }
        }
        // else
        // {
        //     shoot_control.key_time = 0;
        //     shoot_control.shoot_mode = SHOOT_BULLET;
        // }
        break;
    }
    }

    // Continue bullet logic: must be used after state machine, since SHOOT_CONTINUE_BULLET switch back to SHOOT_READY_BULLET if not triggered
	if (shoot_control.shoot_mode > SHOOT_READY_FRIC)
	{
		if (shoot_control.fIsCvControl)
		{
			shoot_control.shoot_mode = SHOOT_CONTINUE_BULLET;
		}
        // Enter shooting state by long pressing the mouse to keep firing
		else if ((shoot_control.press_l_time == PRESS_LONG_TIME) || (shoot_control.press_r_time == PRESS_LONG_TIME) || (shoot_control.rc_s_time == RC_S_LONG_TIME))
		{
			get_shoot_heat0_limit_and_heat0(&shoot_control.heat_limit, &shoot_control.heat);
			if (!toe_is_error(REFEREE_TOE) && (shoot_control.heat + SHOOT_HEAT_REMAIN_VALUE > shoot_control.heat_limit))
			{
				shoot_control.shoot_mode = SHOOT_READY_BULLET_INIT;
			}
			else
			{
				shoot_control.shoot_mode = SHOOT_CONTINUE_BULLET;
			}
		}
	}

	if(shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_laser_off();
        shoot_control.given_current = 0;
        // Friction wheels need to be turned on one by one with a ramp, otherwise the motor may not turn
        ramp_calc(&shoot_control.fric1_ramp, -SHOOT_FRIC_PWM_ADD_VALUE);
        ramp_calc(&shoot_control.fric2_ramp, -SHOOT_FRIC_PWM_ADD_VALUE);
    }
    else
    {
        shoot_laser_on();
        // calculate the PID of the trigger motor
        PID_calc(&shoot_control.trigger_motor_pid, shoot_control.speed, shoot_control.speed_set);
        shoot_control.given_current = (int16_t)(shoot_control.trigger_motor_pid.out);
        if(shoot_control.shoot_mode < SHOOT_READY_BULLET_INIT)
        {
            shoot_control.given_current = 0;
        }
        // friction wheels need to be turned on individually with ramps, otherwise the motor may not turn
        ramp_calc(&shoot_control.fric1_ramp, SHOOT_FRIC_PWM_ADD_VALUE);
        ramp_calc(&shoot_control.fric2_ramp, SHOOT_FRIC_PWM_ADD_VALUE);

    }

    shoot_control.fric_pwm1 = (uint16_t)(shoot_control.fric1_ramp.out);
    shoot_control.fric_pwm2 = (uint16_t)(shoot_control.fric2_ramp.out);
    shoot_fric1_on(shoot_control.fric_pwm1);
    shoot_fric2_on(shoot_control.fric_pwm2);
    return shoot_control.given_current;
}

/**
  * @brief          Set the shoot mode state machine, pull up once on the remote control to turn on, pull up again to turn off, pull down once to shoot one, always down to continue shooting, used to clean up bullets during the 3-minute preparation time
  * @param[in]      void
  * @retval         void
  */
static void shoot_set_mode(void)
{
#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
	shoot_control.fIsCvControl = (shoot_control.shoot_rc->rc.s[RC_RIGHT_LEVER_CHANNEL] == RC_SW_UP);
#else
    // shoot_control.fIsCvControl = 0; // It is 0 by default
#endif

#if (ROBOT_TYPE == SENTRY_2023_MECANUM)
	if (shoot_control.fIsCvControl)
	{
		static uint8_t lastCvShootMode = 0;
		uint8_t CvShootMode = CvCmder_GetMode(CV_MODE_SHOOT_BIT);
		if (CvShootMode != lastCvShootMode)
		{
			if (CvShootMode == 1)
			{
				shoot_control.shoot_mode = SHOOT_READY_FRIC;
			}
			else
			{
				shoot_control.shoot_mode = SHOOT_STOP_INIT;
			}
			lastCvShootMode = CvShootMode;
		}
	}
	else
#endif
	{
        // normal RC control
		if (gimbal_cmd_to_shoot_stop())
		{
			shoot_control.shoot_mode = SHOOT_STOP;
		}
		else
		{
			// remote controller S1 switch logic
			static int8_t last_s = RC_SW_UP;
			int8_t new_s = shoot_control.shoot_rc->rc.s[RC_LEFT_LEVER_CHANNEL];
			switch (new_s)
			{
				case RC_SW_UP:
				{
					if (last_s != RC_SW_UP)
					{
						shoot_control.shoot_mode = SHOOT_READY_FRIC;
					}
					break;
				}
				case RC_SW_MID:
				{
					if (shoot_control.shoot_mode != SHOOT_STOP)
					{
						shoot_control.shoot_mode = SHOOT_STOP_INIT;
					}
					break;
				}
				case RC_SW_DOWN:
				{
					if (last_s != RC_SW_DOWN)
					{
						if (shoot_control.shoot_mode == SHOOT_READY)
						{
							// burst fire
							shoot_control.shoot_mode = SHOOT_BULLET;
						}
					}
					break;
				}
			}
			last_s = new_s;
		}
	}
}

static void shoot_feedback_update(void)
{

    static fp32 speed_fliter_1 = 0.0f;
    static fp32 speed_fliter_2 = 0.0f;
    static fp32 speed_fliter_3 = 0.0f;

    // filter coefficient for trigger motor speed
    static const fp32 fliter_num[3] = {1.725709860247969f, -0.75594777109163436f, 0.030237910843665373f};

    // second-order low-pass filter
    speed_fliter_1 = speed_fliter_2;
    speed_fliter_2 = speed_fliter_3;
    speed_fliter_3 = speed_fliter_2 * fliter_num[0] + speed_fliter_1 * fliter_num[1] + (RPM_TO_RADS(shoot_control.shoot_motor_measure->speed_rpm) / TRIGGER_MOTOR_GEAR_RATIO) * fliter_num[2];
    shoot_control.speed = speed_fliter_3;

    // reset the motor count, because when the output shaft rotates one turn, the motor shaft rotates 36 turns, process the motor shaft data into output shaft data, used to control the output shaft angle
    if (shoot_control.shoot_motor_measure->ecd - shoot_control.shoot_motor_measure->last_ecd > HALF_ECD_RANGE)
    {
        shoot_control.ecd_count--;
    }
    else if (shoot_control.shoot_motor_measure->ecd - shoot_control.shoot_motor_measure->last_ecd < -HALF_ECD_RANGE)
    {
        shoot_control.ecd_count++;
    }

    if (shoot_control.ecd_count == FULL_COUNT)
    {
        shoot_control.ecd_count = -(FULL_COUNT - 1);
    }
    else if (shoot_control.ecd_count == -FULL_COUNT)
    {
        shoot_control.ecd_count = FULL_COUNT - 1;
    }

    // calculate the output shaft angle
    shoot_control.angle = (shoot_control.ecd_count * ECD_RANGE + shoot_control.shoot_motor_measure->ecd) * MOTOR_ECD_TO_RAD / TRIGGER_MOTOR_GEAR_RATIO;
    //microswitch
    shoot_control.key = BUTTEN_TRIG_PIN;
    //mouse clicks
    shoot_control.last_press_l = shoot_control.press_l;
    shoot_control.last_press_r = shoot_control.press_r;
    shoot_control.press_l = shoot_control.shoot_rc->mouse.press_l;
    shoot_control.press_r = shoot_control.shoot_rc->mouse.press_r;
    // Count time for long presss
    if (shoot_control.press_l)
    {
        if (shoot_control.press_l_time < PRESS_LONG_TIME)
        {
            shoot_control.press_l_time++;
        }
    }
    else
    {
        shoot_control.press_l_time = 0;
    }

    if (shoot_control.press_r)
    {
        if (shoot_control.press_r_time < PRESS_LONG_TIME)
        {
            shoot_control.press_r_time++;
        }
    }
    else
    {
        shoot_control.press_r_time = 0;
    }

    // Count time of shoot switch being down
    if (shoot_control.shoot_mode != SHOOT_STOP && switch_is_down(shoot_control.shoot_rc->rc.s[RC_LEFT_LEVER_CHANNEL]))
    {

        if (shoot_control.rc_s_time < RC_S_LONG_TIME)
        {
            shoot_control.rc_s_time++;
        }
    }
    else
    {
        shoot_control.rc_s_time = 0;
    }

    // Right mouse button pressed to accelerate the friction wheel, so that the left mouse button shoots at low speed and the right mouse button shoots at high speed
    static uint16_t up_time = 0;
    if (shoot_control.press_r)
    {
        up_time = UP_ADD_TIME;
    }

    if (up_time > 0)
    {
        shoot_control.fric1_ramp.max_value = FRIC_UP;
        shoot_control.fric2_ramp.max_value = FRIC_UP;
        up_time--;
    }
    else
    {
        // always fast speed
        shoot_control.fric1_ramp.max_value = FRIC_UP;
        shoot_control.fric2_ramp.max_value = FRIC_UP;
        // shoot_control.fric1_ramp.max_value = FRIC_DOWN;
        // shoot_control.fric2_ramp.max_value = FRIC_DOWN;
    }


}

static void trigger_motor_turn_back(void)
{
#if defined(TRIGGER_TURN)
    shoot_control.speed_set = -shoot_control.trigger_speed_set;
#else
    shoot_control.speed_set = shoot_control.trigger_speed_set;
#endif
    if( shoot_control.block_time >= BLOCK_TIME)
    {
        shoot_control.speed_set = -shoot_control.trigger_speed_set;
    }

    if(fabs(shoot_control.speed) < BLOCK_TRIGGER_SPEED && shoot_control.block_time < BLOCK_TIME)
    {
        shoot_control.block_time++;
        shoot_control.reverse_time = 0;
    }
    else if (shoot_control.block_time == BLOCK_TIME && shoot_control.reverse_time < REVERSE_TIME)
    {
        shoot_control.reverse_time++;
    }
    else
    {
        shoot_control.block_time = 0;
    }
}

/**
  * @brief          Burst fire
  * @param[in]      void
  * @retval         void
  */
static void shoot_bullet_control(void)
{
    // Rotate by TRIGGER_ANGLE_INCREMENT every time
    if (shoot_control.move_flag == 0)
    {
        shoot_control.set_angle = rad_format(shoot_control.angle + TRIGGER_ANGLE_INCREMENT);
        shoot_control.move_flag = 1;
    }
    if(shoot_control.key == SWITCH_TRIGGER_OFF)
    {
        shoot_control.shoot_mode = SHOOT_DONE;
    }
    // determine if the angle is reached
    if (rad_format(shoot_control.set_angle - shoot_control.angle) > 0.05f)
    {
        // keep setting the rotation speed until the angle is reached
        shoot_control.trigger_speed_set = TRIGGER_SPEED;
        trigger_motor_turn_back();
    }
    else
    {
        shoot_control.move_flag = 0;
    }
}

