#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>      // socket编程所需头文件
#include <arpa/inet.h>       // IP地址转换函数(inet_ntoa等)
#include <sys/select.h>      // select多路复用I/O
#include <signal.h>          // 信号处理
#include <fcntl.h>           // 文件控制选项
#include <errno.h>           // 错误号定义

#include "server_data.h"     // 服务器数据结构定义
#include "../Common/game_protocol.h"  // 游戏协议常量
#include "../Common/cJSON.h" // JSON解析库

// 全局变量定义
Player players[MAX_CLIENTS];  // 玩家数组，最多10个玩家
Room rooms[MAX_ROOMS];        // 房间数组，最多5个房间

// 定义每个客户端的接收缓冲区（蓄水池）
static uint8_t client_buffers[MAX_CLIENTS][4096];  // 每个客户端4096字节缓冲区
static int client_buffer_len[MAX_CLIENTS] = {0};   // 每个缓冲区当前数据长度

// 初始化服务器数据
void init_server_data() {
    memset(players, 0, sizeof(players));  // 将玩家数组清零
    memset(client_buffers, 0, sizeof(client_buffers));  // 清零所有客户端缓冲区
    memset(client_buffer_len, 0, sizeof(client_buffer_len));  // 清零缓冲区长度
    init_rooms();  // 初始化所有房间状态
}

// 发送JSON格式的结果消息给客户端
void send_json_result(int fd, const char* cmd, int success, const char* msg) {
    cJSON *root = cJSON_CreateObject();  // 创建JSON对象
    cJSON_AddStringToObject(root, KEY_CMD, cmd);        // 添加命令字段
    cJSON_AddNumberToObject(root, KEY_SUCCESS, success); // 添加成功标志(0/1)
    cJSON_AddStringToObject(root, KEY_MESSAGE, msg);    // 添加消息文本
    
    char *str = cJSON_PrintUnformatted(root);  // 将JSON转为字符串(无格式)
    if(str) {
        // 纯文本发送，不加头
        send(fd, str, strlen(str), 0);  // 发送JSON字符串
        free(str);  // 释放字符串内存
    }
    cJSON_Delete(root);  // 释放JSON对象内存
}

