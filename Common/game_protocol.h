#ifndef GAME_PROTOCOL_H
#define GAME_PROTOCOL_H

#define SERVER_PORT 8888

// --- 基础命令 ---
#define CMD_LOGIN         0x10 
#define CMD_REGISTER      0x11 
#define CMD_AUTH_RESULT   0x12 
#define CMD_LOGOUT        0x13 

// --- 大厅命令 ---
#define CMD_GET_ROOM_LIST 0x20 
#define CMD_ROOM_LIST_RES 0x21 
#define CMD_CREATE_ROOM   0x22 
#define CMD_JOIN_ROOM     0x23 
#define CMD_ROOM_RESULT   0x24 

// --- 房间准备命令 ---
#define CMD_READY         0x25  // 发送准备
#define CMD_LEAVE_ROOM    0x26  // 离开房间
#define CMD_ROOM_UPDATE   0x27  // 房间状态更新(刷新名字/准备状态)

// --- 游戏命令 ---
#define CMD_GAME_START    0x30 
#define CMD_MOVE_PIECE    0x31 
#define CMD_GAME_OVER     0x32 
#define CMD_SURRENDER     0x33 

#endif