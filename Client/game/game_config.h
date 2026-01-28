#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// 围棋逻辑参数

#define COLOR_BLACK 0      // 黑棋代号
#define COLOR_WHITE 1      // 白棋代号

// 修改 game_config.h
#define BOARD_SIZE 19
#define GRID_WIDTH 440
#define CELL_SIZE  22      // 固定格子大小
#define STONE_SIZE 20      // 棋子直径，稍微大一点好看
#define BOARD_PADDING 22   // (440 - 18*22) / 2 = 22，让网格在容器里完美居中

#endif