#ifndef __MAIN_H__
#define __MAIN_H__

#include "lvgl/lvgl.h"

/**
 * @brief 创建整个应用程序的界面
 *
 * 包含登录页面、主页面以及相册/视频/游戏/网络四个功能模块的界面
 */
void myinterface(void);

/**
 * @brief 隐藏主页面(背景 + 四个功能图标)
 *
 * 各个功能模块进入时调用（由模块自己的 xxx_show() 调用）
 */
void app_main_page_hide(void);

/**
 * @brief 显示主页面(背景 + 四个功能图标)
 *
 * 各个功能模块点击返回按钮时调用，用于切回主界面
 */
void app_main_page_show(void);

#endif /* __MAIN_H__ */
