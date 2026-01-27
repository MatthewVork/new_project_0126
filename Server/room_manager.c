#include <stdio.h>
#include "server_data.h" // 一定要包含这个，才能认识 Room 和 Player 结构

// 初始化所有房间
void init_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].room_id = i;
        rooms[i].player_count = 0;
        rooms[i].status = 0; // 0=Wait
        rooms[i].white_player_idx = -1; // -1 表示空
        rooms[i].black_player_idx = -1;
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
        printf("[Room] 房间 %d 已清空，系统已自动回收该房间\n", room_id);
    } else {
        // 如果还剩 1 个人，将房间状态改回“等待中”
        room->status = 0; 
        printf("[Room] 玩家 (ID:%d) 离开房间 %d，剩余人数: %d\n", player_idx, room_id, room->player_count);
    }

    return 1;
}