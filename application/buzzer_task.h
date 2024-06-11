#ifndef _BUZZER_TASK_H
#define _BUZZER_TASK_H

#include "AHRS_middleware.h"
#include "chassis_behaviour.h"
#include "chassis_power_control.h"
#include "cmsis_os.h"
#include "gimbal_behaviour.h"
#include "gimbal_task.h"
#include "global_inc.h"
#include "referee.h"
#include "shoot.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define Note_C4 (262) // 261.63Hz, 3822us
#define Note_D4 (294) // 293.66Hz, 3405us
#define Note_E4 (330) // 329.63Hz, 3034us
#define Note_F4 (349) // 349.23Hz, 2863us
#define Note_G4 (392) // 392.00Hz, 2551us
#define Note_A4 (440) // 440.00Hz, 2272us
#define Note_B4 (494) // 493.88Hz, 2052us

#define Note_C5 (523) // 523.25Hz, 1911us
#define Note_D5 (587) // 587.33Hz, 1703us
#define Note_E5 (659) // 659.26Hz, 1517us
#define Note_F5 (698) // 698.46Hz, 1432us
#define Note_G5 (784) // 784.00Hz, 1276us
#define Note_A5 (880) // 880.00Hz, 1136us
#define Note_B5 (988) // 987.77Hz, 1012us

#define Note_C6 (1047) // 1046.50Hz, 956us
#define Note_D6 (1175) // 1174.66Hz, 851us
#define Note_E6 (1319) // 1318.51Hz, 758us
#define Note_F6 (1397) // 1396.91Hz, 716us
#define Note_G6 (1568) // 1567.98Hz, 638us
#define Note_A6 (1760) // 1760.00Hz, 568us
#define Note_B6 (1976) // 1975.53Hz, 506us

#define Note_C7 (2093) // 2093.00Hz, 478us
#define Note_D7 (2349) // 2349.32Hz, 426us
#define Note_E7 (2637) // 2637.02Hz, 379us
#define Note_F7 (2794) // 2793.83Hz, 358us
#define Note_G7 (3136) // 3135.96Hz, 319us
#define Note_A7 (3520) // 3520.00Hz, 284us
#define Note_B7 (3951) // 3951.07Hz, 253us

#define Note_b4 466
#define Note_00 (0)



#define EIGHTH_NOTE_DURATION (125)
#define FOURTH_NOTE_DURATION (250)
#define HALF_NOTE_DURATION (500)
#define WHOLE_NOTE_DURATION (1000)

#define MORSE_LONG 1865
#define MORSE_SHORT 932
#define MORSE_GAP 2637

#define QUINARY_0 Note_A5
#define QUINARY_1 Note_B6
#define QUINARY_5 Note_C6

#define QUINARY_TEST {\
    {Note_A5,300},\
    {Note_B6,100},\
    {Note_C6,150},\
    {Note_00,WHOLE_NOTE_DURATION}}

#define START {\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_C6,FOURTH_NOTE_DURATION},\
    {Note_00,WHOLE_NOTE_DURATION}}

#define END {\
    {Note_00,WHOLE_NOTE_DURATION},\
    {Note_C6,EIGHTH_NOTE_DURATION},\
    {Note_C5,FOURTH_NOTE_DURATION}}

#define MORSE_TEST {\
    {MORSE_SHORT,HALF_NOTE_DURATION},\
    {MORSE_LONG,WHOLE_NOTE_DURATION},\
    {Note_00,WHOLE_NOTE_DURATION}}

#define XIAOMI {\
    {Note_E5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_C6,FOURTH_NOTE_DURATION},\
    {Note_E5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_C6,FOURTH_NOTE_DURATION},\
    {Note_E5,FOURTH_NOTE_DURATION},\
    {Note_F5,FOURTH_NOTE_DURATION},\
    {Note_D5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_D6,FOURTH_NOTE_DURATION},\
    {Note_D5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_D6,FOURTH_NOTE_DURATION},\
    {Note_D5,FOURTH_NOTE_DURATION},\
    {Note_E5,FOURTH_NOTE_DURATION},\
    {Note_C5,FOURTH_NOTE_DURATION},\
    {Note_F5,FOURTH_NOTE_DURATION},\
    {Note_C6,FOURTH_NOTE_DURATION},\
    {Note_C5,FOURTH_NOTE_DURATION},\
    {Note_F6,FOURTH_NOTE_DURATION},\
    {Note_C5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_D5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_D6,FOURTH_NOTE_DURATION},\
    {Note_D5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION},\
    {Note_D6,FOURTH_NOTE_DURATION},\
    {Note_D5,FOURTH_NOTE_DURATION},\
    {Note_G5,FOURTH_NOTE_DURATION}}
 
