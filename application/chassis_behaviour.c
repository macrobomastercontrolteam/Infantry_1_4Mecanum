#include "arm_math.h"
#include "chassis_behaviour.h"
#include "chassis_task.h"
#include "cmsis_os.h"
#include "detect_task.h"

#include "biped.h"
#include "cv_usart_task.h"
#include "gimbal_behaviour.h"

#define RPM_TO_RADS(_ROUND_PER_MIN) (_ROUND_PER_MIN * 0.10471975511965977f)
#define SPINNING_CHASSIS_LOW_OMEGA (RPM_TO_RADS(25.0f))
#define SPINNING_CHASSIS_MED_OMEGA (RPM_TO_RADS(30.0f))
#define SPINNING_CHASSIS_HIGH_OMEGA (RPM_TO_RADS(35.0f))

#if CHASSIS_JSCOPE_DEBUG
int32_t chassis_behaviour_mode_int;
static void jscope_chassis_behavior_test(void)
{
	chassis_behaviour_mode_int = (int32_t)(chassis_behaviour_mode);
}
#endif

/**
 * @brief          when chassis behaviour mode is CHASSIS_ZERO_FORCE, the function is called
 *                 and chassis control mode is raw. The raw chassis control mode means set value
 *                 will be sent to CAN bus derectly, and the function will set all speed zero.
 * @param[out]     vx_can_set: vx speed value, it will be sent to CAN bus derectly.
 * @param[out]     vy_can_set: vy speed value, it will be sent to CAN bus derectly.
 * @param[out]     wz_can_set: wz rotate speed value, it will be sent to CAN bus derectly.
 * @param[in]      chassis_move_rc_to_vector: chassis data
 * @retval         none
 */
/**
 * @brief          ������������Ϊ״̬���£�����ģʽ��raw���ʶ��趨ֵ��ֱ�ӷ��͵�can�����Ϲʶ����趨ֵ������Ϊ0
 * @author         RM
 * @param[in]      vx_setǰ�����ٶ� �趨ֵ��ֱ�ӷ��͵�can������
 * @param[in]      vy_set���ҵ��ٶ� �趨ֵ��ֱ�ӷ��͵�can������
 * @param[in]      wz_set��ת���ٶ� �趨ֵ��ֱ�ӷ��͵�can������
 * @param[in]      chassis_move_rc_to_vector��������
 * @retval         ���ؿ�
 */
static void chassis_zero_force_control(chassis_move_t *chassis_move_rc_to_vector);

/**
 * @brief          when chassis behaviour mode is CHASSIS_NO_FOLLOW_YAW, chassis control mode is speed control mode.
 *                 chassis will no follow angle, chassis rotation speed is set by wz_set.
 * @param[out]     vx_set: vx speed value, positive value means forward speed, negative value means backward speed,
 * @param[out]     vy_set: vy speed value, positive value means left speed, negative value means right speed.
 * @param[out]     wz_set: rotation speed,positive value means counterclockwise , negative value means clockwise
 * @param[in]      chassis_move_rc_to_vector: chassis data
 * @retval         none
 */
/**
 * @brief          ���̲�����Ƕȵ���Ϊ״̬���£�����ģʽ�ǲ�����Ƕȣ�������ת�ٶ��ɲ���ֱ���趨
 * @author         RM
 * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
 * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
 * @param[in]      wz_set�������õ���ת�ٶ�,��ֵ ��ʱ����ת����ֵ ˳ʱ����ת
 * @param[in]      chassis_move_rc_to_vector��������
 * @retval         ���ؿ�
 */
static void chassis_no_follow_yaw_control(chassis_move_t *chassis_move_rc_to_vector);

static void chassis_cv_no_follow_yaw_control(chassis_move_t *chassis_move_rc_to_vector);

// highlight, the variable chassis behaviour mode
// ���⣬���������Ϊģʽ����
chassis_behaviour_e chassis_behaviour_mode = CHASSIS_ZERO_FORCE;

/**
 * @brief          logical judgement to assign "chassis_behaviour_mode" variable to which mode
 * @param[in]      chassis_move_mode: chassis data
 * @retval         none
 */
/**
 * @brief          ͨ���߼��жϣ���ֵ"chassis_behaviour_mode"������ģʽ
 * @param[in]      chassis_move_mode: ��������
 * @retval         none
 */
