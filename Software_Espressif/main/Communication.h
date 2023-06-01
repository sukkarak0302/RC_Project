/*
 * Communication.h
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */
/*
 *             5V ----------  3V
 *            GND ----------  16
 * GPIO_MOSI - 12 ----------   0
 * GPIO_MISO - 13 ---------- GND
 * GPIO_SCLK - 15 ---------- VCC
 * GPIO_CS   - 14 ---------- UAR (3) -
 *  			2 ---------- UAT (1) -
 *  			4 ---------- GND
 *
 *
 */
#ifndef MAIN_COMMUNICATION_H_
#define MAIN_COMMUNICATION_H_

#include "esp_wifi.h"
#include "esp_http_server.h"
#include "driver/spi_master.h"
#include "NRF24L01.h"

#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14

int Com_init();
int Com_stop();
int get_main(httpd_req_t *req);
int get_phone(httpd_req_t *req);
int get_joy(httpd_req_t *req);
int get_key(httpd_req_t *req);
int get_joyControl(httpd_req_t *req);

void Com_nrf24_init();
void Com_nrf24_main();
void Com_nrf24_receive(void* pvParameter);
void Com_nrf24_read(uint8_t * buf, uint8_t len);
void Com_nrf24_cmd(spi_device_handle_t spi, const uint8_t cmd, uint8_t* send_buf, uint8_t size, bool keep_cs_active);
void Com_nrf24_rx_cmd(spi_device_handle_t spi, const uint8_t cmd, uint8_t* rcv_buffer, uint8_t rx_len, bool keep_cs_active);
void Com_nrf24_writeReg(uint8_t reg, uint8_t* data, uint8_t size);
uint8_t Com_nrf24_readReg(uint8_t reg);
void Com_nrf24_RxMode();
void Com_nrf24_readBuffer(uint8_t *data);
uint8_t Com_nrf24_dataAvailable(int pipenum);

int Com_server_uri();
void Com_start_server();
int charToInt(char *buf, int limit);

int Com_server_uri_stream();
void Com_start_server_stream();

void rf433_init();
void Com_rf433_receive(void* pvParameter);

#endif /* MAIN_COMMUNICATION_H_ */