#define DDLC {\
    {Note_00,WHOLE_NOTE_DURATION},\
    {Note_00,WHOLE_NOTE_DURATION},\
    {Note_00,WHOLE_NOTE_DURATION},\
    {Note_00,WHOLE_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_E5,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_00,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_F5,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_00,EIGHTH_NOTE_DURATION},\
    {Note_D5,EIGHTH_NOTE_DURATION},\
    {Note_G5,115},\
    {Note_D6,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_A4,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_F5,EIGHTH_NOTE_DURATION},\
    {Note_00,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_E5,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_00,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_F5,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_00,EIGHTH_NOTE_DURATION},\
    {Note_D5,EIGHTH_NOTE_DURATION},\
    {Note_G5,115},\
    {Note_D6,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_A4,EIGHTH_NOTE_DURATION},\
    {Note_C5,EIGHTH_NOTE_DURATION},\
    {Note_F5,EIGHTH_NOTE_DURATION}}



#define MARIO_THEME {\
    {Note_E6, EIGHTH_NOTE_DURATION},\
    {Note_E6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_E6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_C6, EIGHTH_NOTE_DURATION},\
    {Note_E6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_G6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_G5, EIGHTH_NOTE_DURATION},\
    {Note_00, FOURTH_NOTE_DURATION},\
    {Note_C6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_G5, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_E5, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_A5, EIGHTH_NOTE_DURATION},\
    {Note_00, FOURTH_NOTE_DURATION},\
    {Note_B5, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_A5, EIGHTH_NOTE_DURATION},\
    {Note_A5, EIGHTH_NOTE_DURATION},\
    {Note_00, FOURTH_NOTE_DURATION},\
    {Note_G5, EIGHTH_NOTE_DURATION},\
    {Note_E6, EIGHTH_NOTE_DURATION},\
    {Note_G6, EIGHTH_NOTE_DURATION},\
    {Note_A6, EIGHTH_NOTE_DURATION},\
    {Note_00, FOURTH_NOTE_DURATION},\
    {Note_F6, EIGHTH_NOTE_DURATION},\
    {Note_G6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_E6, EIGHTH_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_C6, EIGHTH_NOTE_DURATION},\
    {Note_D6, EIGHTH_NOTE_DURATION},\
    {Note_B5, EIGHTH_NOTE_DURATION},\
    {Note_00, FOURTH_NOTE_DURATION}}

#define IED {\
    {Note_C4, EIGHTH_NOTE_DURATION},\
    {Note_C4, EIGHTH_NOTE_DURATION},\
    {Note_C4, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_G4, HALF_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {466, HALF_NOTE_DURATION},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_F5, EIGHTH_NOTE_DURATION},\
    {622, EIGHTH_NOTE_DURATION},\
    {Note_D5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {415, EIGHTH_NOTE_DURATION},\
    {415, EIGHTH_NOTE_DURATION},\
    {415, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {Note_F5, EIGHTH_NOTE_DURATION},\
    {622, EIGHTH_NOTE_DURATION},\
    {Note_D5, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION}}

#define CKBK {\
    {Note_G4, EIGHTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {Note_G4, EIGHTH_NOTE_DURATION},\
    {Note_D4, 350},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_A4, FOURTH_NOTE_DURATION},\
    {Note_G4, EIGHTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {Note_G4, EIGHTH_NOTE_DURATION},\
    {Note_D4, 350},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {466, FOURTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {466, EIGHTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {311, 350},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {466, FOURTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {466, EIGHTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {311, 350},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_C5, FOURTH_NOTE_DURATION},\
    {466, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {466, EIGHTH_NOTE_DURATION},\
    {311, 350},\
    {Note_00, EIGHTH_NOTE_DURATION},\
    {Note_C5, FOURTH_NOTE_DURATION},\
    {466, EIGHTH_NOTE_DURATION},\
    {Note_C5, EIGHTH_NOTE_DURATION},\
    {466, EIGHTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {Note_G4, EIGHTH_NOTE_DURATION},\
    {Note_A4, EIGHTH_NOTE_DURATION},\
    {370, 450},\
    {Note_00, EIGHTH_NOTE_DURATION}}
        
typedef struct Buzzer_Note {
    uint16_t frequency;
    uint16_t period_us;
    uint16_t loudness;
} Buzzer_Note;

typedef struct Music_Data{
    uint16_t bpm;
    uint16_t psc;
    uint16_t pwm;
    uint16_t gap;
    uint16_t music_length;
    uint16_t music[70][2];
}Music_Data;


extern void buzzer_task(void const *argument);
extern const Buzzer_Note buzzer_notes[] ;
extern const uint16_t frequency[];
extern void buzzer_play(Music_Data music);
void play_toe_status(uint32_t toe_list);
uint32_t toe_check(void);
void play_num_quinary(int number);
void playMorseCode(int number);
void playMorseCodeDigit(char digit);
#endif /* _BUZZER_TASK_H */

