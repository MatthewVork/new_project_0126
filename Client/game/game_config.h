#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// 围棋逻辑参数
#define BOARD_SIZE 19      
#define COLOR_BLACK 0      
#define COLOR_WHITE 1      

// ★★★ UI 绘图参数 (基于 385x385 网格精准修正) ★★★
#define GRID_WIDTH 440

// 核心修正 1: 精确的格子间距 (385 / 18)
#define CELL_SIZE  21.3889f  

// 核心修正 2: 精确的起始偏移 ( (440 - 385) / 2 )
#define BOARD_PADDING 27.5f  

// 棋子直径 (20像素比较合适，不会显得太挤)
#define STONE_SIZE 20     

#endif