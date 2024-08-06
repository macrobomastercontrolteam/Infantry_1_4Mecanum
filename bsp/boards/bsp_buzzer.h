#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H
#include "global_inc.h"



void Buzzer_Init(void);
extern void buzzer_on(uint16_t psc, uint16_t pwm);
extern void buzzer_set(uint16_t psc,uint16_t arr,uint16_t pwm);
extern void buzzer_off(void);

#endif
