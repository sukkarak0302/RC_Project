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

uint8_t address[] = {0x00, 0x00, 0x00, 0x00, 0x01};

/* Empty handle to esp_http_server */
static httpd_handle_t server = NULL;

static QueueHandle_t uart2_queue;

static spi_device_handle_t spi;

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

	Com_nrf24_init();


#ifdef DEBUG
	if (ret_val == 0) ESP_LOGI(LOG, "Wifi Init successful!");
	else ESP_LOGE(LOG, "Wifi Init failed!");
#endif

	return ret_val;
}

void Com_nrf24_init()
{
	esp_err_t ret;
	spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 32
    };

	//Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=8,
        //.address_bits=0,
        //.dummy_bits=0,
        .clock_speed_hz=10*1000*1000,
        .mode=0,
        .spics_io_num=GPIO_CS,
		.flags = SPI_DEVICE_HALFDUPLEX,
		.pre_cb=NULL,  //Specify pre-transfer callback to handle D/C line
        //.cs_ena_posttrans=3,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=4
    };

	ret=spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);


/*
    Com_nrf24_writeReg(3,0x03, 1); // EN_RXADDR, choosing 5 bit tx/rx address
    Com_nrf24_writeReg(6,0x0E, 1); // SETUP_AW
    Com_nrf24_writeReg(5,0x00, 1); // RF_CH
    Com_nrf24_writeReg(6,0x0E, 1); // RF_SETUP power = 0dB
*/
    uint8_t dat = 0;
    Com_nrf24_writeReg(CONFIG, &dat, 1);  // will be configured later
    Com_nrf24_writeReg(EN_AA, &dat, 1);  // No Auto ACK
    Com_nrf24_writeReg(EN_RXADDR, &dat, 1);  // Not Enabling any data pipe right now
    Com_nrf24_writeReg(SETUP_RETR, &dat, 1);   // No retransmission
    Com_nrf24_writeReg(RF_CH, &dat, 1);  // will be setup during Tx or RX

    dat = 0x03;
    Com_nrf24_writeReg(SETUP_AW, &dat, 1);  // 5 Bytes for the TX/RX address
    dat= 0x0E;
	Com_nrf24_writeReg(RF_SETUP, &dat, 1);   // Power= 0db, data rate = 2Mbps

    /*
    uint8_t cmd = 0x20;
	Com_nrf24_cmd(spi, cmd, NULL, true); // W_REGISTER
	ESP_LOGI(LOG, "W_Register sent");
	cmd = 0x0B;
	Com_nrf24_cmd(spi, 0b00001011, NULL, true); // initializing
	ESP_LOGI(LOG, "RF Initialized");
    */

#ifdef DEBUG
		ESP_LOGI(LOG, "spi cofig done\n");
#endif
}

void Com_nrf24_writeReg(uint8_t reg, uint8_t* data, uint8_t size)
{
	Com_nrf24_cmd(spi, (reg|1<<5), data, size, 1);
}

uint8_t Com_nrf24_readReg(uint8_t reg)
{
	uint8_t data=0;
	uint8_t cmd=0;
	cmd = cmd | reg;
	Com_nrf24_rx_cmd(spi, cmd, &data, 1, 1);

	return data;
}

void Com_nrf24_RxMode()
{
	uint8_t dat = 10;
	Com_nrf24_writeReg(RF_CH, &dat, 1); // selecting the channel
	dat = 0x02;
	Com_nrf24_writeReg(EN_RXADDR, &dat, 1); // select data pipe1

	Com_nrf24_writeReg(RX_ADDR_P1, address, 5); // Writing the pipe1 address
	dat = 0xEE;
	Com_nrf24_writeReg(RX_ADDR_P2, &dat, 1); // 32 bit payload size for pipe1
	dat = 32;
	Com_nrf24_writeReg(RX_PW_P2, &dat, 1); // 32 bit payload size for pipe1

	uint8_t config = Com_nrf24_readReg(RF_CH);
	ESP_LOGI(LOG, "read reg test : %d \n", config);
	config = config | (1<<1) | (1<<0);
	Com_nrf24_writeReg(CONFIG, &config, 1);


}

void Com_nrf24_readBuffer(uint8_t *data)
{
	uint8_t cmdtosend = 0;

	cmdtosend = R_RX_PAYLOAD;

	//Com_nrf24_rx_cmd(spi_device_handle_t spi, const uint8_t cmd, uint8_t* rcv_buffer, uint8_t rx_len, bool keep_cs_active)
	Com_nrf24_rx_cmd(spi, cmdtosend, data, 1, 1);
	//Com_nrf24_cmd(spi, &cmdtosend, data, 1);

}

