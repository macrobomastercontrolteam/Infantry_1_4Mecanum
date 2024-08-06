#include "bsp_buzzer.h"
#include "buzzer_task.h"
#include "main.h"
#include "cmsis_os.h"
#include "user_lib.h"
#include "detect_task.h"

extern TIM_HandleTypeDef htim4;
uint16_t sound_step;
uint32_t counter;
uint32_t sound_psc;
uint32_t sound_arr;
uint32_t sound_pwm;
uint32_t test_variable;
uint32_t delay_time;
Buzzer_Note current_note;
uint64_t toe;

uint8_t playing_flag;
uint8_t battery_flag;
uint8_t toe_flag;
uint8_t song_flag;
uint8_t info_flag;


buzzer_control_t buzzer_control;

#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t buzzer_high_water;
#endif
extern Music_Data xiaomi = {
    .gap = 60,
    .psc = 65,
    .pwm = 90,
    .music_length = 33,
    .music = XIAOMI,
};
extern Music_Data flyme = {
    .gap = 40,
    .psc = 65,
    .pwm = 90,
    .music_length = 19,
    .music = FLYME,
};

extern Music_Data ddlc = {
    .gap = 150,
    .psc = 75,
    .pwm = 80,
    .music_length = 37,
    .music = DDLC,
};

extern Music_Data ied = {
    .gap = 50,
    .psc = 100,
    .pwm = 150,
    .music_length = 29,
    .music = IED,
};
extern Music_Data ckbk = {
    .gap = 50,
    .psc = 100,
    .pwm = 60,
    .music_length = 37,
    .music = CKBK,
};
extern Music_Data ydy = {
    .gap = 50,
    .psc = 100,
    .pwm = 60,
    .music_length = 19,
    .music = YDY,
};
extern Music_Data rd1 = {
    .gap = 25,
    .psc = 50,
    .pwm = 45,
    .music_length = 44,
    .music = RD1,
};
Music_Data bespridnnica1 = {  //Play song Bespridnnic in order 112344
    .gap = 60,
    .psc = 50,
    .pwm = 55,
    .music_length = 26,
    .music = BESPRIDANNICA1,
};
Music_Data bespridnnica2 = {
    .gap = 60,
    .psc = 50,
    .pwm = 55,
    .music_length = 57,
    .music = BESPRIDANNICA2,
};
Music_Data bespridnnica3 = {
    .gap = 60,
    .psc = 50,
    .pwm = 55,
    .music_length = 63,
    .music = BESPRIDANNICA3,
};
Music_Data bespridnnica4 = {
    .gap = 60,
    .psc = 50,
    .pwm = 55,
    .music_length = 32,
    .music = BESPRIDANNICA4,
};
Music_Data morse_test = {
    .gap = 150,
    .psc = 100,
    .pwm = 150,
    .music_length = 3,
    .music = MORSE_TEST,
};
Music_Data start = {
    .gap = 20,
    .psc = 100,
    .pwm = 100,
    .music_length = 3,
    .music = START,
};
Music_Data end = {
    .gap = 20,
    .psc = 100,
    .pwm = 100,
    .music_length = 3,
    .music = END,
};
Music_Data quinary_test = {
    .gap = 100,
    .psc = 100,
    .pwm = 60,
    .music_length = 4,
    .music = QUINARY_TEST,
};

void buzzer_init(void);
void buzzer_play(Music_Data music_data);
void play_num_quinary(int number);
void play_toe_status(uint32_t toe_list);
void play_Bespridnnica(void);
void buzzer_init()
{
    test_variable = 0;
    delay_time = 0;
    toe = 0;

    playing_flag = 0;
    battery_flag = 0;
    toe_flag = 0;
    song_flag = 0;
    info_flag = 0;

    buzzer_control.buzzer_mode = STOP;
    buzzer_control.buzzer_last_mode = buzzer_control.buzzer_mode;
    buzzer_control.battery_timeout = 0;
    buzzer_control.last_batteery_report_time = 0;
    buzzer_control.toe_timeout = 0;
    buzzer_control.last_toe_report_time = 0;
}


    

