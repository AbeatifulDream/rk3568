#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "lvgl/lvgl.h"

/**
 * @brief 创建网络客户端模块的界面
 *
 * @param parent 父对象(主页面 page_main)
 */
void network_create(lv_obj_t* parent);

/**
 * @brief 进入网络客户端模块
 *
 * 隐藏主界面，复位网络页面状态并显示页面
 */
void network_show(void);

#endif /* __NETWORK_H__ */
