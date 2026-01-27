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