#include "board_view.h"
#include <stdio.h>

// 画子
void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color) {
    if (!parent) return;

    lv_obj_t * stone = lv_obj_create(parent);
    lv_obj_set_size(stone, STONE_SIZE, STONE_SIZE);
    lv_obj_set_style_radius(stone, LV_RADIUS_CIRCLE, 0); 
    lv_obj_set_style_border_width(stone, 0, 0);          
    
    if (color == COLOR_BLACK) {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0x000000), 0);
    } else {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0xFFFFFF), 0);
    }

    // 计算坐标
    int pos_x = BOARD_PADDING + x * CELL_SIZE - STONE_SIZE / 2;
    int pos_y = BOARD_PADDING + y * CELL_SIZE - STONE_SIZE / 2;
    
    lv_obj_set_pos(stone, pos_x, pos_y);
    lv_obj_clear_flag(stone, LV_OBJ_FLAG_SCROLLABLE); 
}

// 清盘
void board_view_clear(lv_obj_t *parent) {
    if (!parent) return;
    lv_obj_clean(parent);
}

// 在你的坐标转换函数中修改
bool board_view_get_click_xy(lv_obj_t * obj, int * x_out, int * y_out) {
    lv_indev_t * indev = lv_indev_get_act();
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    // 1. 转换到容器内坐标
    int local_x = point.x - 180; 
    int local_y = point.y - 20;

    // 2. 减去边距，计算相对于第一条线的偏移
    int grid_x = local_x - BOARD_PADDING;
    int grid_y = local_y - BOARD_PADDING;

    // 3. 使用相同的 CELL_SIZE (22) 进行换算
    int gx = (int)((grid_x / (float)CELL_SIZE) + 0.5f);
    int gy = (int)((grid_y / (float)CELL_SIZE) + 0.5f);

    if (gx < 0 || gx >= BOARD_SIZE || gy < 0 || gy >= BOARD_SIZE) return false;

    *x_out = gx;
    *y_out = gy;
    return true;
}

// 在 board_view.c 中新增：绘制棋盘网格
void board_view_draw_grid(lv_obj_t *parent) {
    if (!parent) return;
    
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 1);
    lv_style_set_line_color(&style_line, lv_color_hex(0x000000)); // 黑线

    for (int i = 0; i < BOARD_SIZE; i++) {
        // 画横线
        lv_obj_t * h_line = lv_line_create(parent);
        static lv_point_t h_points[2];
        h_points[0].x = BOARD_PADDING;
        h_points[0].y = BOARD_PADDING + i * CELL_SIZE;
        h_points[1].x = BOARD_PADDING + (BOARD_SIZE-1) * CELL_SIZE;
        h_points[1].y = BOARD_PADDING + i * CELL_SIZE;
        lv_line_set_points(h_line, h_points, 2);
        lv_obj_add_style(h_line, &style_line, 0);

        // 画竖线
        lv_obj_t * v_line = lv_line_create(parent);
        static lv_point_t v_points[2];
        v_points[0].x = BOARD_PADDING + i * CELL_SIZE;
        v_points[0].y = BOARD_PADDING;
        v_points[1].x = BOARD_PADDING + i * CELL_SIZE;
        v_points[1].y = BOARD_PADDING + (BOARD_SIZE-1) * CELL_SIZE;
        lv_line_set_points(v_line, v_points, 2);
        lv_obj_add_style(v_line, &style_line, 0);
    }
}