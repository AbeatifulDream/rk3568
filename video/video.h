#ifndef __VIDEO_H__
#define __VIDEO_H__

#include "lvgl/lvgl.h"

/**
 * @brief 创建视频模块的界面
 *
 * 同时创建 mplayer 控制管道(FIFO)
 *
 * @param parent 父对象(主页面 page_main)
 */
void video_create(lv_obj_t* parent);

/**
 * @brief 进入视频模块
 *
 * 隐藏主界面，显示视频网格模式
 */
void video_show(void);

#endif /* __VIDEO_H__ */
