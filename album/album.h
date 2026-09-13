#ifndef __ALBUM_H__
#define __ALBUM_H__

#include "lvgl/lvgl.h"

/**
 * @brief 创建相册模块的界面
 *
 * @param parent 父对象(主页面 page_main)
 */
void album_create(lv_obj_t* parent);

/**
 * @brief 进入相册模块
 *
 * 隐藏主界面，显示相册网格模式
 */
void album_show(void);

#endif /* __ALBUM_H__ */
