#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <stdint.h>

// 初始化网络连接
int net_init(const char* server_ip, int port);

// 接收数据（非阻塞）
int net_poll(uint8_t* out_pkt_buffer);

// --- 业务发送函数 ---
void net_send_login(const char* username, const char* password);
void net_send_register(const char* username, const char* password);
void net_send_logout();
void net_send_create_room();
void net_send_join_room(int room_id);
void net_send_get_room_list();

// --- 新增的函数 (解决警告的关键) ---
void net_send_ready();
void net_send_leave_room();

#endif