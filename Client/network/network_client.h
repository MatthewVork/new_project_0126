#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <stdint.h>
// 请确保这个相对路径是正确的，取决于你的Common文件夹在哪
#include "../../Common/game_protocol.h"

// 初始化网络模块
int net_init(const char* server_ip, int port);

// --- 发送功能 ---
// 发送登录
void net_send_login(const char* username, const char* password);

// 发送注册 (新增)
void net_send_register(const char* username, const char* password);

// 发送创建房间
void net_send_create_room();

// 发送加入房间
void net_send_join_room(int room_id);

// --- 接收功能 ---
// 轮询网络数据，供 main 循环调用
int net_poll(uint8_t* out_pkt_buffer);

#endif