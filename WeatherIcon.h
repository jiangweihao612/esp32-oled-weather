#ifndef __WEATHER_ICON_H
#define __WEATHER_ICON_H

#include <stdint.h>

/* 32x32 单色天气图标，每个图标 32*32/8 = 128 字节 */
#define WEATHER_ICON_W      32
#define WEATHER_ICON_H      32
#define WEATHER_ICON_BYTES  (WEATHER_ICON_W * WEATHER_ICON_H / 8)

/* 图标编号 */
#define ICON_SUNNY      0   /* 晴 */
#define ICON_PARTLY     1   /* 多云 */
#define ICON_CLOUDY     2   /* 阴 */
#define ICON_RAIN       3   /* 雨 */
#define ICON_THUNDER    4   /* 雷雨 */
#define ICON_SNOW       5   /* 雪 */
#define ICON_FOG        6   /* 雾 */
#define ICON_DUST       7   /* 浮尘/沙尘 */
#define ICON_WIND       8   /* 大风 */
#define ICON_COUNT      9

/* 图标数据表，用 WEATHER_ICONS[ICON_XXX] 取用 */
extern const uint8_t* const WEATHER_ICONS[ICON_COUNT];

#endif