void chassis_behaviour_mode_set(chassis_move_t *chassis_move_mode)
{
	if (chassis_move_mode == NULL)
	{
		return;
	}

	static uint8_t last_right_rc_switch = RC_SW_DOWN;
	static uint8_t last_left_rc_switch = RC_SW_DOWN;
	uint8_t right_rc_switch = chassis_move_mode->chassis_RC->rc.s[RIGHT_LEVER_CHANNEL];
	uint8_t left_rc_switch = chassis_move_mode->chassis_RC->rc.s[LEFT_LEVER_CHANNEL];
	switch (right_rc_switch)
	{
		case RC_SW_UP:
		{
			if (last_right_rc_switch != right_rc_switch)
			{
				biped_jumpStart();
			}
			break;
		}
		case RC_SW_MID:
		{
			if ((last_right_rc_switch != right_rc_switch) || (last_left_rc_switch != left_rc_switch))
			{
				if ((left_rc_switch == RC_SW_DOWN) && (toe_is_error(CV_TOE) == 0))
				{					
					chassis_behaviour_mode = CHASSIS_CV_NO_FOLLOW_YAW;
				}
				else
				{
					chassis_behaviour_mode = CHASSIS_NO_FOLLOW_YAW;
				}
			}
			break;
		}
		case RC_SW_DOWN:
		default:
		{
			chassis_behaviour_mode = CHASSIS_ZERO_FORCE;
			break;
		}
	}
	last_right_rc_switch = right_rc_switch;
	last_left_rc_switch = left_rc_switch;

	// //when gimbal in some mode, such as init mode, chassis must's move
	// //����̨��ĳЩģʽ�£����ʼ���� ���̲���
	// if (gimbal_cmd_to_chassis_stop())
	// {
	//     chassis_behaviour_mode = CHASSIS_NO_MOVE;
	// }

	// accord to beheviour mode, choose chassis control mode
	// ������Ϊģʽѡ��һ�����̿���ģʽ
	switch (chassis_behaviour_mode)
	{
		case CHASSIS_NO_MOVE:
		case CHASSIS_NO_FOLLOW_YAW:
		{
			chassis_move_mode->chassis_mode = CHASSIS_VECTOR_NO_FOLLOW_YAW;
			break;
		}
		case CHASSIS_CV_NO_FOLLOW_YAW:
		{
			chassis_move_mode->chassis_mode = CHASSIS_VECTOR_CV_NO_FOLLOW_YAW;
			break;
		}
		case CHASSIS_SPINNING:
		{
			chassis_move_mode->chassis_mode = CHASSIS_VECTOR_SPINNING;
			break;
		}
		case CHASSIS_ZERO_FORCE:
		default:
		{
			chassis_move_mode->chassis_mode = CHASSIS_VECTOR_RAW;
			break;
		}
	}

#if CHASSIS_JSCOPE_DEBUG
	jscope_chassis_behavior_test();
#endif
}

/**
 * @brief          set control set-point. three movement param, according to difference control mode,
 *                 will control corresponding movement.in the function, usually call different control function.
 * @param[out]     vx_set, usually controls vertical speed.
 * @param[out]     vy_set, usually controls horizotal speed.
 * @param[out]     wz_set, usually controls rotation speed.
 * @param[in]      chassis_move_rc_to_vector,  has all data of chassis
 * @retval         none
 */
/**
 * @brief          ���ÿ�����.���ݲ�ͬ���̿���ģʽ��������������Ʋ�ͬ�˶�.������������棬����ò�ͬ�Ŀ��ƺ���.
 * @param[out]     vx_set, ͨ�����������ƶ�.
 * @param[out]     vy_set, ͨ�����ƺ����ƶ�.
 * @param[out]     wz_set, ͨ��������ת�˶�.
 * @param[in]      chassis_move_rc_to_vector,  ��������������Ϣ.
 * @retval         none
 */

