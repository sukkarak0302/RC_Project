/*
 * Communication.c
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */
#include "Communication.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "math.h"

#include "nvs_flash.h"

#include "string.h"

#include "Web_Page.h"

#include "Parameter.h"
#include "RC_Control.h"
#include "Camera.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#define ESP_WIFI_CHANNEL 1
#define ESP_MAX_STA_CONN 2

#define BUF_SIZE 1024

#define LOG "WIFI"

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* Empty handle to esp_http_server */
static httpd_handle_t server = NULL;

static QueueHandle_t uart2_queue;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(LOG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(LOG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

int Com_init()
{
	int ret_val = 1;

	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
    	ESP_ERROR_CHECK(nvs_flash_erase());
    	ret = nvs_flash_init();
    }

	esp_netif_init();
	esp_event_loop_create_default();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t wifi_config_default = WIFI_INIT_CONFIG_DEFAULT();

	esp_wifi_init(&wifi_config_default);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wf_config =
	{
		.ap =
		{
			.ssid = ESP_WIFI_SSID,
			.ssid_len = strlen(ESP_WIFI_SSID),
			.channel = ESP_WIFI_CHANNEL,
			.password = ESP_WIFI_PASS,
			.max_connection = ESP_MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};

	if( esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK && esp_wifi_set_config(WIFI_IF_AP, &wf_config) == ESP_OK )
	{
		if ( esp_wifi_start() == ESP_OK )
		{
			ret_val = 0;
		}
	}


#ifdef DEBUG
	if (ret_val == 0) ESP_LOGI(LOG, "Wifi Init successful!");
	else ESP_LOGE(LOG, "Wifi Init failed!");
#endif

	return ret_val;
}

void rf433_init()
{
	uart_config_t uart_config = {
	        .baud_rate = 2400,
	        .data_bits = UART_DATA_8_BITS,
	        .parity    = UART_PARITY_DISABLE,
	        .stop_bits = UART_STOP_BITS_1,
	        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};


    if(uart_param_config(UART_NUM_2, &uart_config))
    	ESP_LOGE(LOG,"PARAM Failed");
    uart_set_pin(UART_NUM_2, 0, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, BUF_SIZE*2, 0, 0, NULL, 0);

}

int Com_stop()
{
	int ret_val = 0;

	if(server != NULL)
	{
		if( httpd_stop(server) == ESP_OK )
		{
			if ( esp_wifi_stop() == ESP_OK )
			{
				ret_val = 1;
			}
		}
	}

#ifdef DEBUG
	ESP_LOGI(LOG, "Wifi_stop - ret_val : %d", ret_val);
#endif

	return ret_val;
}

/* RF433 related
 *
 */
void Com_rf433_receive(void* pvParameter)
{
    uart_event_t event;
    size_t* buffered_size;
    uint8_t* data = (uint8_t*) malloc(32+1);

	while(1)
	{
		const int rxBytes = uart_read_bytes(UART_NUM_2, data, 32, 1000 / portTICK_RATE_MS);
		if ( rxBytes > 0 )
		{
			data[rxBytes] = 0;
			ESP_LOGI(LOG, "Read %d bytes: '%s'", rxBytes, data);
			ESP_LOG_BUFFER_HEXDUMP(LOG, data, rxBytes, ESP_LOG_INFO);
		}
	}
	free(data);
}


int get_main(httpd_req_t *req)
{
	httpd_resp_send_chunk(req, Main_Html, strlen(Main_Html));
	httpd_resp_send_chunk(req, NULL, 0);

	return 0;
}

int get_phone(httpd_req_t *req)
{
	size_t qur_len;
	char *qur;

	char control[2];

	httpd_resp_send_chunk(req, Phone_Control_Html, strlen(Phone_Control_Html));
	httpd_resp_send_chunk(req, NULL, 0);

	qur_len = httpd_req_get_url_query_len(req) + 1;
#ifdef DEBUG
			ESP_LOGI(LOG, "QUR_len : %d", qur_len);
#endif
	if (qur_len >1)
	{
		qur = malloc(qur_len);
		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
		{
#ifdef DEBUG
			ESP_LOGI(LOG, "QUR : %s", qur);
#endif
   			if ( httpd_query_key_value(qur, "ACC", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_motor(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "ACC : %d", charToInt(control,1));
#endif
   			}

   			if ( httpd_query_key_value(qur, "STR", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_steering(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "STR : %d", charToInt(control,1));
#endif
   			}
		}
	}

	return 0;
}

int get_key(httpd_req_t *req)
{
	size_t qur_len;
	char *qur;

	char control[2];

	httpd_resp_send_chunk(req, Phone_VR_Html, strlen(Phone_VR_Html));
	httpd_resp_send_chunk(req, NULL, 0);

	qur_len = httpd_req_get_url_query_len(req) + 1;
#ifdef DEBUG
			ESP_LOGI(LOG, "QUR_len : %d", qur_len);
#endif
	if (qur_len >1)
	{
		qur = malloc(qur_len);
		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
		{
#ifdef DEBUG
			ESP_LOGI(LOG, "QUR : %s", qur);
#endif
   			if ( httpd_query_key_value(qur, "ACC", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_motor(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "ACC : %d", charToInt(control,1));
#endif
   			}

   			if ( httpd_query_key_value(qur, "STR", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_steering(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "STR : %d", charToInt(control,1));
#endif
   			}
		}
	}

	return 0;
}

int get_joy(httpd_req_t *req)
{
	httpd_resp_send_chunk(req, Phone_JoyControl_Html, strlen(Phone_JoyControl_Html));
	httpd_resp_send_chunk(req, NULL, 0);
	return 0;
}

int get_streaming(httpd_req_t *req)
{
	esp_err_t res = ESP_OK;

	size_t _jpg_buf_len = 0;
	uint8_t * _jpg_buf = NULL;
	char * part_buf[64];
#ifdef DEBUG
	ESP_LOGI(LOG, "streaming works!");
#endif
	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

	while(1)
	{
		camera_capture();
		_jpg_buf = get_frame();
		_jpg_buf_len = get_frame_length();


		if(res == ESP_OK)
		{
			size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
			res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
		}
		if(res == ESP_OK)
		{
			res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
		}
		if(res == ESP_OK)
		{
			res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		}
		camera_release();

		if(res != ESP_OK)
			break;
	}

	return res;
}

int get_joyControl(httpd_req_t *req)
{
	size_t qur_len;
	char *qur;

	char control[2];

	httpd_resp_send_chunk(req, Phone_JoyControl_Html, strlen(Phone_JoyControl_Html));
	httpd_resp_send_chunk(req, NULL, 0);

	qur_len = httpd_req_get_url_query_len(req) + 1;
#ifdef DEBUG
			ESP_LOGI(LOG, "QUR_len : %d", qur_len);
#endif
	if (qur_len >1)
	{
		qur = malloc(qur_len);
		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
		{
#ifdef DEBUG
			ESP_LOGI(LOG, "QUR : %s", qur);
#endif
   			if ( httpd_query_key_value(qur, "ACC", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_joy_motor(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "ACC : %d", charToInt(control,1));
#endif
   			}

   			if ( httpd_query_key_value(qur, "BRK", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_joy_brake(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "BRK : %d", charToInt(control,1));
#endif
   			}

   			if ( httpd_query_key_value(qur, "STR", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_joy_steering(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "STR : %d", charToInt(control,1));
#endif
   			}

   			if ( httpd_query_key_value(qur, "GER", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_joy_gear(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "GER : %d", charToInt(control,1));
#endif
   			}
		}
	}

	return 0;
}

/*
 * HTTP URI
 */
httpd_uri_t uri_main = {
		.uri	= "/main",
		.method = HTTP_GET,
		.handler = get_main,
		.user_ctx = NULL
};

httpd_uri_t uri_phone = {
		.uri	= "/phone",
		.method = HTTP_GET,
		.handler = get_phone,
		.user_ctx = NULL
};

httpd_uri_t uri_vrkey = {
		.uri	= "/vr_key",
		.method = HTTP_GET,
		.handler = get_key,
		.user_ctx = NULL
};

httpd_uri_t uri_vrjoy = {
		.uri	= "/vr_joy",
		.method = HTTP_GET,
		.handler = get_joy,
		.user_ctx = NULL
};

httpd_uri_t uri_streaming = {
		.uri	= "/streaming",
		.method = HTTP_GET,
		.handler = get_streaming,
		.user_ctx = NULL
};

httpd_uri_t uri_joyControl = {
		.uri	= "/joyControl",
		.method = HTTP_GET,
		.handler = get_joyControl,
		.user_ctx = NULL
};


/*
 * WEB Server main part
 */
int Com_server_uri()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	config.max_open_sockets = 4;
	config.max_uri_handlers = 6;

	int ret_val = 0;

	if ( httpd_start(&server, &config) == ESP_OK )
	{
		httpd_register_uri_handler(server, &uri_main);
		httpd_register_uri_handler(server, &uri_phone);
		httpd_register_uri_handler(server, &uri_vrkey);
		httpd_register_uri_handler(server, &uri_vrjoy);
		httpd_register_uri_handler(server, &uri_joyControl);
		ret_val = 1;
	}

	config.server_port += 1;
	config.ctrl_port += 1;
	if ( httpd_start(&server, &config) == ESP_OK )
	{
		httpd_register_uri_handler(server, &uri_streaming);
	}

#ifdef DEBUG
	ESP_LOGI(LOG, "start_control_server : %d", ret_val);
#endif

	return ret_val;
}

void Com_start_server_stream()
{
	Com_server_uri_stream();
#ifdef DEBUG
	ESP_LOGI(LOG, "Wifi control started!");
#endif

	for ( ; ; )
	{

	}
}

void Com_start_server()
{
	Com_server_uri();

#ifdef DEBUG
	ESP_LOGI(LOG, "Wifi control started!");
#endif

	for ( ; ; )
	{

	}
}



/*
 * Query handler helper functions
 */
int charToInt(char *buf, int limit)
{
	int ret_val = 0;
	for ( int i = 0 ; i < limit ; i++ )
	{
		ret_val = ret_val + ( buf[i] - '0' ) * pow( (double)10, ( limit - i - 1 ) );
	}
#ifdef DEBUG
	ESP_LOGI(LOG, "Retval : %d", ret_val);
#endif

	return ret_val;
}
