#include <stdio.h>
#include <string.h>
#include "server_data.h" // 一定要包含这个，才能认识 Room 和 Player 结构
#include "../Common/game_protocol.h"    

// 初始化所有房间
void init_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].room_id = i;
        rooms[i].player_count = 0;
        rooms[i].status = 0; // 0=Wait
        rooms[i].white_player_idx = -1; // -1 表示空
        rooms[i].black_player_idx = -1;
        rooms[i].white_ready = 0; // 初始化为未准备
        rooms[i].black_ready = 0;
    }
    printf("[System] 房间系统初始化完成 (%d 个房间)\n", MAX_ROOMS);
}

// 创建房间逻辑
// 返回值：成功返回房间ID，失败返回 -1
int create_room_logic(int player_idx) {
    // 遍历找一个空房间
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].player_count == 0) {
            // 找到了！初始化这个房间
            rooms[i].player_count = 1;
            rooms[i].status = 0; // 等待对手
            
            // 默认创建者先进入（这里简单设为执红/白方）
            rooms[i].white_player_idx = player_idx;
            rooms[i].black_player_idx = -1;
            
            printf("[Room] 玩家 %s (ID:%d) 创建了房间 %d\n", players[player_idx].username, player_idx, i);
            broadcast_room_info(i);
            return i;
        }
    }
    return -1; // 没有空房间了
}

// 加入房间逻辑
// 返回值：1=成功，0=失败
int join_room_logic(int room_id, int player_idx) {
    // 1. 校验房间号
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    
    // 2. 校验房间是否已满
    if (rooms[room_id].player_count >= 2) return 0;
    
    // 3. 加入逻辑
    if (rooms[room_id].player_count == 1) {
        rooms[room_id].player_count = 2;
        rooms[room_id].status = 1; // 满员，状态变为“游戏中”
        
        // 补上黑方的位置
        // (严谨逻辑应该判断哪边是-1，这里简单假设白方已有人)
        rooms[room_id].black_player_idx = player_idx;
        
        printf("[Room] 玩家 %s (ID:%d) 加入了房间 %d. 游戏开始!\n", players[player_idx].username, player_idx, room_id);

        broadcast_room_info(room_id);

        return 1;
    }
    
    return 0; // 其他情况视为失败
}

// 退出房间逻辑：负责修改房间数组的状态
int leave_room_logic(int room_id, int player_idx) {
    // 1. 安全检查：防止非法房间号
    if (room_id < 0 || room_id >= MAX_ROOMS) return 0;
    
    // 获取房间指针
    Room *room = &rooms[room_id];

    // 2. 检查玩家是否在该房间，并清理位置
    if (room->white_player_idx == player_idx) {
        room->white_player_idx = -1;
        room->player_count--;
    } else if (room->black_player_idx == player_idx) {
        room->black_player_idx = -1;
        room->player_count--;
    } else {
        return 0; // 玩家不在该房间内
    }

    // 3. 核心逻辑：无人则销毁
    if (room->player_count <= 0) {
        room->player_count = 0;
        room->status = 0; // 重置为等待状态
        room->white_player_idx = -1;
        room->black_player_idx = -1;
        room->white_ready = 0;
        room->black_ready = 0;
        printf("[Room] 房间 %d 已清空，系统已自动回收该房间\n", room_id);
    } else {
        // 如果还剩 1 个人，将房间状态改回“等待中”
        room->status = 0; 
        printf("[Room] 玩家 (ID:%d) 离开房间 %d，剩余人数: %d\n", player_idx, room_id, room->player_count);
        broadcast_room_info(room_id);
    }

    return 1;
}

// --- 新增函数：广播房间最新状态给房间内的所有人 ---
void broadcast_room_info(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];

    RoomUpdatePacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = CMD_ROOM_UPDATE;
    pkt.room_id = room_id;

    // 填充玩家1 (白方) 信息
    if (r->white_player_idx != -1) {
        strncpy(pkt.p1_name, players[r->white_player_idx].username, 32);
        pkt.p1_state = 1;
        pkt.p1_ready = r->white_ready;
    } else {
        pkt.p1_state = 0; // 空位
        pkt.p1_ready = 0;
    }

    // 填充玩家2 (黑方) 信息
    if (r->black_player_idx != -1) {
        strncpy(pkt.p2_name, players[r->black_player_idx].username, 32);
        pkt.p2_state = 1;
        pkt.p2_ready = r->black_ready;
    } else {
        pkt.p2_state = 0; // 空位
        pkt.p2_ready = 0;
    }

    // 发送给白方
    if (r->white_player_idx != -1) {
        send(players[r->white_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    }
    // 发送给黑方
    if (r->black_player_idx != -1) {
        send(players[r->black_player_idx].socket_fd, &pkt, sizeof(pkt), 0);
    }
    
    printf("[Room] 房间 %d 状态已同步 (P1: %s, P2: %s)\n", room_id, pkt.p1_name, pkt.p2_name);
}

// 广播游戏开始信息给房间内的双方玩家
void broadcast_game_start(int room_id) {
    Room *r = &rooms[room_id];
    GameStartPacket pkt;
    pkt.cmd = CMD_GAME_START; // 0x30

    // 准备数据：从全局 players 数组获取名字
    strncpy(pkt.red_name, players[r->white_player_idx].username, 32);
    strncpy(pkt.black_name, players[r->black_player_idx].username, 32);

    // 1. 发送给红方 (白方下标)
    if (r->white_player_idx != -1) {
        pkt.your_side = 0; // 标记你是红方
        int fd = players[r->white_player_idx].socket_fd;
        send(fd, &pkt, sizeof(pkt), 0);
    }

    // 2. 发送给黑方
    if (r->black_player_idx != -1) {
        pkt.your_side = 1; // 标记你是黑方
        int fd = players[r->black_player_idx].socket_fd;
        send(fd, &pkt, sizeof(pkt), 0);
    }
    
    printf("[Broadcast] 房间 %d 的游戏开始信息已同步给双方\n", room_id);
}

// 处理准备和取消准备
void handle_ready_toggle(int room_id, int player_idx, int is_ready) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return;
    Room *r = &rooms[room_id];

    // 如果游戏已经开始了，就不允许更改准备状态
    if (r->status == 2) return; 

    // 根据是哪一方来修改标记
    if (r->white_player_idx == player_idx) {
        r->white_ready = is_ready;
    } else if (r->black_player_idx == player_idx) {
        r->black_ready = is_ready;
    }

    printf("[Room] 房间 %d: 玩家 %d %s\n", room_id, player_idx, is_ready ? "已准备" : "取消准备");
    broadcast_room_info(room_id);
}