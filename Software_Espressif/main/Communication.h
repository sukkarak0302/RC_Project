/*
 * Communication.h
 *
 *  Created on: 2022. 3. 15.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_COMMUNICATION_H_
#define MAIN_COMMUNICATION_H_

#include "esp_wifi.h"
#include "esp_http_server.h"

int Com_init();
int Com_stop();
int get_main(httpd_req_t *req);
int get_phone(httpd_req_t *req);
int get_joy(httpd_req_t *req);
int get_key(httpd_req_t *req);

int Com_server_uri();
void Com_start_server();
int charToInt(char *buf, int limit);

int Com_server_uri_stream();
void Com_start_server_stream();

#endif /* MAIN_COMMUNICATION_H_ */
