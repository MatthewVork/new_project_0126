#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#include <fcntl.h> // ★ 新增头文件
#include <errno.h> // ★ 新增头文件

#include "server_data.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

Player players[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

static uint8_t client_buffers[MAX_CLIENTS][4096];
static int client_buffer_len[MAX_CLIENTS] = {0};

// ★★★ 新增：设置非阻塞函数 ★★★
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void init_server_data() {
    memset(players, 0, sizeof(players));
    memset(client_buffers, 0, sizeof(client_buffers));
    memset(client_buffer_len, 0, sizeof(client_buffer_len));
    init_rooms();
}

void send_json_result(int fd, const char* cmd, int success, const char* msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, cmd);
    cJSON_AddNumberToObject(root, KEY_SUCCESS, success);
    cJSON_AddStringToObject(root, KEY_MESSAGE, msg);
    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        // 非阻塞发送，忽略错误，防止卡死
        #ifdef MSG_NOSIGNAL
            send(fd, str, strlen(str), MSG_NOSIGNAL);
        #else
            send(fd, str, strlen(str), 0);
        #endif
        free(str);
    }
    cJSON_Delete(root);
}

void process_server_json(int client_idx, cJSON *json) {
    // ... 你的原有业务逻辑 ...
    // ... 为了篇幅，请直接保留你原来的 process_server_json 内容 ...
    // ... 或者从我上一次回复中复制 ...
    // ⬇️ 这里我放一个精简版占位，请务必把你的 9 个 if-else 逻辑放进去 ⬇️
    int sd = players[client_idx].socket_fd;
    cJSON *cmdItem = cJSON_GetObjectItem(json, KEY_CMD);
    if (!cmdItem || !cJSON_IsString(cmdItem)) return;
    char *cmd = cmdItem->valuestring;

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
    else if (strcmp(cmd, CMD_STR_REGISTER) == 0) {
        cJSON *u = cJSON_GetObjectItem(json, KEY_USERNAME);
        cJSON *p = cJSON_GetObjectItem(json, KEY_PASSWORD);
        if (u && p) {
            if (check_register(u->valuestring, p->valuestring)) send_json_result(sd, CMD_STR_AUTH_RESULT, 1, "Register Success");
            else send_json_result(sd, CMD_STR_AUTH_RESULT, 0, "Username Exists");
        }
    }
    else if (strcmp(cmd, CMD_STR_LOGOUT) == 0) handle_logout(client_idx);
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
        } else send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Create Failed");
    }
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
            } else send_json_result(sd, CMD_STR_ROOM_RESULT, 0, "Room Full/Not Found");
        }
    }
    else if (strcmp(cmd, CMD_STR_LEAVE_ROOM) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1 && leave_room_logic(rid, client_idx)) {
            players[client_idx].state = STATE_LOBBY;
            players[client_idx].current_room_id = -1;
            send_json_result(sd, CMD_STR_ROOM_RESULT, 1, "Left Room");
        }
    }
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
    else if (strcmp(cmd, CMD_STR_READY) == 0 || strcmp(cmd, CMD_STR_CANCEL_READY) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            handle_ready_toggle(rid, client_idx, (strcmp(cmd, CMD_STR_READY) == 0));
            int can_start = (rooms[rid].player_count == 2 && rooms[rid].white_ready && rooms[rid].black_ready);
            if (can_start) {
                rooms[rid].status = 2;
                usleep(50000);
                broadcast_game_start(rid);
            } else broadcast_room_info(rid);
        }
    }
    else if (strcmp(cmd, CMD_STR_PLACE_STONE) == 0) {
        int rid = players[client_idx].current_room_id;
        if (rid != -1) {
            cJSON *x = cJSON_GetObjectItem(json, KEY_X);
            cJSON *y = cJSON_GetObjectItem(json, KEY_Y);
            if (x && y) handle_place_stone(rid, client_idx, x->valueint, y->valueint);
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN); 

    int server_fd, new_socket, max_sd, activity, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { exit(EXIT_FAILURE); }
    
    // 设置服务器 socket 复用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    init_server_data();
    printf("=== 围棋服务器启动 (Port: %d) ===\n", SERVER_PORT);

    // ★★★ 核心修复：把监听 Socket 也设为非阻塞，防止 select 误报卡死 ★★★
    set_nonblocking(server_fd); 

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if(players[i].socket_fd > 0) FD_SET(players[i].socket_fd, &readfds);
            if(players[i].socket_fd > max_sd) max_sd = players[i].socket_fd;
        }
        
        // 使用 timeout 防止 select 永久阻塞
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms
        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            // Select error
        }

        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket > 0) {
                // ★★★ 核心修复：新连接立即设为非阻塞 ★★★
                set_nonblocking(new_socket);
                
                printf("新连接: %s, Socket %d\n", inet_ntoa(address.sin_addr), new_socket);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if(players[i].socket_fd == 0) {
                        players[i].socket_fd = new_socket;
                        players[i].state = STATE_CONNECTED;
                        client_buffer_len[i] = 0;
                        memset(client_buffers[i], 0, 4096);
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = players[i].socket_fd;
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int remain_size = 4096 - client_buffer_len[i] - 1;
                if (remain_size <= 0) { 
                    client_buffer_len[i] = 0; 
                    remain_size = 4095;
                }

                valread = read(sd, client_buffers[i] + client_buffer_len[i], remain_size);

                // 处理断开或错误
                if (valread == 0) {
                    close(sd);
                    handle_disconnect(i);
                    client_buffer_len[i] = 0;
                } 
                else if (valread < 0) {
                    // 如果是 EAGAIN (暂时没数据)，不是错误，忽略
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        close(sd);
                        handle_disconnect(i);
                        client_buffer_len[i] = 0;
                    }
                }
                else {
                    client_buffer_len[i] += valread;
                    client_buffers[i][client_buffer_len[i]] = '\0'; 

                    char *parse_start = (char*)client_buffers[i];
                    char *processed_ptr = parse_start;
                    int parse_offset = 0;
                    
                    while (parse_offset < client_buffer_len[i]) {
                        char *start_ptr = strchr((char*)client_buffers[i] + parse_offset, '{');
                        if (!start_ptr) break; 
                        
                        parse_offset = (uint8_t*)start_ptr - client_buffers[i];
                        const char *end_ptr = NULL;
                        cJSON *json = cJSON_ParseWithOpts(start_ptr, &end_ptr, 0);
                        
                        if (json) {
                            process_server_json(i, json);
                            cJSON_Delete(json);
                            parse_offset = (uint8_t*)end_ptr - client_buffers[i];
                        } else {
                            break;
                        }
                    }

                    if (parse_offset > 0) {
                        int remaining = client_buffer_len[i] - parse_offset;
                        if (remaining > 0) memmove(client_buffers[i], client_buffers[i] + parse_offset, remaining);
                        client_buffer_len[i] = remaining;
                        memset(client_buffers[i] + client_buffer_len[i], 0, 4096 - client_buffer_len[i]);
                    }
                }
            }
        }
    }
    return 0;
}