uint8_t Com_nrf24_dataAvailable(int pipenum)
{
	uint8_t status = Com_nrf24_readReg(STATUS);

	if ((status&(1<<6)) && (status&(pipenum<<1)))
	{
		Com_nrf24_writeReg(STATUS, (1<<6), 1);

		return 1;
	}
	return 0;
}

void Com_nrf24_main()
{
	uint8_t buf_len = 4;
	uint8_t recv_buffer[buf_len];

    Com_nrf24_RxMode();

	while(1)
	{
		if(Com_nrf24_dataAvailable(2) ==1)
		{
			Com_nrf24_readBuffer(recv_buffer);
		}
#ifdef DEBUG
		//ESP_LOGI(LOG, "%d / %d / %d / %d\n", recv_buffer[0], recv_buffer[1], recv_buffer[2], recv_buffer[3]);
#endif
		vTaskDelay( 100/portTICK_RATE_MS );
	}

}

void Com_nrf24_rx_cmd(spi_device_handle_t spi, const uint8_t cmd, uint8_t* rcv_buffer, uint8_t rx_len, bool keep_cs_active)
{
	esp_err_t ret;
	spi_transaction_t t = {
		.cmd = cmd,
		.length = 8*rx_len,
		.rxlength = 8*rx_len,
	};

	if(rx_len > 4) {
		t.rx_buffer = rcv_buffer;
	}
	else {
		t.tx_data[0] = cmd;
		t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
	}

    t.user=(void*)0;                //D/C needs to be set to 0
	spi_device_transmit(spi, &t);
	ESP_LOGI(LOG, "cmd tx : %d, cmd rx : ", cmd);
	if(rx_len > 4) {
		for(int i = 0; i < rx_len; i++) {
			ESP_LOGI(LOG, "%d / ", rcv_buffer[i]);
		}
	}
	else {
		//for(int i = 0; i < 4; i++) {
		ESP_LOGI(LOG, "%d / %d / %d / %d", t.rx_data[0], t.rx_data[1], t.rx_data[2], t.rx_data[3]);
		//}
	}
	//ESP_LOGI(LOG, "\n");

    assert(ret==ESP_OK);            //Should have had no issues.
}

void Com_nrf24_cmd(spi_device_handle_t spi, const uint8_t cmd, uint8_t* send_buf, uint8_t size, bool keep_cs_active)
{
    esp_err_t ret;
    spi_transaction_t t = {
		.cmd = (uint16_t)(cmd),
		.length = 8 * size,
		.rxlength = 8 * size,
		.rx_buffer = NULL
    };

    if(size > 1) {
    	t.flags = SPI_TRANS_USE_RXDATA;
    	t.tx_buffer=send_buf;
    }
    else {
    	t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    	t.tx_data[0] = send_buf[0];
    }

    t.user=(void*)0;                //D/C needs to be set to 0
	spi_device_transmit(spi, &t);
	ESP_LOGI(LOG, "cmd tx : %d, %d / rx : %d, %d, %d, %d", cmd, t.tx_data[0], t.rx_data[0], t.rx_data[1], t.rx_data[2], t.rx_data[3]);
    assert(ret==ESP_OK);            //Should have had no issues.
}

/*
* nrf24 read command
*/
void Com_nrf24_read(uint8_t * buf, uint8_t len)
{
	int idx = 0;
	//uint8_t recv_buffer[buf_len];
	Com_nrf24_cmd(spi, 0x61, NULL, 0, true); 		// Read RX Buffer
	while(idx < len) {
		Com_nrf24_cmd(spi, 0xFF, NULL, 0, true);
		buf++;
		idx++;
	}
	Com_nrf24_cmd(spi, 0b11100010, NULL, 0, true); // Flush RX Buffer
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

   			if ( httpd_query_key_value(qur, "FLA", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_flash(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "FLA : %d", charToInt(control,1));
#endif
   			}

   			if ( httpd_query_key_value(qur, "ROT_EN", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_rotation_enable(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "ROT_EN : %d", charToInt(control,1));
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

   			if ( httpd_query_key_value(qur, "FLA", control, sizeof(control)) == ESP_OK )
   			{
   				set_value_flash(charToInt(control,1));
#ifdef DEBUG
   				ESP_LOGI(LOG, "FLA : %d", charToInt(control,1));
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
