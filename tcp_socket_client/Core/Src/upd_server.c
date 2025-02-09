/*
 * upd_server.c
 *
 *  Created on: 17 Apr 2023
 *      Author: dark-on
 */

#include "main.h"
#include "lwip.h"
#include "sockets.h"
#include "cmsis_os.h"
#include <string.h>

#if defined(USE_UDP_SERVER) || !defined(USE_TCP_SERVER) || !defined(USE_HTTP_SERVER)
#define PORTNUM 5678UL
#else
#ifndef PORTNUM
#define PORTNUM 1500UL
#endif
#endif

#if (USE_UDP_SERVER_PRINTF == 1)
#include <stdio.h>
#define UDP_SERVER_PRINTF(...) do { printf("[udp_server.c: %s: %d]: ",__func__, __LINE__);printf(__VA_ARGS__); } while (0)
#else
#define UDP_SERVER_PRINTF(...)
#endif

static struct sockaddr_in serv_addr, client_addr;
static int socket_fd;
static uint16_t nport;

#include "cmsis_os.h"

int udp_parse_led_cmd(char * buffer);

static int udpServerInit(void)
{
	// socket()
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0) {
		UDP_SERVER_PRINTF("socket() error\n");
		return -1;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	bzero(&client_addr, sizeof(client_addr));

	// filling server information
	nport = htons((uint16_t) PORTNUM);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = nport;

	// bind()
	if( bind(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		UDP_SERVER_PRINTF("bind() error\n");
		close(socket_fd);
		return -1;
	}

	UDP_SERVER_PRINTF("Server is ready\n");
	return 0;
}

void StartUdpServerTask(void const * argument)
{
	osDelay(5000);// wait 5 sec to init lwip stack

	if(udpServerInit() < 0) {
		UDP_SERVER_PRINTF("udpSocketServerInit() error\n");
		osThreadTerminate(NULL);
		return;
	}

	int addr_len = sizeof(client_addr);


	ssize_t nbytes;
	char buffer[256];

	memset(buffer, 8, 256);

	// recvfrom() + sendto()
	while ( (nbytes = recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len)) > 0 )
	{
		buffer[nbytes] = '\0';
		UDP_SERVER_PRINTF("Client: %s\n", inet_ntoa(client_addr.sin_addr));

		if (strncmp(buffer, "exit", strlen("exit")) == 0){
			// checking is it exit command
			sendto(socket_fd, "goodby!", strlen("goodby!"), 0, (struct sockaddr *)&client_addr, addr_len);
			break;
		}else if(strncmp(buffer, "sversion", strlen("sversion")) == 0){
			// checking is it version command
			sendto(socket_fd, "udp_srv_svitlana_drozd_17042023\r\n", strlen("udp_srv_svitlana_drozd_17042023\r\n"), 0, (struct sockaddr *)&client_addr, addr_len);
		}else if(strncmp(buffer, "led", 3) == 0){
			// checking is it commands with leds
			int ret_code = udp_parse_led_cmd(buffer);
			if (ret_code < 0){
				sendto(socket_fd, "ERROR\r\n", strlen("ERROR\r\n"), 0, (struct sockaddr *)&client_addr, addr_len);
			}else{
				if (ret_code == 0){
					sendto(socket_fd, buffer, 10, 0, (struct sockaddr *)&client_addr, addr_len);
				}
				sendto(socket_fd, "OK\r\n", strlen("OK\r\n"), 0, (struct sockaddr *)&client_addr, addr_len);
			}
		}else if (sendto(socket_fd, buffer, nbytes, 0, (struct sockaddr *)&client_addr, addr_len) < 0){
			// unexpected command
			sendto(socket_fd, "ERROR\r\n", strlen("ERROR\r\n"), 0, (struct sockaddr *)&client_addr, addr_len);
			UDP_SERVER_PRINTF("sendto() error\n");
		}
	} //while

	close(socket_fd);
} //StartUdpServerTask


int udp_parse_led_cmd(char * buffer){
	// split to 2 parts
	char *cmdpart1 = strtok(buffer, " ");
	char *cmdpart2 = strtok(NULL, " ");

	if( strlen(cmdpart1) != 4 || cmdpart1[3] < '3' || cmdpart1[3] > '6'){
		return -1;
	}

	char led_num = *strpbrk(cmdpart1, "3456");

	if(strncmp(cmdpart2, "on", strlen("on")) == 0){
		// on ---
		udp_led_on_handler(led_num);
		return 1;
	}else if(strncmp(cmdpart2, "off", strlen("off")) == 0){
		// off ---
		udp_led_off_handler(led_num);
		return 1;
	}else if(strncmp(cmdpart2, "toggle", strlen("toggle")) == 0){
		// toggle ---
		udp_led_toggle_handler(led_num);
		return 1;
	}else if(strncmp(cmdpart2, "status", strlen("status")) == 0){
		// status ---
		char * send_buff;

		if (udp_led_status_handler(led_num) == 1){
			char msg[] = "LED. ON\r\n";
			send_buff = msg;
		}else if (udp_led_status_handler(led_num) == 0){
			char msg[] = "LED. OFF\r\n";
			send_buff = msg;
		} else{
			UDP_SERVER_PRINTF("Error reading LED status\n");
			return -1;
		}

		send_buff[3] = cmdpart1[3];
		strcpy(buffer, send_buff);

		return 0;
	}else{
		return -1;
	}
}


