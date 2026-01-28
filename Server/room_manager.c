#include <stdio.h>
#include <string.h>
#include "server_data.h"
#include "../Common/game_protocol.h"

// --- 前置声明 ---
void broadcast_room_info(int room_id);

// 1. 初始化所有房间
void init_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].room_id = i;
        rooms[i].player_count = 0;
        rooms[i].status = 0; // 0=Wait
        
        rooms[i].white_player_idx = -1;
        rooms[i].black_player_idx = -1;
        rooms[i].white_ready = 0;
        rooms[i].black_ready = 0;

        // ★ 初始化围棋棋盘 (-1 表示空)
        memset(rooms[i].board, -1, sizeof(rooms[i].board));
        rooms[i].current_turn = 0; // 默认黑先
    }
    printf("[System] 房间系统初始化完成 (%d 个房间)\n", MAX_ROOMS);
}

// 2. 广播房间信息 (显示名字/准备状态)
void broadcast_room_info(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];

    RoomUpdatePacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_ROOM_UPDATE; // 0x26
    pkt.room_id = room_id;

    // P1 (黑方/房主) 信息
    if (r->black_player_idx != -1) {
        strncpy(pkt.p1_name, players[r->black_player_idx].username, 32);
        pkt.p1_state = 1;
        pkt.p1_ready = r->black_ready;
    }
    // P2 (白方/挑战者) 信息
    if (r->white_player_idx != -1) {
        strncpy(pkt.p2_name, players[r->white_player_idx].username, 32);
        pkt.p2_state = 1;
        pkt.p2_ready = r->white_ready;
    }

    // 分别发给房间里的两个人
    if (r->black_player_idx != -1) send(players[r->black_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    if (r->white_player_idx != -1) send(players[r->white_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
}

// 3. ★ 广播游戏开始 (围棋版逻辑) ★
void broadcast_game_start(int room_id) {
    Room *r = &rooms[room_id];
    
    // 关键：开局前清空服务器端的棋盘内存
    memset(r->board, -1, sizeof(r->board)); 
    r->current_turn = 0; // 0 = 黑棋先手

    GameStartPacket pkt;
    memset(&pkt, 0, sizeof(pkt)); // 安全清空
    pkt.cmd = CMD_GAME_START; // 0x30
    
    // 填入双方名字
    if(r->black_player_idx != -1) strncpy(pkt.black_name, players[r->black_player_idx].username, 32);
    if(r->white_player_idx != -1) strncpy(pkt.white_name, players[r->white_player_idx].username, 32);

    // 发给黑方 (P1)
    if (r->black_player_idx != -1) {
        pkt.your_color = 0; // 0=黑
        send(players[r->black_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    }

    // 发给白方 (P2)
    if (r->white_player_idx != -1) {
        pkt.your_color = 1; // 1=白
        send(players[r->white_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    }
    
    printf("[Game] 房间 %d 开始游戏! 黑方:%s vs 白方:%s\n", room_id, pkt.black_name, pkt.white_name);
}

// 4. 创建房间逻辑
int create_room_logic(int player_idx) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].player_count == 0) {
            rooms[i].player_count = 1;
            rooms[i].status = 0;
            
            // 创建者默认为黑方(P1)
            rooms[i].black_player_idx = player_idx;
            rooms[i].white_player_idx = -1;
            
            // 重置状态
            rooms[i].black_ready = 0;
            rooms[i].white_ready = 0;
            memset(rooms[i].board, -1, sizeof(rooms[i].board));

            printf("[Room] 玩家 %s (ID:%d) 创建了房间 %d\n", players[player_idx].username, player_idx, i);
            
            // 立即广播，让房主看到自己的名字
            //broadcast_room_info(i);
            return i;
        }
    }
    return -1;
}

// 5. 加入房间逻辑
int join_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    if (rooms[room_id].player_count >= 2) return 0;

    // 只要有空位就加入
    if (rooms[room_id].player_count == 0) {
        return create_room_logic(player_idx);
    } else {
        rooms[room_id].player_count++;
        
        // 自动补位
        if (rooms[room_id].black_player_idx == -1) {
            rooms[room_id].black_player_idx = player_idx;
        } else {
            rooms[room_id].white_player_idx = player_idx;
        }
        
        printf("[Room] 玩家 %s 加入房间 %d\n", players[player_idx].username, room_id);
        
        //broadcast_room_info(room_id);
        return 1;
    }
}

// 6. 离开房间逻辑
int leave_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    Room *r = &rooms[room_id];

    // 找到玩家并移除
    if (r->black_player_idx == player_idx) {
        r->black_player_idx = -1;
        r->black_ready = 0;
    } else if (r->white_player_idx == player_idx) {
        r->white_player_idx = -1;
        r->white_ready = 0;
    } else {
        return 0; // 不在这个房间
    }

    r->player_count--;
    
    // 如果没人了，重置房间
    if (r->player_count <= 0) {
        r->player_count = 0;
        r->status = 0;
        printf("[Room] 房间 %d 已清空回收\n", room_id);
    } else {
        // 如果还有人，强制结束游戏
        if (r->status == 2) {
            r->status = 0;
            printf("[Room] 房间 %d 游戏因有人退出而强制结束\n", room_id);
        }
        // 通知剩下的那个人
        broadcast_room_info(room_id);
    }
    return 1;
}

// 7. 处理准备/取消准备
void handle_ready_toggle(int room_id, int player_idx, int is_ready) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];

    // 游戏中不能取消准备
    if (r->status == 2) return;

    if (r->black_player_idx == player_idx) {
        r->black_ready = is_ready;
    } else if (r->white_player_idx == player_idx) {
        r->white_ready = is_ready;
    }
    
    broadcast_room_info(room_id);
}

// 8. ★★★ 处理围棋落子 (核心逻辑) ★★★
void handle_place_stone(int room_id, int player_idx, int x, int y) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];

    // (1) 必须在游戏中
    if (r->status != 2) return;

    // (2) 必须轮到该玩家
    // 黑方(P1)对应颜色0, 白方(P2)对应颜色1
    int my_color = (r->black_player_idx == player_idx) ? 0 : 1;
    
    if (my_color != r->current_turn) {
        printf("[Game] 拒绝: 玩家 %s 还没轮到你 (当前轮到: %d)\n", players[player_idx].username, r->current_turn);
        return; 
    }

    // (3) 坐标合法性检查
    if (x < 0 || x >= 19 || y < 0 || y >= 19) return;
    if (r->board[y][x] != -1) {
        printf("[Game] 拒绝: 位置 (%d, %d) 已有子\n", x, y);
        return;
    }

    // (4) 执行落子
    r->board[y][x] = my_color;          // 记录到服务器内存
    r->current_turn = !r->current_turn; // 切换回合 (0->1, 1->0)

    // (5) 广播结果给房间里的两个人
    StonePacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_PLACE_STONE; // 0x31
    pkt.x = x;
    pkt.y = y;
    pkt.color = my_color;

    if (r->black_player_idx != -1) send(players[r->black_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    if (r->white_player_idx != -1) send(players[r->white_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    
    printf("[Game] 房间 %d 落子成功: (%d, %d) 颜色:%d\n", room_id, x, y, my_color);
}