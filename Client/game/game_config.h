#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// 围棋逻辑参数
#define BOARD_SIZE 19      // 19路棋盘
#define COLOR_BLACK 0      // 黑棋代号
#define COLOR_WHITE 1      // 白棋代号

// UI 绘图参数 (根据你的 800x480 屏幕和 440x440 棋盘容器调整)
#define GRID_WIDTH 440     // 棋盘UI总宽度
#define CELL_SIZE  22      // 格子间距 (440 / 20 ≈ 22)
#define STONE_SIZE 18      // 棋子直径
#define BOARD_PADDING 10   // 棋盘内边距

#endif