/*
 * Control.h
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_RC_CONTROL_H_
#define MAIN_RC_CONTROL_H_

#include "esp_attr.h"


int control_init();
void control_main();
void set_steering(int val);
void set_motor(int val);
void set_rotation(int val);

static uint32_t servo_per_degree_init(uint32_t degree_of_rotation);

void set_value_steering(int val);
void set_value_motor(int val);
void set_value_rotation(int val);

int control_brake(int val_motor);

//Joy stick
void set_value_joy_motor(int val);
void set_value_joy_brake(int val);
void set_value_joy_steering(int val);
void set_value_joy_gear(int val);

#endif /* MAIN_RC_CONTROL_H_ */
