#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <unistd.h>
#include "server_data.h"
#include "../Common/game_protocol.h"
#include "../Common/cJSON.h" 

void broadcast_room_info(int room_id);

void init_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].room_id = i;
        rooms[i].player_count = 0;
        rooms[i].status = 0; 
        rooms[i].white_player_idx = -1;
        rooms[i].black_player_idx = -1;
        rooms[i].white_ready = 0;
        rooms[i].black_ready = 0;
        memset(rooms[i].board, -1, sizeof(rooms[i].board));
        rooms[i].current_turn = 0; 
    }
    printf("[System] 房间系统初始化完成\n");
}

void broadcast_room_info(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_ROOM_UPDATE);
    cJSON_AddNumberToObject(root, KEY_ROOM_ID, room_id);

    if (r->black_player_idx != -1) {
        cJSON_AddStringToObject(root, KEY_P1_NAME, players[r->black_player_idx].username);
        cJSON_AddNumberToObject(root, KEY_P1_READY, r->black_ready);
    } else {
        cJSON_AddStringToObject(root, KEY_P1_NAME, ""); 
        cJSON_AddNumberToObject(root, KEY_P1_READY, 0);
    }
    
    if (r->white_player_idx != -1) {
        cJSON_AddStringToObject(root, KEY_P2_NAME, players[r->white_player_idx].username);
        cJSON_AddNumberToObject(root, KEY_P2_READY, r->white_ready);
    } else {
        cJSON_AddStringToObject(root, KEY_P2_NAME, ""); 
        cJSON_AddNumberToObject(root, KEY_P2_READY, 0);
    }

    char *str = cJSON_PrintUnformatted(root);
    if(str) {
        int len = strlen(str);
        if (r->black_player_idx != -1) send(players[r->black_player_idx].socket_fd, str, len, 0);
        if (r->white_player_idx != -1) send(players[r->white_player_idx].socket_fd, str, len, 0);
        free(str);
    }
    cJSON_Delete(root); 
}

void broadcast_game_start(int room_id) {
    Room *r = &rooms[room_id];
    memset(r->board, -1, sizeof(r->board)); 
    r->current_turn = 0; 

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_GAME_START);
    
    if(r->black_player_idx != -1) cJSON_AddStringToObject(root, KEY_P1_NAME, players[r->black_player_idx].username);
    if(r->white_player_idx != -1) cJSON_AddStringToObject(root, KEY_P2_NAME, players[r->white_player_idx].username);

    if (r->black_player_idx != -1) {
        cJSON_AddNumberToObject(root, KEY_YOUR_COLOR, 0); 
        char *str = cJSON_PrintUnformatted(root);
        send(players[r->black_player_idx].socket_fd, str, strlen(str), 0);
        free(str);
        cJSON_DeleteItemFromObject(root, KEY_YOUR_COLOR); 
    }

    if (r->white_player_idx != -1) {
        cJSON_AddNumberToObject(root, KEY_YOUR_COLOR, 1); 
        char *str = cJSON_PrintUnformatted(root);
        send(players[r->white_player_idx].socket_fd, str, strlen(str), 0);
        free(str);
    }
    
    cJSON_Delete(root);
    printf("[Game] 房间 %d 开始游戏\n", room_id);
}

int create_room_logic(int player_idx) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].player_count == 0) {
            rooms[i].player_count = 1;
            rooms[i].status = 0;
            rooms[i].black_player_idx = player_idx;
            rooms[i].white_player_idx = -1;
            rooms[i].black_ready = 0;
            rooms[i].white_ready = 0;
            memset(rooms[i].board, -1, sizeof(rooms[i].board));
            usleep(100000); 
            broadcast_room_info(i); 
            return i;
        }
    }
    return -1;
}

int join_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    if (rooms[room_id].player_count >= 2) return 0;
    if (rooms[room_id].player_count == 0) return create_room_logic(player_idx);
    else {
        rooms[room_id].player_count++;
        if (rooms[room_id].black_player_idx == -1) rooms[room_id].black_player_idx = player_idx;
        else rooms[room_id].white_player_idx = player_idx;
        usleep(100000);
        broadcast_room_info(room_id);
        return 1;
    }
}

