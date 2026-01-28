#ifndef BOARD_VIEW_H
#define BOARD_VIEW_H

#include "../lvgl/lvgl.h"
#include "game_config.h"

// 1. 在指定容器(parent)的 (x,y) 处画一颗棋子
// x,y: 0-18
// color: 0=黑, 1=白
void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color);

// 2. 清空棋盘上的所有棋子
void board_view_clear(lv_obj_t *parent);

// 3. 处理点击：把屏幕坐标转换成棋盘网格坐标
// 返回值: 1=点击有效, 0=点击无效(点在界外)
// 结果存入 out_x, out_y
int board_view_get_click_xy(lv_obj_t *board_obj, int *out_x, int *out_y);

#endif