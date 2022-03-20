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

#define LOG "CONTROL"

#define CHANNEL_STEERING 0
#define CHANNEL_ROTATION 1
#define CHANNEL_DRIVING1 2
#define CHANNEL_DRIVING2 3

#define GPIO_STEERING 15
#define GPIO_ROTATION 13
#define GPIO_DRIVING1 2
#define GPIO_DRIVING2 14

#define TOTAL_CHANNEL 4

#define SERVO_MIN_PULSEWIDTH_US 1000 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2000 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 180 //Maximum angle in degree upto which servo can rotate

static int value_steering = 5;
static int value_motor = 5;
static int value_rotation;

static int value_steering_pre = 5;
static int value_motor_pre = 5;
static int value_rotation_pre;

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

	mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &Motor_config);
	mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_0, &Steering_config);
	//mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &Rotation_config);

	gpio_config(&flash_config);
	gpio_set_level(4,1);



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
				set_motor(value_motor);
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
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.1);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
	}
#ifdef DEBUG
	ESP_LOGI(LOG, "Motor : %d / %f\n", val,duty_cycle);
#endif
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
