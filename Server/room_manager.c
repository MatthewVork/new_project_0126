#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
            rooms[i].black_player_idx = player_idx;
            rooms[i].white_player_idx = -1;
            rooms[i].black_ready = 0;
            rooms[i].white_ready = 0;
            memset(rooms[i].board, -1, sizeof(rooms[i].board));

            printf("[Room] 玩家 %s (ID:%d) 创建了房间 %d\n", players[player_idx].username, player_idx, i);
            
            // ★★★ 只加 0.1秒，既不会觉得卡，又能保证不粘包 ★★★
            usleep(100000); 
            broadcast_room_info(i); 
            
            return i;
        }
    }
    return -1;
}

// 5. 加入房间逻辑
int join_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    if (rooms[room_id].player_count >= 2) return 0;

    if (rooms[room_id].player_count == 0) {
        return create_room_logic(player_idx);
    } else {
        rooms[room_id].player_count++;
        // 自动补位
        if (rooms[room_id].black_player_idx == -1) rooms[room_id].black_player_idx = player_idx;
        else rooms[room_id].white_player_idx = player_idx;
        
        printf("[Room] 玩家 %s 加入房间 %d\n", players[player_idx].username, room_id);
        
        // ★★★ 同样只加 0.1秒 ★★★
        usleep(100000);
        broadcast_room_info(room_id);
        
        return 1;
    }
}

// 6. 离开房间逻辑 (含逃跑判负)
int leave_room_logic(int room_id, int player_idx) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    Room *r = &rooms[room_id];

    int is_black_leaving = 0;
    // 移除玩家逻辑
    if (r->black_player_idx == player_idx) {
        r->black_player_idx = -1;
        r->black_ready = 0;
        is_black_leaving = 1;
    } else if (r->white_player_idx == player_idx) {
        r->white_player_idx = -1;
        r->white_ready = 0;
        is_black_leaving = 0;
    } else {
        return 0;
    }

    r->player_count--;
    
    // 房间空了重置
    if (r->player_count <= 0) {
        r->player_count = 0;
        r->status = 0;
        r->black_player_idx = -1;
        r->white_player_idx = -1;
        r->black_ready = 0;
        r->white_ready = 0;
        memset(r->board, 0, sizeof(r->board));
        printf("[Room] 房间 %d 已清空回收\n", room_id);
    } else {
        // 游戏中有人逃跑
        if (r->status == 2) {
            int winner_idx = is_black_leaving ? r->white_player_idx : r->black_player_idx;
            int winner_color = is_black_leaving ? 1 : 0;
            if (winner_idx != -1) {
                GameOverPacket pkt;
                pkt.cmd = CMD_GAME_OVER;
                pkt.winner_color = winner_color;
                send(players[winner_idx].socket_fd, &pkt, sizeof(pkt), 0);
            }
            r->status = 0; 
            r->black_ready = 0;
            r->white_ready = 0;
            memset(r->board, 0, sizeof(r->board)); 
            usleep(200000); // 判负稍微多一点点延时没关系
        }
        
        // ★★★ 重点：必须广播！让剩下的人看到对方变成了 Waiting ★★★
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
    usleep(100000); 
    broadcast_room_info(room_id);
}

// --- 辅助函数：判断五子连珠 ---
// 返回 1 表示获胜，0 表示没赢
int check_win(int rid, int x, int y, int player_color) {
    // 这里的 player_color 是协议里的 0(黑) 或 1(白)
    // 我们要把它转换成存储在 board 里的 1(黑) 或 2(白)
    int target_val = player_color + 1; 

    // 四个方向：横、竖、右斜(\)、左斜(/)
    int directions[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };

    for (int d = 0; d < 4; d++) {
        int count = 1; // 算上当前这一子
        int dx = directions[d][0];
        int dy = directions[d][1];

        // 1. 正向检查 (最多查4格)
        for (int i = 1; i < 5; i++) {
            int nx = x + i * dx;
            int ny = y + i * dy;
            // 检查边界 + 检查颜色是否一致
            if (nx >= 0 && nx < 19 && ny >= 0 && ny < 19 && 
                rooms[rid].board[nx][ny] == target_val) {
                count++;
            } else {
                break;
            }
        }

        // 2. 反向检查 (最多查4格)
        for (int i = 1; i < 5; i++) {
            int nx = x - i * dx;
            int ny = y - i * dy;
            if (nx >= 0 && nx < 19 && ny >= 0 && ny < 19 && 
                rooms[rid].board[nx][ny] == target_val) {
                count++;
            } else {
                break;
            }
        }

        // 3. 结算
        if (count >= 5) return 1; // 赢了！
    }
    return 0; // 没赢
}

// 8. 处理落子逻辑
void handle_place_stone(int rid, int player_idx, int x, int y) {
    if (rid < 0 || rid >= MAX_ROOMS) return;
    
    // 1. 确定落子颜色
    int color = (players[player_idx].socket_fd == players[rooms[rid].black_player_idx].socket_fd) ? 0 : 1;
    
    // 2. 记录到服务器棋盘
    rooms[rid].board[x][y] = color + 1;

    // 3. ★★★ 修正这里：把 MovePacket 改成 StonePacket ★★★
    // (编译器提示你原来的定义叫 StonePacket)
    StonePacket move_pkt; 
    move_pkt.cmd = 0x31; // CMD_MOVE
    move_pkt.x = x;
    move_pkt.y = y;
    move_pkt.color = color;
    
    // 给房间内两人发包
    int p1 = rooms[rid].black_player_idx;
    int p2 = rooms[rid].white_player_idx;
    if(p1 != -1) send(players[p1].socket_fd, &move_pkt, sizeof(move_pkt), 0);
    if(p2 != -1) send(players[p2].socket_fd, &move_pkt, sizeof(move_pkt), 0);

    usleep(50000);

    // 4. 检查获胜 (代码保持不变)
    if (check_win(rid, x, y, color)) {
        printf("[Server] 房间 %d 结束，获胜者颜色: %d\n", rid, color);
        
        GameOverPacket over_pkt;
        over_pkt.cmd = CMD_GAME_OVER;
        over_pkt.winner_color = color;

        if(p1 != -1) send(players[p1].socket_fd, &over_pkt, sizeof(over_pkt), 0);
        if(p2 != -1) send(players[p2].socket_fd, &over_pkt, sizeof(over_pkt), 0);

        usleep(50000);

        // 重置房间
        rooms[rid].status = 0; 
        rooms[rid].black_ready = 0;
        rooms[rid].white_ready = 0;
        memset(rooms[rid].board, 0, sizeof(rooms[rid].board));
        broadcast_room_info(rid);
    }
}