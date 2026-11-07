/***************************************************************************************
  * ESP32 + 0.96寸 OLED（I2C: SCL=GPIO22, SDA=GPIO21）心知天气显示屏
  *
  * 功能：
  *   1. 通过心知天气 API 获取当前天气
  *   2. OLED 显示：日期星期、时间、天气图标、地区城市、天气中文、温度
  *   3. 按键（GPIO23 接 GND）：短按切换城市，长按立即刷新当前城市
  *   4. 默认城市：北京；每 10 分钟自动刷新一次
  *
  * 使用前请先在 Config.h 里填写 WiFi 名称/密码和心知天气私钥
  ***************************************************************************************/

#include "OLED.h"
#include "Weather.h"
#include "WeatherIcon.h"
#include "Config.h"
#include <time.h>	/* NTP 对时：configTime / getLocalTime / struct tm */

/* 当前选中的城市（0 = 北京） */
static uint8_t     cityIndex = 0;
/* 当前屏幕上显示的数据 */
static WeatherData weather;
/* 每个城市各自缓存上一次成功取到的数据，切城市时先显示缓存（即时响应） */
static WeatherData gCache[64];	/* 每城市一份缓存，64 足够覆盖所有城市 */
/* 下次自动刷新的时间点 */
static uint32_t    nextRefreshMs = 0;

/* 按键消抖用变量 */
static bool     btnLastRaw  = false;
static bool     btnStable   = false;
static uint32_t btnChangeMs = 0;
static uint32_t btnPressMs  = 0;
static bool     btnLongDone = false;

/* ------------------------------------------------------------------ 显示 */

/* 按右边界对齐显示一串 ASCII 字符 */
static void showRight(int16_t rightX, int16_t y, const char *s, uint8_t font)
{
	int16_t w = 0;
	for (const char *p = s; *p != '\0'; p++)
	{
		w += font;
	}
	OLED_ShowString(rightX - w, y, (char *)s, font);
}

/* 全屏提示信息：三行统一用最大的 8x16 字体（驱动只有 8x16 / 6x8 两种字号），
 * 纵向居中排布，上下各留 8 像素。
 * 注意：8x16 每字符宽 8 像素，一行最多 16 个 ASCII 字符，超了会被屏幕边缘裁掉 */
static void drawMessage(const char *line1, const char *line2, const char *line3)
{
	OLED_Clear();
	OLED_ShowString(0, 8, (char *)line1, OLED_8X16);
	if (line2 != NULL)
	{
		OLED_ShowString(0, 24, (char *)line2, OLED_8X16);
	}
	if (line3 != NULL)
	{
		OLED_ShowString(0, 40, (char *)line3, OLED_8X16);
	}
	OLED_Update();
}

/* 取当前本地时间（NTP 同步后），分别格式化为 日期 / 时间 / 星期。
 * 未同步返回 false。星期用「周X」表示，便于在 OLED 上紧凑显示 */
static bool getDateTime(char *dateBuf, char *timeBuf, char *wkBuf, size_t n)
{
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo))
		return false;
	strftime(dateBuf, n, "%Y-%m-%d", &timeinfo);
	strftime(timeBuf, n, "%H:%M:%S", &timeinfo);
	const char *wd[7] = { "周日", "周一", "周二", "周三", "周四", "周五", "周六" };
	strncpy(wkBuf, wd[timeinfo.tm_wday], n - 1);
	wkBuf[n - 1] = '\0';
	return true;
}

/* 显示布局：
 *   +--------------------------------+
 *   | 2026-11-07 周六                 |  ← 日期 + 星期
 *   | 17:57:12              [icon]    |  ← 时间(左上) + 天气图标(右)
 *   | 湖北 武汉                       |  ← 地区 + 城市文字
 *   | 小雨                     15C    |  ← 天气中文 + 温度
 *   +--------------------------------+
 *   时间(每秒)与日期由 drawClock() 刷新：只清对应区域，不影响图标，不闪烁
 */
/* 静态主界面：第1行 年月日+星期，第2行 时分秒+图标，第3行 地区城市，第4行 天气中文+温度 */
static void drawWeatherStatic(void)
{
    const CityInfo *city = &CITIES[cityIndex];
    char buf[40];
    char dateBuf[16], timeBuf[16], wkBuf[8];
    struct tm ti;
    if (getLocalTime(&ti))
    {
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &ti);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &ti);
        const char *wd[7] = {"周日","周一","周二","周三","周四","周五","周六"};
        snprintf(wkBuf, sizeof(wkBuf), "%s", wd[ti.tm_wday]);
    }
    else
    {
        strcpy(dateBuf, "---- -- --");
        strcpy(timeBuf, "--:--:--");
        strcpy(wkBuf, "---");
    }

    OLED_Clear();

    /* 第1行：年月日 + 星期 */
    snprintf(buf, sizeof(buf), "%s %s", dateBuf, wkBuf);
    OLED_ShowString(0, 0, buf, OLED_8X16);

    /* 第2行：时分秒（左），右侧天气图标 */
    OLED_ShowString(0, 16, timeBuf, OLED_8X16);
    OLED_ShowImage(96, 16, WEATHER_ICON_W, WEATHER_ICON_H, WEATHER_ICONS[weather.icon]);

    /* 第3行：地区 城市（直辖市 region 为空，只显示城市名） */
    if (city->region[0] != '\0')
    {
        snprintf(buf, sizeof(buf), "%s %s", city->region, city->nameZh);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%s", city->nameZh);
    }
    OLED_ShowString(0, 32, buf, OLED_8X16);

    /* 第4行：天气中文（左） + 温度（右） */
    OLED_ShowString(0, 48, (char *)weather.textZh, OLED_8X16);
    snprintf(buf, sizeof(buf), "%dC", weather.temperature);
    showRight(127, 48, buf, OLED_8X16);

    OLED_Update();
}

