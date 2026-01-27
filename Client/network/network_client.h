#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <stdint.h>
#include "../../Common/game_protocol.h"

// 初始化
int net_init(const char* server_ip, int port);

// 发送功能
void net_send_login(const char* username, const char* password);
void net_send_register(const char* username, const char* password);
void net_send_logout(); // 新增
void net_send_create_room();
void net_send_join_room(int room_id);

// 接收功能
int net_poll(uint8_t* out_pkt_buffer);
void net_send_get_room_list();

#endif