// 业务逻辑分发：处理客户端发送的JSON命令
void process_server_json(int client_idx, cJSON *json) {
    int sd = players[client_idx].socket_fd;  // 获取客户端的socket描述符
    
    // 从JSON中提取命令字段
    cJSON *cmdItem = cJSON_GetObjectItem(json, KEY_CMD);
    if (!cmdItem || !cJSON_IsString(cmdItem)) return;  // 命令不存在或不是字符串则返回
    char *cmd = cmdItem->valuestring;  // 获取命令字符串

    // 处理登录命令
    if (strcmp(cmd, CMD_STR_LOGIN) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);  // 用户名
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);  // 密码
        if (u && p) {
            int status = check_login(u->valuestring, p->valuestring);  // 验证登录
            if (status == 1) {  // 登录成功
                players[client_idx].state = STATE_LOBBY;  // 状态设为大厅
                strncpy(players[client_idx].username, u->valuestring, 31);  // 保存用户名
                send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Login OK");  // 发送成功响应
                printf("User %s Logged in\n", players[client_idx].username);  // 日志输出
            } else if (status == -1) send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Already Logged In");
            else send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Wrong Password/User");
        }
    }
    // 处理注册命令
    else if (strcmp(cmd, CMD_STR_REGISTER) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (u && p) {
            if (check_register(u->valuestring, p->valuestring)) 
                send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Register Success");
            else 
                send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Username Exists");
        }
    }
    // 处理登出命令
    else if (strcmp(cmd, CMD_STR_LOGOUT) == 0) handle_logout(client_idx);
    
    // 处理创建房间命令
    else if (strcmp(cmd, CMD_STR_CREATE_ROOM) == 0) {
        int rid = create_room_logic(client_idx);  // 创建房间
        if (rid != -1) {  // 创建成功
            players[client_idx].state = STATE_IN_ROOM;  // 状态设为在房间中
            players[client_idx].current_room_id = rid;  // 记录房间ID
            
            // 构建成功响应JSON
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_RESULT);
            cJSON_AddNumberToObject(res, KEY_SUCCESS, 1);
            cJSON_AddStringToObject(res, KEY_MESSAGE, "Room Created");
            cJSON_AddNumberToObject(res, KEY_ROOM_ID, rid);
            
            char *s = cJSON_PrintUnformatted(res);  // 转为字符串
            send(sd, s, strlen(s), 0);  // 发送响应
            free(s);  // 释放内存
            cJSON_Delete(res);  // 释放JSON对象
            
            usleep(50000);  // 等待50ms，确保消息发送完成
            broadcast_room_info(rid);  // 广播房间信息
        } else {
            send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Create Failed");
        }
    }
    // 处理加入房间命令
    else if (strcmp(cmd, CMD_STR_JOIN_ROOM) == 0) {
        cJSON *ridItem = cJSON_GetObjectItem(json, KEY_ROOM_ID);
        if (ridItem) {
            int rid = ridItem->valueint;  // 获取房间ID
            if (join_room_logic(rid, client_idx)) {  // 加入房间
                players[client_idx].state = STATE_IN_ROOM;
                players[client_idx].current_room_id = rid;
                
                cJSON *res = cJSON_CreateObject();
                cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_RESULT);
                cJSON_AddNumberToObject(res, KEY_SUCCESS, 1);
                cJSON_AddStringToObject(res, KEY_MESSAGE, "Joined Room");
                cJSON_AddNumberToObject(res, KEY_ROOM_ID, rid);
                
                char *s = cJSON_PrintUnformatted(res);
                send(sd, s, strlen(s), 0);
                free(s);
                cJSON_Delete(res);
                
                usleep(50000);
                broadcast_room_info(rid);  // 广播更新后的房间信息
            } else {
                send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Room Full/Not Found");
            }
        }
    }
    // 处理离开房间命令
    else if (strcmp(cmd, CMD_STR_LEAVE_ROOM) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1 && leave_room_logic(rid, client_idx)) {
            players[client_idx].state = STATE_LOBBY;  // 状态回到大厅
            players[client_idx].current_room_id = -1;  // 清除房间ID
            send_json_result(sd, CMD_STR_ROOM_RESULT, 1, "Left Room");
        }
    }
    // 处理获取房间列表命令
    else if (strcmp(cmd, CMD_STR_GET_ROOM_LIST) == 0) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_LIST_RES);  // 设置响应命令
        
        cJSON *arr = cJSON_CreateArray();  // 创建JSON数组
        for(int j=0; j<MAX_ROOMS; j++) {
            if(rooms[j].player_count > 0) {  // 只处理有玩家的房间
                cJSON *item = cJSON_CreateObject();
                cJSON_AddNumberToObject(item, KEY_ROOM_ID, rooms[j].room_id);
                cJSON_AddNumberToObject(item, KEY_COUNT, rooms[j].player_count);
                cJSON_AddNumberToObject(item, KEY_STATUS, rooms[j].status);
                cJSON_AddItemToArray(arr, item);  // 添加到数组
            }
        }
        cJSON_AddItemToObject(res, KEY_ROOM_LIST, arr);  // 将数组加入响应
        
        char *s = cJSON_PrintUnformatted(res);
        send(sd, s, strlen(s), 0);
        free(s);
        cJSON_Delete(res);
    }
    // 处理准备/取消准备命令
    else if (strcmp(cmd, CMD_STR_READY) == 0 || strcmp(cmd, CMD_STR_CANCEL_READY) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            // 切换准备状态，true表示准备，false表示取消准备
            handle_ready_toggle(rid, client_idx, (strcmp(cmd, CMD_STR_READY) == 0));
            
            // 检查是否可以开始游戏：两人且都准备
            int can_start = (rooms[rid].player_count == 2 && rooms[rid].white_ready && rooms[rid].black_ready);
            if (can_start) {
                rooms[rid].status = 2;  // 状态设为游戏中
                usleep(50000);  // 等待50ms
                broadcast_game_start(rid);  // 广播游戏开始
            } else {
                broadcast_room_info(rid);  // 广播房间信息更新
            }
        }
    }
    // 处理落子命令
    else if (strcmp(cmd, CMD_STR_PLACE_STONE) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            cJSON *x = cJSON_GetObjectItem(json, KEY_X);  // X坐标
            cJSON *y = cJSON_GetObjectItem(json, KEY_Y);  // Y坐标
            if (x && y) handle_place_stone(rid, client_idx, x->valueint, y->valueint);
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号，避免写关闭的socket导致程序退出

    int server_fd, new_socket, max_sd, activity, valread;
    struct sockaddr_in address;  // 服务器地址结构
    int addrlen = sizeof(address);
    fd_set readfds;  // 读文件描述符集合

    // 创建socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
        exit(EXIT_FAILURE);  // 创建失败则退出
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  // 设置地址重用
    
    // 设置服务器地址
    address.sin_family = AF_INET;  // IPv4
    address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网络接口
    address.sin_port = htons(SERVER_PORT);  // 端口号(转为网络字节序)
    
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));  // 绑定地址
    listen(server_fd, 10);  // 开始监听，最大等待连接数10
    
    init_server_data();  // 初始化服务器数据
    
    printf("=== 围棋服务器启动 (Port: %d) ===\n", SERVER_PORT);  // 启动日志

    // 主循环
    while(1) {
        FD_ZERO(&readfds);  // 清空文件描述符集合
        FD_SET(server_fd, &readfds);  // 将服务器socket加入集合
        max_sd = server_fd;  // 当前最大文件描述符
        
        // 将所有客户端socket加入集合
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if(players[i].socket_fd > 0) {
                FD_SET(players[i].socket_fd, &readfds);
                if(players[i].socket_fd > max_sd) max_sd = players[i].socket_fd;  // 更新最大值
            }
        }
        
        // 使用select等待I/O事件
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        
        // 检查是否有新的客户端连接
        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket > 0) {
                printf("新连接: %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);
                
                // 为新连接分配一个空闲位置
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if(players[i].socket_fd == 0) {  // 找到空闲槽位
                        players[i].socket_fd = new_socket;
                        players[i].state = STATE_CONNECTED;  // 已连接但未登录
                        client_buffer_len[i] = 0;  // 缓冲区长度清零
                        memset(client_buffers[i], 0, 4096);  // 清零缓冲区
                        break;
                    }
                }
            }
        }

        // 检查所有客户端是否有数据可读
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if (FD_ISSET(sd, &readfds)) {  // 该客户端有数据到达
                // 计算缓冲区剩余空间
                int remain_size = 4096 - client_buffer_len[i] - 1;
                if (remain_size <= 0) { 
                    client_buffer_len[i] = 0;  // 缓冲区满，重置
                    remain_size = 4095;
                }

                // 读取数据到缓冲区
                valread = read(sd, client_buffers[i] + client_buffer_len[i], remain_size);

                if (valread <= 0) {  // 读取失败或连接关闭
                    close(sd);  // 关闭socket
                    handle_disconnect(i);  // 处理断开连接
                    client_buffer_len[i] = 0;  // 重置缓冲区长度
                } else {
                    client_buffer_len[i] += valread;  // 更新缓冲区长度
                    client_buffers[i][client_buffer_len[i]] = '\0';  // 添加字符串结束符

                    // 蓄水池解析逻辑：循环解析缓冲区中的完整JSON
                    int parse_offset = 0;  // 已解析数据的偏移量
                    while (parse_offset < client_buffer_len[i]) {
                        // 查找JSON开始符'{'
                        char *start_ptr = strchr((char*)client_buffers[i] + parse_offset, '{');
                        if (!start_ptr) break;  // 没有找到，跳出循环
                        
                        // 计算开始符在缓冲区中的位置
                        parse_offset = (uint8_t*)start_ptr - client_buffers[i];
                        const char *end_ptr = NULL;
                        
                        // 解析JSON（支持多个JSON对象粘包）
                        cJSON *json = cJSON_ParseWithOpts(start_ptr, &end_ptr, 0);
                        
                        if (json) {  // 解析成功
                            process_server_json(i, json);  // 处理JSON命令
                            cJSON_Delete(json);  // 释放JSON对象
                            
                            // 更新解析位置到JSON结束处
                            parse_offset = (uint8_t*)end_ptr - client_buffers[i];
                        } else {
                            break;  // 解析失败，跳出循环
                        }
                    }

                    // 处理完已解析的数据后，移动未处理的数据到缓冲区开头
                    if (parse_offset > 0) {
                        int remaining = client_buffer_len[i] - parse_offset;  // 剩余未处理数据
                        if (remaining > 0) {
                            // 将未处理数据移到缓冲区开头
                            memmove(client_buffers[i], client_buffers[i] + parse_offset, remaining);
                        }
                        client_buffer_len[i] = remaining;  // 更新缓冲区长度
                        
                        // 清零缓冲区剩余部分（可选，便于调试）
                        memset(client_buffers[i] + client_buffer_len[i], 0, 4096 - client_buffer_len[i]);
                    }
                }
            }
        }
    }
    return 0;
}