int leave_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    Room *r = &rooms[room_id];
    int is_black_leaving = (r->black_player_idx == player_idx);

    if (r->black_player_idx == player_idx) { r->black_player_idx = -1; r->black_ready = 0; }
    else if (r->white_player_idx == player_idx) { r->white_player_idx = -1; r->white_ready = 0; }
    else return 0;

    r->player_count--;
    
    if (r->player_count <= 0) {
        r->player_count = 0; r->status = 0; r->black_player_idx = -1; r->white_player_idx = -1;
        r->black_ready = 0; r->white_ready = 0;
        memset(r->board, 0, sizeof(r->board));
    } else {
        if (r->status == 2) {
            int winner_idx = is_black_leaving ? r->white_player_idx : r->black_player_idx;
            int winner_color = is_black_leaving ? 1 : 0;
            if (winner_idx != -1) {
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_GAME_OVER);
                cJSON_AddNumberToObject(root, KEY_WINNER, winner_color);
                char *str = cJSON_PrintUnformatted(root);
                send(players[winner_idx].socket_fd, str, strlen(str), 0);
                free(str);
                cJSON_Delete(root);
            }
            r->status = 0; r->black_ready = 0; r->white_ready = 0;
            memset(r->board, 0, sizeof(r->board)); 
            usleep(200000); 
        }
        broadcast_room_info(room_id);
    }
    return 1;
}

void handle_ready_toggle(int room_id, int player_idx, int is_ready) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];
    if (r->status == 2) return;
    if (r->black_player_idx == player_idx) r->black_ready = is_ready;
    else if (r->white_player_idx == player_idx) r->white_ready = is_ready;
    usleep(100000); 
    broadcast_room_info(room_id);
}

int check_win(int rid, int x, int y, int player_color) {
    int target_val = player_color + 1; 
    int directions[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };
    for (int d = 0; d < 4; d++) {
        int count = 1;
        int dx = directions[d][0];
        int dy = directions[d][1];
        for (int i = 1; i < 5; i++) {
            int nx = x + i * dx;
            int ny = y + i * dy;
            if (nx >= 0 && nx < 19 && ny >= 0 && ny < 19 && rooms[rid].board[nx][ny] == target_val) count++;
            else break;
        }
        for (int i = 1; i < 5; i++) {
            int nx = x - i * dx;
            int ny = y - i * dy;
            if (nx >= 0 && nx < 19 && ny >= 0 && ny < 19 && rooms[rid].board[nx][ny] == target_val) count++;
            else break;
        }
        if (count >= 5) return 1;
    }
    return 0;
}

void handle_place_stone(int rid, int player_idx, int x, int y) {
    if (rid < 0 || rid >= MAX_ROOMS) return;

    // ★ 核心修复：防止坐标越界导致崩溃
    if (x < 0 || x >= 19 || y < 0 || y >= 19) {
        printf("[Server] 坐标越界(%d,%d)，忽略\n", x, y);
        return;
    }
    
    int color = (players[player_idx].socket_fd == players[rooms[rid].black_player_idx].socket_fd) ? 0 : 1;
    rooms[rid].board[x][y] = color + 1;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CMD, CMD_STR_PLACE_STONE);
    cJSON_AddNumberToObject(root, KEY_X, x);
    cJSON_AddNumberToObject(root, KEY_Y, y);
    cJSON_AddNumberToObject(root, KEY_COLOR, color);
    char *json_str = cJSON_PrintUnformatted(root);
    
    int p1 = rooms[rid].black_player_idx;
    int p2 = rooms[rid].white_player_idx;
    if (json_str) {
        int len = strlen(json_str);
        if(p1 != -1) send(players[p1].socket_fd, json_str, len, 0);
        if(p2 != -1) send(players[p2].socket_fd, json_str, len, 0);
        free(json_str);
    }
    cJSON_Delete(root);

    usleep(50000);

    if (check_win(rid, x, y, color)) {
        printf("[Server] 房间 %d 结束，获胜: %d\n", rid, color);
        
        cJSON *over = cJSON_CreateObject();
        cJSON_AddStringToObject(over, KEY_CMD, CMD_STR_GAME_OVER);
        cJSON_AddNumberToObject(over, KEY_WINNER, color);
        char *ostr = cJSON_PrintUnformatted(over);
        if(ostr) {
            int len = strlen(ostr);
            if(p1 != -1) send(players[p1].socket_fd, ostr, len, 0);
            if(p2 != -1) send(players[p2].socket_fd, ostr, len, 0);
            free(ostr);
        }
        cJSON_Delete(over);

        usleep(50000);
        rooms[rid].status = 0; 
        rooms[rid].black_ready = 0; 
        rooms[rid].white_ready = 0;
        memset(rooms[rid].board, 0, sizeof(rooms[rid].board));
        broadcast_room_info(rid);
    }
}