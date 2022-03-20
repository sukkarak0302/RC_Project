/*
 * Camera.h
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_CAMERA_H_
#define MAIN_CAMERA_H_

#include "esp_attr.h"

esp_err_t camera_init();
void camera_main();
void camera_capture();
void camera_release();

uint8_t * get_frame();
size_t get_frame_length();



#endif /* MAIN_CAMERA_H_ */
