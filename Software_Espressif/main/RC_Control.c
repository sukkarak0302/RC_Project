/*
 * Control.c
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */

#include "RC_Control.h"
#include "Parameter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "driver/mcpwm.h"

#include "esp_log.h"

#include "esp_system.h"

#include "driver/gpio.h"

#include "driver/ledc.h"

#define LOG "CONTROL"

#define CHANNEL_STEERING 0
#define CHANNEL_ROTATION 1
#define CHANNEL_DRIVING1 2
#define CHANNEL_DRIVING2 3

#define GPIO_STEERING 4//13
#define GPIO_ROTATION 4//15
#define GPIO_DRIVING1 4//2
#define GPIO_DRIVING2 4//14
#define GPIO_FLASH 4

#define TOTAL_CHANNEL 4

#define SERVO_MIN_PULSEWIDTH_US 1000 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2000 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 180 //Maximum angle in degree upto which servo can rotate

#define MAX_DRV 4

#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (GPIO_ROTATION) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_3
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (76) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (50) // Frequency in Hertz. Set frequency at 5 kHz

static int value_steering = 5;
static int value_motor = 5;
static int value_rotation;
static int value_brake = 0;
static int value_forward = 1;
static float value_motor_f = 5.0;

static int value_steering_pre = 5;
static int value_motor_pre = 5;
static int value_brake_pre = 0;
static int value_rotation_pre;
static int value_flash = 0;
static int value_rotation_enable = 0;

int control_init(void)
{

	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_DRIVING1);
	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_DRIVING2);

	mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0A, GPIO_STEERING);
	//mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM2A, GPIO_ROTATION);

	mcpwm_config_t Motor_config =
	{
		.frequency 		= 1000,
		.cmpr_a			= 0,
		.cmpr_b			= 0,
		.counter_mode	= MCPWM_UP_COUNTER,
		.duty_mode		= MCPWM_DUTY_MODE_0
	};

	mcpwm_config_t Steering_config =
	{
		.frequency 		= 50,
		.cmpr_a			= 0,
		.counter_mode	= MCPWM_UP_COUNTER,
		.duty_mode		= MCPWM_DUTY_MODE_0
	};

	mcpwm_config_t Rotation_config =
	{
		.frequency 		= 50,
		.cmpr_a			= 0,
		.cmpr_b			= 0,
		.counter_mode	= MCPWM_UP_COUNTER,
		.duty_mode		= MCPWM_DUTY_MODE_0
	};

	gpio_config_t flash_config =
	{
			.intr_type = GPIO_INTR_DISABLE,
			.mode = GPIO_MODE_OUTPUT,
			.pin_bit_mask = (GPIO_SEL_4),
			.pull_down_en = 0,
			.pull_up_en = 0
	};

	//
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 76, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

	mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &Motor_config);
	mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_0, &Steering_config);
	//mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &Rotation_config);

	gpio_config(&flash_config);
	gpio_set_level(GPIO_FLASH,value_flash);

	return 1;
}

void control_main()
{
	uint32_t count = 0;
	while(1)
	{
		if ( (value_steering_pre != value_steering) || (value_motor_pre != value_motor) || (value_rotation_pre != value_rotation) || (count >= 50))
		{
			if(value_steering_pre != value_steering)
				set_steering(value_steering);
			if(value_motor_pre != value_motor)
				set_motor(control_brake(value_motor));
			if(value_rotation_pre != value_rotation)
				set_rotation(value_rotation);
			count = 0;
		}
		else
		{
			count = count + 1;
			vTaskDelay(10);
		}
	}
}

void set_steering(int val)
{
	value_steering_pre = val;
	uint32_t active_time = -(val-5)*(SERVO_MIN_PULSEWIDTH_US-SERVO_MAX_PULSEWIDTH_US)/4 + ((SERVO_MIN_PULSEWIDTH_US+SERVO_MAX_PULSEWIDTH_US)/2);
	mcpwm_set_duty_in_us(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_OPR_A, active_time);
#ifdef DEBUG
	ESP_LOGI(LOG, "Str : %d / dut : %d \n", val, active_time);
#endif
	if(value_rotation_enable == 1){
		uint32_t active_pwm = (uint32_t)(((float)(9-val)/10.0 + 1.0)*1024.0/20.0);
		ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, active_pwm));
		ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
	}

}

void set_motor(int val)
{
	value_motor_pre = val;
	float duty_cycle = (float)((val-5) / 4.0)*100;
	if (val > 5)
	{
		mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty_cycle);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
	}
	else if (val < 5)
	{
		mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, (-1*duty_cycle));
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
	}
	else
	{
		// min duty cycle for steering
		mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
		mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.1);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
	}
#ifdef DEBUG
	ESP_LOGI(LOG, "Motor : %d / %f\n", val,duty_cycle);
#endif
}


int control_brake(int val_motor)
{
	if ( value_brake == 1 )
	{
		value_motor_f = 0;
		if ( value_brake_pre != value_brake )
		{
			if ( value_motor > 7 )
			{
				value_motor = 6;
				return 1;
			}
			else if ( value_motor > 5 )
			{
				value_motor = 5;
				return 3;
			}
			else if ( value_motor < 3 )
			{
				value_motor = 4;
				return 9;
			}
			else if ( value_motor < 5 )
			{
				value_motor = 5;
				return 7;
			}
		}
		value_motor = 5;
		return 5;
	}
	else
	{
		return val_motor;
	}
}

void set_rotation(int val)
{
	// Not yet implemented
}

//Helper function
static uint32_t servo_per_degree_init(uint32_t angle)
{
    return (angle + SERVO_MAX_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (2 * SERVO_MAX_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

void set_value_steering(int val)
{
	value_steering = val;
}

void set_value_motor(int val)
{
	value_motor = val;
}

void set_value_rotation(int val)
{
	value_rotation = val;
}

void set_value_flash(int val)
{
	if(value_flash != val)
		gpio_set_level(GPIO_FLASH, value_flash);
	value_flash = val;

}

void set_value_rotation_enable(int val)
{
	value_rotation_enable = val;
}

//Joy stick
void set_value_joy_motor(int val)
{
	// If no acceleration then reduce speed
	if( val == 0 )
	{
		value_motor_f = value_motor_f - 0.3;
	}
	else
	{
		if( value_motor_f + (float)(val/2) >= MAX_DRV )
			value_motor_f = MAX_DRV;
		else
			value_motor_f = value_motor_f + (float)(val/2);
	}

	if ( value_motor_f <= 0.0 )
		value_motor_f = 0.0;

	if ( value_forward == 2 )
	{
		value_motor = -1*(int)(value_motor_f) + 5;
	}
	else if ( value_forward == 1 )
	{
		value_motor = (int)(value_motor_f) + 5;
	}
	else
	{
		value_motor = 5;
	}

#ifdef DEBUG
	ESP_LOGI(LOG,"value_motor : %f / %d\n", value_motor_f, value_motor);
#endif
}

void set_value_joy_brake(int val)
{
	value_brake_pre = value_brake;
	value_brake = val;
}

void set_value_joy_steering(int val)
{
	// temporary implementation
	value_steering = val;
}

void set_value_joy_gear(int val)
{
	// forward = 1 --> forward / forward = -1 --> backward
	value_forward = val;
}
