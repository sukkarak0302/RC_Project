#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "Camera.h"
#include "Communication.h"
#include "RC_Control.h"

#include "Parameter.h"

void init_main();

void app_main(void)
{
	TaskHandle_t Com_Handle = NULL;
	TaskHandle_t Control_Handle = NULL;
	TaskHandle_t Com_Rf_Handle = NULL;
	TaskHandle_t Com_Nrf_Handle = NULL;

	init_main();
	xTaskCreate( Com_start_server, "Com_start_server", STACK_SIZE, ( void * ) 1, 1U, &Com_Handle );
	xTaskCreate( Com_nrf24_main, "Com_nrf24_main", STACK_SIZE, ( void * ) 1, 1U, &Com_Nrf_Handle );

	//xTaskCreate( Com_start_server_stream, "Com_start_server_stream", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &Com_Stream_Handle );
	//xTaskCreate( control_main, "control_main", STACK_SIZE, ( void * ) 1, 1U, &Control_Handle );
	//xTaskCreate( Com_rf433_receive, "Com_rf433_receive", STACK_SIZE, ( void * ) 1, 1U, &Com_Rf_Handle );
}

void init_main()
{
	camera_init();
	Com_init();
	control_init();
	//rf433_init();
}
