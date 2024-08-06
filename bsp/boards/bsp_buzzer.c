#include "bsp_buzzer.h"
#include "main.h"
#include "cmsis_os.h"
#include "user_lib.h"

extern TIM_HandleTypeDef htim4;



void Buzzer_Init(void)
{
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
}
void buzzer_on(uint16_t psc, uint16_t pwm)
{
    __HAL_TIM_PRESCALER(&htim4, psc);
    __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, pwm);

}
void buzzer_set(uint16_t psc,uint16_t arr,uint16_t pwm)
{
    __HAL_TIM_PRESCALER(&htim4, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim4, arr);
    __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, pwm);

}

void buzzer_off(void)
{
    __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, 0);
}

