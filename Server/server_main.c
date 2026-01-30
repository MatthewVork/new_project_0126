#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h> // 必须包含，用于 htonl/ntohl

#include "server_data.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

Player players[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

void init_server_data() {
    memset(players, 0, sizeof(players));
    init_rooms();
}

// ★★★ 核心工具：发送带长度头的数据包 ★★★
void send_packet(int fd, const char* json_str) {
    if (fd <= 0 || !json_str) return;
    
    int len = strlen(json_str);
    int net_len = htonl(len); // 转为网络大端序
    
    // 1. 发送 4字节长度
    send(fd, &net_len, 4, 0);
    // 2. 发送 数据本体
    send(fd, json_str, len, 0);
}

// 辅助发送结果
void send_json_result(int fd, const char* cmd, int success, const char* msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd);
    cJSON_AddNumberToObject(root, KEY_SUCCESS, success);
    cJSON_AddStringToObject(root, KEY_MESSAGE, msg);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        send_packet(fd, str); // ★ 改用封装好的发送
        free(str);
    }
    cJSON_Delete(root);
}

// --- 业务逻辑 ---
void process_server_json(int client_idx, cJSON *json) {
    int sd = players[client_idx].socket_fd;
    cJSON *cmdItem = cJSON_GetObjectItem(json, KEY_CMD);
    if (!cmdItem || !cJSON_IsString(cmdItem)) return;
    char *cmd = cmdItem->valuestring;

    // 1. 登录
    if (strcmp(cmd, CMD_STR_LOGIN) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (u && p) {
            int status = check_login(u->valuestring, p->valuestring);
            if (status == 1) {
                players[client_idx].state = STATE_LOBBY;
                strncpy(players[client_idx].username, u->valuestring, 31);
                send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Login OK");
                printf("User %s Logged in\n", players[client_idx].username);
            } else if (status == -1) send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Already Logged In");
            else send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Wrong Password/User");
        }
    }
    // 2. 注册
    else if (strcmp(cmd, CMD_STR_REGISTER) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (u && p) {
            if (check_register(u->valuestring, p->valuestring)) send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Register Success");
            else send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Username Exists");
        }
    }
    // 3. 登出
    else if (strcmp(cmd, CMD_STR_LOGOUT) == 0) {
        handle_logout(client_idx);
    }
    // 4. 建房
    else if (strcmp(cmd, CMD_STR_CREATE_ROOM) == 0) {
        int rid = create_room_logic(client_idx);
        if (rid != -1) {
            players[client_idx].state = STATE_IN_ROOM;
            players[client_idx].current_room_id = rid;
            
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_RESULT);
            cJSON_AddNumberToObject(res, KEY_SUCCESS, 1);
            cJSON_AddStringToObject(res, KEY_MESSAGE, "Room Created");
            cJSON_AddNumberToObject(res, KEY_ROOM_ID, rid);
            char *s = cJSON_PrintUnformatted(res);
            
            send_packet(sd, s); // ★ 改为带头发送
            
            free(s);
            cJSON_Delete(res);

            usleep(50000);
            broadcast_room_info(rid);
        } else {
            send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Create Failed");
        }
    }
    // 5. 加入
    else if (strcmp(cmd, CMD_STR_JOIN_ROOM) == 0) {
        cJSON *ridItem = cJSON_GetObjectItem(json, KEY_ROOM_ID);
        if (ridItem) {
            int rid = ridItem->valueint;
            if (join_room_logic(rid, client_idx)) {
                players[client_idx].state = STATE_IN_ROOM;
                players[client_idx].current_room_id = rid;
                
                cJSON *res = cJSON_CreateObject();
                cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_RESULT);
                cJSON_AddNumberToObject(res, KEY_SUCCESS, 1);
                cJSON_AddStringToObject(res, KEY_MESSAGE, "Joined Room");
                cJSON_AddNumberToObject(res, KEY_ROOM_ID, rid);
                char *s = cJSON_PrintUnformatted(res);
                
                send_packet(sd, s); // ★ 改为带头发送
                
                free(s);
                cJSON_Delete(res);

                usleep(50000);
                broadcast_room_info(rid);
            } else {
                send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Room Full or Not Found");
            }
        }
    }
    // 6. 离开
    else if (strcmp(cmd, CMD_STR_LEAVE_ROOM) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            if (leave_room_logic(rid, client_idx)) {
                players[client_idx].state = STATE_LOBBY;
                players[client_idx].current_room_id = -1;
                send_json_result(sd, CMD_STR_ROOM_RESULT, 1, "Left Room");
            }
        }
    }
    // 7. 列表
    else if (strcmp(cmd, CMD_STR_GET_ROOM_LIST) == 0) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, KEY_CMD, CMD_STR_ROOM_LIST_RES);
        cJSON *arr = cJSON_CreateArray();
        for(int j=0; j<MAX_ROOMS; j++) {
            if(rooms[j].player_count > 0) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddNumberToObject(item, KEY_ROOM_ID, rooms[j].room_id);
                cJSON_AddNumberToObject(item, KEY_COUNT, rooms[j].player_count);
                cJSON_AddNumberToObject(item, KEY_STATUS, rooms[j].status);
                cJSON_AddItemToArray(arr, item);
            }
        }
        cJSON_AddItemToObject(res, KEY_ROOM_LIST, arr);
        char *s = cJSON_PrintUnformatted(res);
        
        send_packet(sd, s); // ★ 改为带头发送
        
        free(s);
        cJSON_Delete(res);
    }
    // 8. 准备
    else if (strcmp(cmd, CMD_STR_READY) == 0 || strcmp(cmd, CMD_STR_CANCEL_READY) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            handle_ready_toggle(rid, client_idx, (strcmp(cmd, CMD_STR_READY) == 0));
            
            // 检查是否都准备好
            if (rooms[rid].player_count == 2 && rooms[rid].white_ready && rooms[rid].black_ready) {
                rooms[rid].status = 2; // Playing
                usleep(50000);
                broadcast_game_start(rid);
            } else {
                broadcast_room_info(rid);
            }
        }
    }
    // 9. 落子
    else if (strcmp(cmd, CMD_STR_PLACE_STONE) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            cJSON *x = cJSON_GetObjectItem(json, KEY_X);
            cJSON *y = cJSON_GetObjectItem(json, KEY_Y);
            if (x && y) {
                // 转交给 room_manager 处理
                handle_place_stone(rid, client_idx, x->valueint, y->valueint);
            }
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN); 

    int server_fd, new_socket, max_sd, activity;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { exit(EXIT_FAILURE); }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    init_server_data();
    printf("=== 围棋服务器启动 (Port: %d) [Protocol: Header+Body] ===\n", SERVER_PORT);

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if(players[i].socket_fd > 0) FD_SET(players[i].socket_fd, &readfds);
            if(players[i].socket_fd > max_sd) max_sd = players[i].socket_fd;
        }
        
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket > 0) {
                printf("新连接: %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if(players[i].socket_fd == 0) {
                        players[i].socket_fd = new_socket;
                        players[i].state = STATE_CONNECTED;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                // ★★★ 核心修复：基于长度头的读取逻辑 ★★★
                
                // 1. 先读 4 字节头部
                int net_len = 0;
                int n = recv(sd, &net_len, 4, MSG_WAITALL); 
                // MSG_WAITALL: 确保读够4字节才返回，除非断开

                if (n <= 0) {
                    close(sd);
                    handle_disconnect(i);
                } else if (n == 4) {
                    // 2. 解析长度
                    int data_len = ntohl(net_len);
                    
                    // 安全检查：防止超大包攻击
                    if (data_len > 0 && data_len < 10240) {
                        char *json_buf = (char*)malloc(data_len + 1);
                        if (json_buf) {
                            // 3. 读取 data_len 长度的内容
                            int total_read = 0;
                            while(total_read < data_len) {
                                int r = recv(sd, json_buf + total_read, data_len - total_read, 0);
                                if (r <= 0) {
                                    free(json_buf);
                                    close(sd);
                                    handle_disconnect(i);
                                    goto NEXT_CLIENT; // 跳出循环
                                }
                                total_read += r;
                            }
                            
                            // 4. 解析 JSON
                            json_buf[data_len] = '\0';
                            cJSON *json = cJSON_Parse(json_buf);
                            if (json) {
                                process_server_json(i, json);
                                cJSON_Delete(json);
                            } else {
                                printf("JSON解析失败: %s\n", json_buf);
                            }
                            free(json_buf);
                        }
                    } else {
                        printf("非法包长度: %d\n", data_len);
                        close(sd);
                        handle_disconnect(i);
                    }
                }
            }
            NEXT_CLIENT:;
        }
    }
    return 0;
}