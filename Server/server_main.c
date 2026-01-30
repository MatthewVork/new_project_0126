#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "server_data.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

Player players[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

void init_server_data() {
    memset(players, 0, sizeof(players));
    init_rooms();
}

// 辅助发送函数
void send_json_result(int fd, const char* cmd, int success, const char* msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd);
    cJSON_AddNumberToObject(root, KEY_SUCCESS, success);
    cJSON_AddStringToObject(root, KEY_MESSAGE, msg);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        send(fd, str, strlen(str), 0);
        free(str);
    }
    cJSON_Delete(root);
}

// ★★★ 核心修复：处理单个 JSON 的业务逻辑 ★★★
void process_server_json(int client_idx, cJSON *json) {
    int sd = players[client_idx].socket_fd;
    cJSON *cmdItem = cJSON_GetObjectItem(json, KEY_CMD);
    if (!cmdItem || !cJSON_IsString(cmdItem)) return;
    
    char *cmd = cmdItem->valuestring;

    // 1. 登录
    if (strcmp(cmd, CMD_STR_LOGIN) == 0) {
        cJSON *uItem = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *pItem = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (uItem && pItem) {
            int status = check_login(uItem->valuestring, pItem->valuestring);
            if (status == 1) {
                players[client_idx].state = STATE_LOBBY;
                strncpy(players[client_idx].username, uItem->valuestring, 31);
                send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Login OK");
                printf("用户 %s 登录成功\n", players[client_idx].username);
            } else if (status == -1) {
                send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Already Logged In");
            } else {
                send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Wrong Password/User");
            }
        }
    }
    // 2. 注册
    else if (strcmp(cmd, CMD_STR_REGISTER) == 0) {
        cJSON *uItem = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *pItem = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (uItem && pItem) {
            if (check_register(uItem->valuestring, pItem->valuestring)) {
                send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Register Success");
            } else {
                send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Username Exists");
            }
        }
    }
    // 3. 登出
    else if (strcmp(cmd, CMD_STR_LOGOUT) == 0) {
        handle_logout(client_idx);
    }
    // 4. 创建房间
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
            send(sd, s, strlen(s), 0);
            free(s);
            cJSON_Delete(res);

            usleep(50000);
            broadcast_room_info(rid);
        } else {
            send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Create Failed (Max Rooms)");
        }
    }
    // 5. 加入房间
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
                send(sd, s, strlen(s), 0);
                free(s);
                cJSON_Delete(res);

                usleep(50000);
                broadcast_room_info(rid);
            } else {
                send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Room Full or Not Found");
            }
        }
    }
    // 6. 离开房间
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
    // 7. 获取列表
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
        send(sd, s, strlen(s), 0);
        free(s);
        cJSON_Delete(res);
    }
    // 8. 准备
    else if (strcmp(cmd, CMD_STR_READY) == 0 || strcmp(cmd, CMD_STR_CANCEL_READY) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            handle_ready_toggle(rid, client_idx, (strcmp(cmd, CMD_STR_READY) == 0));
            int can_start = 0;
            if (rooms[rid].player_count == 2 && rooms[rid].white_ready && rooms[rid].black_ready) {
                can_start = 1;
            }
            if (can_start) {
                rooms[rid].status = 2;
                usleep(50000);
                broadcast_game_start(rid);
            } else {
                broadcast_room_info(rid);
            }
        }
    }
    // 9. 落子 (重点修复：安全检查)
    else if (strcmp(cmd, CMD_STR_PLACE_STONE) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            cJSON *xItem = cJSON_GetObjectItem(json, KEY_X);
            cJSON *yItem = cJSON_GetObjectItem(json, KEY_Y);
            
            // ★★★ 安全检查：防止空指针崩溃 ★★★
            if (xItem && yItem) {
                handle_place_stone(rid, client_idx, xItem->valueint, yItem->valueint);
            } else {
                printf("[Server] 收到非法落子包(缺坐标)，忽略\n");
            }
        }
    }
}

int main() {
    int server_fd, new_socket, max_sd, activity, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    uint8_t buffer[1024];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { exit(EXIT_FAILURE); }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    init_server_data();
    printf("=== 围棋服务器启动 (Port: %d) ===\n", SERVER_PORT);

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
            printf("新连接: %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(players[i].socket_fd == 0) {
                    players[i].socket_fd = new_socket;
                    players[i].state = STATE_CONNECTED;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if (FD_ISSET(sd, &readfds)) {
                // 安全读取
                valread = read(sd, buffer, 1023);

                if (valread == 0) {
                    close(sd);
                    handle_disconnect(i);
                } else {
                    buffer[valread] = '\0'; 

                    // ★★★ 核心修复：服务器端也支持粘包处理 ★★★
                    char *parse_ptr = (char*)buffer;
                    while (*parse_ptr == '{') {
                        const char *end_ptr = NULL;
                        cJSON *json = cJSON_ParseWithOpts(parse_ptr, &end_ptr, 0);
                        if (json) {
                            process_server_json(i, json);
                            cJSON_Delete(json); // 必须释放
                            
                            // 移动指针到下一个 JSON
                            parse_ptr = (char*)end_ptr;
                            while (*parse_ptr && *parse_ptr != '{') parse_ptr++;
                        } else {
                            break; // 解析失败，停止
                        }
                    }
                }
            }
        }
    }
    return 0;
}