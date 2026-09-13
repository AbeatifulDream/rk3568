#ifndef __GAME_H__
#define __GAME_H__

#include "lvgl/lvgl.h"

/**
 * @brief 创建井字棋游戏模块的界面
 *
 * @param parent 父对象(主页面 page_main)
 */
void game_create(lv_obj_t* parent);

/**
 * @brief 进入游戏模块
 *
 * 隐藏主界面，显示游戏页面并重新开始一局
 */
void game_show(void);

#endif /* __GAME_H__ */
