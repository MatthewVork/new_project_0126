#include "board_view.h"
#include <stdio.h>

// 画子实现
void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color) {
    if (!parent) return;

    // 创建棋子对象 (基本图形)
    lv_obj_t * stone = lv_obj_create(parent);
    lv_obj_set_size(stone, STONE_SIZE, STONE_SIZE);
    lv_obj_set_style_radius(stone, LV_RADIUS_CIRCLE, 0); // 圆形
    lv_obj_set_style_border_width(stone, 0, 0);          // 无边框
    
    // 设置颜色
    if (color == COLOR_BLACK) {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0x000000), 0); // 黑
    } else {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0xFFFFFF), 0); // 白
    }

    // 计算像素坐标
    // 公式: 内边距 + 网格坐标 * 格子大小 - 棋子半径(居中)
    int pos_x = BOARD_PADDING + x * CELL_SIZE - STONE_SIZE / 2;
    int pos_y = BOARD_PADDING + y * CELL_SIZE - STONE_SIZE / 2;
    
    // 移动棋子
    lv_obj_set_pos(stone, pos_x, pos_y);
    lv_obj_clear_flag(stone, LV_OBJ_FLAG_SCROLLABLE); // 禁止滚动
}

// 清盘实现
void board_view_clear(lv_obj_t *parent) {
    if (!parent) return;
    // LVGL 函数：清除对象的所有子对象
    lv_obj_clean(parent);
}

// 坐标转换实现
int board_view_get_click_xy(lv_obj_t *board_obj, int *out_x, int *out_y) {
    if (!board_obj) return 0;

    // 1. 获取输入设备(鼠标/触摸)的绝对坐标
    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    // 2. 获取棋盘容器的绝对坐标区域
    lv_area_t coords;
    lv_obj_get_coords(board_obj, &coords);

    // 3. 计算相对坐标 (点击点 - 容器左上角)
    int rel_x = p.x - coords.x1;
    int rel_y = p.y - coords.y1;

    // 4. 网格吸附算法
    // (相对位置 - 边距 + 半个格子) / 格子大小
    // "+ CELL_SIZE/2" 是为了实现四舍五入的吸附效果
    int gx = (rel_x - BOARD_PADDING + CELL_SIZE / 2) / CELL_SIZE;
    int gy = (rel_y - BOARD_PADDING + CELL_SIZE / 2) / CELL_SIZE;

    // 5. 边界检查
    if (gx >= 0 && gx < BOARD_SIZE && gy >= 0 && gy < BOARD_SIZE) {
        *out_x = gx;
        *out_y = gy;
        return 1; // 有效
    }
    
    return 0; // 无效
}