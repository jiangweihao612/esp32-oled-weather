#ifndef __WEATHER_H
#define __WEATHER_H

#include <Arduino.h>
#include <stdint.h>

/* 一个城市的信息 */
typedef struct
{
	const char *nameZh;		/* 中文城市名（需在 OLED 字库中有字模） */
	const char *nameEn;		/* 英文城市名（ASCII） */
	const char *region;		/* 地区/省份（需在 OLED 字库中有字模） */
	const char *location;	/* 心知天气 location 参数：纬度:经度 */
} CityInfo;

/* 一次天气查询的结果 */
typedef struct
{
	bool		valid;		/* 数据是否有效 */
	int16_t		temperature;/* 温度，单位 ℃ */
	char		updated[8];	/* 数据更新时间 HH:MM（从接口 last_update 提取） */
	uint8_t		icon;		/* 图标编号，见 WeatherIcon.h */
	const char *textZh;		/* 中文天气（取自内置码表，保证字库里有字模） */
} WeatherData;

/* 城市列表，按按键依次切换。新增城市只需在 Weather.cpp 的 CITIES[] 加一行，
   CITY_COUNT 会根据数组长度自动计算，无需手动修改。 */
extern const CityInfo CITIES[];
extern const int CITY_COUNT;

/* 连接 WiFi（阻塞，带超时），成功返回 true */
bool wifiConnect(void);

/* 查询指定城市当前天气，成功返回 true */
bool weatherFetch(const CityInfo *city, WeatherData *data);

#endif