void chassis_behaviour_control_set(chassis_move_t *chassis_move_rc_to_vector)
{

	if (chassis_move_rc_to_vector == NULL)
	{
		return;
	}

	switch (chassis_behaviour_mode)
	{
		case CHASSIS_ZERO_FORCE:
		{
			chassis_zero_force_control(chassis_move_rc_to_vector);
			break;
		}
		case CHASSIS_NO_FOLLOW_YAW:
		{
			chassis_no_follow_yaw_control(chassis_move_rc_to_vector);
			break;
		}
		case CHASSIS_CV_NO_FOLLOW_YAW:
		{
			chassis_cv_no_follow_yaw_control(chassis_move_rc_to_vector);
			break;
		}
		default:
		{
			break;
		}
	}
}

/**
 * @brief          when chassis behaviour mode is CHASSIS_ZERO_FORCE, the function is called
 *                 and chassis control mode is raw. The raw chassis chontrol mode means set value
 *                 will be sent to CAN bus derectly, and the function will set all speed zero.
 * @param[out]     vx_can_set: vx speed value, it will be sent to CAN bus derectly.
 * @param[out]     vy_can_set: vy speed value, it will be sent to CAN bus derectly.
 * @param[out]     wz_can_set: wz rotate speed value, it will be sent to CAN bus derectly.
 * @param[in]      chassis_move_rc_to_vector: chassis data
 * @retval         none
 */
/**
 * @brief          ������������Ϊ״̬���£�����ģʽ��raw���ʶ��趨ֵ��ֱ�ӷ��͵�can�����Ϲʶ����趨ֵ������Ϊ0
 * @author         RM
 * @param[in]      vx_setǰ�����ٶ� �趨ֵ��ֱ�ӷ��͵�can������
 * @param[in]      vy_set���ҵ��ٶ� �趨ֵ��ֱ�ӷ��͵�can������
 * @param[in]      wz_set��ת���ٶ� �趨ֵ��ֱ�ӷ��͵�can������
 * @param[in]      chassis_move_rc_to_vector��������
 * @retval         ���ؿ�
 */

static void chassis_zero_force_control(chassis_move_t *chassis_move_rc_to_vector)
{
	if (chassis_move_rc_to_vector == NULL)
	{
		return;
	}

	biped.roll.set = 0;
	// biped.pitch.set = 0;
	biped.yaw.set = 0;

	// biped.velocity.set = 0;

	biped_set_dis(biped.leg_simplified.dis.now, (biped.pitch.now < 0));

	biped.leg_L.L0.set = LEG_L0_MID;
	biped.leg_R.L0.set = LEG_L0_MID;
}

static void chassis_no_follow_yaw_control(chassis_move_t *chassis_move_rc_to_vector)
{
	if (chassis_move_rc_to_vector == NULL)
	{
		return;
	}
	fp32 distanceDelta = 0;
	chassis_rc_to_control_vector(chassis_move_rc_to_vector, &distanceDelta);

	biped_brakeManager(distanceDelta);
	biped_jumpManager();

	biped.leg_L.L0.set = fp32_constrain(biped.leg_L.L0.set, LEG_L0_MIN, LEG_L0_MAX);
	biped.leg_R.L0.set = fp32_constrain(biped.leg_R.L0.set, LEG_L0_MIN, LEG_L0_MAX);
	biped.roll.set = fp32_constrain(biped.roll.set, -MAX_CHASSIS_ROLL, MAX_CHASSIS_ROLL);
	biped.yaw.set = fp32_constrain(biped.yaw.set, -PI, PI);
}

static void chassis_cv_no_follow_yaw_control(chassis_move_t *chassis_move_rc_to_vector)
{
	if (chassis_move_rc_to_vector == NULL)
	{
		return;
	}
	fp32 distanceDelta = 0;
	chassis_cv_to_control_vector(chassis_move_rc_to_vector, &distanceDelta);
	chassis_rc_to_control_vector(chassis_move_rc_to_vector, &distanceDelta);

	biped_brakeManager(distanceDelta);
	biped_jumpManager();

	biped.leg_L.L0.set = fp32_constrain(biped.leg_L.L0.set, LEG_L0_MIN, LEG_L0_MAX);
	biped.leg_R.L0.set = fp32_constrain(biped.leg_R.L0.set, LEG_L0_MIN, LEG_L0_MAX);
	biped.roll.set = fp32_constrain(biped.roll.set, -MAX_CHASSIS_ROLL, MAX_CHASSIS_ROLL);
	biped.yaw.set = fp32_constrain(biped.yaw.set, -PI, PI);
}