void buzzer_play(Music_Data music_data)
{
    if(playing_flag==0)
    {
        playing_flag=1;
        for (int i = 0; i < music_data.music_length; i++)
        {
            current_note.frequency = music_data.music[i][0];
            current_note.loudness = music_data.pwm;
            
            buzzer_set(music_data.psc,1000000/(music_data.music[i][0]),music_data.pwm);
            osDelay((uint32_t)(music_data.music[i][1]));
            buzzer_set(0,0,0);
            osDelay((uint32_t)(music_data.gap));
        }
        buzzer_off();
        playing_flag=0;
    }
}

//13 = 55111 
void play_num_quinary(int number)
{
    if(number == 0)
    {
        buzzer_set(100,1000000/(QUINARY_0),60); 
        osDelay(300);
        
        buzzer_set(0, 0, 0);
    }
    else
    {
        while (number)
        {
            if ((number - 5) >= 0)
            {
                buzzer_set(100, 1000000 / (QUINARY_5), 60);
                osDelay(150);
                number -= 5;
            }
            else if ((number - 1) >= 0)
            {
                buzzer_set(100, 1000000 / (QUINARY_1), 60);
                osDelay(100);
                number -= 1;
            }
            buzzer_set(0, 0, 0);
            osDelay(150);
        }
    }
    buzzer_off();
    osDelay(500);
}

void play_toe_status(uint32_t toe_list)
{
    if (playing_flag == 0)
    {
        playing_flag = 1;

        for (int i = 0; i < (ERROR_LIST_LENGTH); i++)
        {
            if ((toe_list >> i) & 1)
            {
                play_num_quinary(i);
            }
        }

        buzzer_off();
        osDelay(500);
        playing_flag = 0;
        //toe_flag = 0;
    }
}

// buzzer_play(bespridnnica1);
// buzzer_play(bespridnnica1);
// buzzer_play(bespridnnica2);
// buzzer_play(bespridnnica3);
// buzzer_play(bespridnnica4);
// buzzer_play(bespridnnica4);

// buzzer_play(ckbk);
// buzzer_play(flyme);
// buzzer_play(ddlc);
// buzzer_play(xiaomi);
// buzzer_play(rd1);


void buzzer_task(void const *argument)
{
    buzzer_init();
    osDelay(1000);
#if ((ROBOT_TYPE == INFANTRY_2023_MECANUM)||(ROBOT_TYPE == INFANTRY_2024_MECANUM))
    buzzer_play(ydy);
#elif (ROBOT_TYPE == INFANTRY_2023_SWERVE)
    buzzer_play(ddlc);
#endif    
    while(1)
    {
        //check
        //battery check
        if((osKernelSysTick() - buzzer_control.last_batteery_report_time > buzzer_control.battery_timeout) && (get_battery_percentage() <= 20))
        {
            battery_flag=1;
            buzzer_control.battery_timeout = BATTERY_TIMEOUT;
            buzzer_control.last_batteery_report_time = osKernelSysTick();
            
        }
        //toe check
        if(osKernelSysTick() - buzzer_control.last_toe_report_time > buzzer_control.toe_timeout)
        {
            if (toe_check() != toe)
            {
                osDelay(500);
                if (toe_check() != toe)//double check
                {
                    toe_flag = 1;
                    toe = toe_check();
                }
            }
            buzzer_control.toe_timeout = TOE_TIMEOUT;
            buzzer_control.last_toe_report_time = osKernelSysTick();
        }

        //play
        if(toe_flag)
        {
            buzzer_play(quinary_test);//play sound for 0,1,5 in sequence
            osDelay(1000);
            play_toe_status(toe);
            buzzer_play(end);
            toe_flag = 0;
                        
        }
        else if((battery_flag))
        {
            buzzer_play(ied);
            battery_flag = 0;
        }
#if INCLUDE_uxTaskGetStackHighWaterMark
	buzzer_high_water = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}

