#ifndef CHASSIS_BEHAVIOUR_H
#define CHASSIS_BEHAVIOUR_H
#include "chassis_task.h"
#include "struct_typedef.h"

typedef enum
{
	CHASSIS_ZERO_FORCE,    // chassis will be like no power,��������, ��û�ϵ�����
	CHASSIS_NO_MOVE,       // chassis will be stop,���̱��ֲ���
	CHASSIS_NO_FOLLOW_YAW, // chassis does not follow angle, angle is open-loop,but wheels have closed-loop speed
	                       // ���̲�����Ƕȣ��Ƕ��ǿ����ģ������������ٶȻ�
	CHASSIS_SPINNING, // @TODO: Chassis spinning mode
} chassis_behaviour_e;

#define CHASSIS_OPEN_RC_SCALE 10.0f // in CHASSIS_OPEN mode, multiply the value. ��chassis_open ģ���£�ң�������Ըñ������͵�can��

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
extern void chassis_behaviour_mode_set(chassis_move_t *chassis_move_mode);

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

extern void chassis_behaviour_control_set(chassis_move_t *chassis_move_rc_to_vector);

extern chassis_behaviour_e chassis_behaviour_mode;

#endif
