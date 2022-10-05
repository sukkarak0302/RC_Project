/*
 * Camera.c
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */
//#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_camera.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"

static const char *TAG = "CAMERA";

#define HEADER "HDR"

//PIN MAP for ESP32-CAM
#define CAM_PIN_PWDN    32 // power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define MAX_FILE_NAME_LENGTH 50

static camera_fb_t * fb;

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 16000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA, //QQVGA-QXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 5, //0-63 lower number means higher quality
    .fb_count = 1, //if more than one, i2s runs in continuous mode. Use only with JPEG
	.fb_location = CAMERA_FB_IN_DRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY //. Sets when buffers should be filled
};



esp_err_t camera_init()
{
    int ret_val = 0;

    if (esp_camera_init(&camera_config) != ESP_OK)
    	ret_val++;

#ifdef DEBUG
    if (ret_val == 0) ESP_LOGI(TAG, "camera init successful");
    ESP_LOGE(TAG, "camera init - ret_val : %d", ret_val);
#endif

    return ret_val;
}

void camera_main()
{
	while(1)
	{

	}
}

void camera_capture()
{
	esp_err_t res = ESP_OK;
	size_t _jpg_buf_len = 0;
	uint8_t * _jpg_buf = NULL;

	sensor_t * s = esp_camera_sensor_get();
	s->set_vflip(s, 1);
	s->set_hmirror(s, 1);

    fb = esp_camera_fb_get();
    if (!fb)
    {
        res = ESP_FAIL;
    }
    else
    {
     	if(fb->format != PIXFORMAT_JPEG)
        {
     		bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if(!jpeg_converted)
            {
                res = ESP_FAIL;
            }
        }
        else
        {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }
    }
#ifdef DEBUG
    ESP_LOGE(TAG, "camera capture : %d", res);
#endif
}

void camera_release()
{
    if(fb){
        esp_camera_fb_return(fb);
        fb = NULL;
    }
}


uint8_t * get_frame()
{
	camera_capture();
	return fb->buf;
}

size_t get_frame_length()
{
	return fb->len;
}