/* 每秒走字：只刷新第1行(日期/星期)和第2行左侧(时分秒)，图标与下方不动 */
static void drawClock(void)
{
    char dateBuf[16], timeBuf[16], wkBuf[8];
    struct tm ti;
    if (getLocalTime(&ti))
    {
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &ti);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &ti);
        const char *wd[7] = {"周日","周一","周二","周三","周四","周五","周六"};
        snprintf(wkBuf, sizeof(wkBuf), "%s", wd[ti.tm_wday]);
    }
    else
    {
        strcpy(dateBuf, "---- -- --");
        strcpy(timeBuf, "--:--:--");
        strcpy(wkBuf, "---");
    }

    /* 第1行只放日期和星期，可以整行清干净再重写 */
    OLED_ClearArea(0, 0, 128, 16);
    char line[32];
    snprintf(line, sizeof(line), "%s %s", dateBuf, wkBuf);
    OLED_ShowString(0, 0, line, OLED_8X16);

    /* 第2行只清左侧 96 像素，右侧图标保持不动 */
    OLED_ClearArea(0, 16, 96, 16);
    OLED_ShowString(0, 16, timeBuf, OLED_8X16);

    OLED_Update();
}

/* ------------------------------------------------------------------ 联网取数 */

/* NTP 对时：WiFi 连上后调用一次即可。configTime 是非阻塞的，
   首次 getLocalTime 时才真正去服务器同步，之后本地时钟自行走时 */
static void initNTP(void)
{
	static bool done = false;
	if (!done)
	{
		/* 时区 UTC+8；主用阿里云 NTP，备用 pool.ntp.org / time.nist.gov */
		configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org", "time.nist.gov");
		done = true;
		Serial.println("[NTP] 已启动对时，等待同步...");
	}
}

static void refreshWeather(void)
{
	if (!wifiConnect())
	{
		drawMessage("WiFi FAIL", "check SSID/PASS", "in Config.h");
		nextRefreshMs = millis() + RETRY_INTERVAL_MS;
		return;
	}

	initNTP();	/* WiFi 已连，启动网络对时 */

	drawMessage("Loading...", CITIES[cityIndex].nameEn, "Seniverse API");

	WeatherData tmp;
	if (weatherFetch(&CITIES[cityIndex], &tmp))
	{
		gCache[cityIndex] = tmp;
		weather = tmp;
		drawWeatherStatic();
		nextRefreshMs = millis() + REFRESH_INTERVAL_MS;
	}
	else if (gCache[cityIndex].valid)
	{
		/* 取数失败，但本城市有缓存，显示缓存并稍后重试 */
		weather = gCache[cityIndex];
		drawWeatherStatic();
		nextRefreshMs = millis() + RETRY_INTERVAL_MS;
	}
	else if (weather.valid)
	{
		/* 本城市无缓存、但全局曾经取数成功：继续显示旧数据，城市名已切到新城市 */
		drawWeatherStatic();
		nextRefreshMs = millis() + RETRY_INTERVAL_MS;
	}
	else
	{
		drawMessage("Fetch Fail", CITIES[cityIndex].nameEn, "auto retry 60s");
		nextRefreshMs = millis() + RETRY_INTERVAL_MS;
	}
}

/* ------------------------------------------------------------------ 按键 */

static void pollButton(void)
{
	bool     raw = (digitalRead(BUTTON_PIN) == LOW);	/* 按下为低电平 */
	uint32_t now = millis();

	/* 先做 25ms 消抖，得到稳定状态 */
	if (raw != btnLastRaw)
	{
		btnLastRaw  = raw;
		btnChangeMs = now;
	}

	if (now - btnChangeMs >= 25 && raw != btnStable)
	{
		btnStable = raw;

		if (btnStable)
		{
			/* 按下：记录时间，等松开或长按 */
			btnPressMs  = now;
			btnLongDone = false;
		}
		else if (!btnLongDone)
		{
			/* 松开且未触发长按 → 短按：切换到下一个城市 */
			cityIndex = (uint8_t)((cityIndex + 1) % CITY_COUNT);
			/* 若该城市有缓存，先立刻显示出来（切城市即时响应，不用等联网） */
			if (gCache[cityIndex].valid)
			{
				weather = gCache[cityIndex];
				drawWeatherStatic();
			}
			refreshWeather();
		}
	}

	/* 长按：刷新当前城市 */
	if (btnStable && !btnLongDone && (now - btnPressMs >= BUTTON_LONG_MS))
	{
		btnLongDone = true;
		refreshWeather();
	}
}

/* ------------------------------------------------------------------ 主流程 */

void setup(void)
{
	Serial.begin(115200);
	OLED_Init();
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	drawMessage("Weather", "0.96 OLED", "starting...");
	delay(600);

	refreshWeather();
}

void loop(void)
{
	pollButton();

	/* 时钟走字：NTP 同步成功后，每秒刷新底部日期/星期/时间（只刷下半屏，不闪） */
	static int  lastSecond = -1;
	static bool ntpSynced  = false;
	struct tm ti;
	if (weather.valid && getLocalTime(&ti))
	{
		if (!ntpSynced)
		{
			ntpSynced = true;
			Serial.printf("[NTP] 同步成功，本地时间 %02d:%02d:%02d\r\n",
			              ti.tm_hour, ti.tm_min, ti.tm_sec);
		}
		if (ti.tm_sec != lastSecond)
		{
			lastSecond = ti.tm_sec;
			drawClock();
		}
	}

	/* 到点自动刷新（用差值判断，避免 millis 溢出问题） */
	if ((int32_t)(millis() - nextRefreshMs) >= 0)
	{
		refreshWeather();
	}
}
