#include "Weather.h"
#include "WeatherIcon.h"
#include "Config.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string.h>

/* ===================================================================
 *  城市列表：默认第 0 个（北京），按一下按键切到下一个
 *  location 用「纬度:经度」，这样不依赖心知的城市 ID，最稳
 *  坐标填市政府/市中心，别填行政区划的边缘 —— 接口会把坐标吸附到最近的
 *  区县气象站，填偏了会取到隔壁县市的数据（屏幕上却照样显示你写的城市名）
 *  region 留空表示直辖市，屏幕上只显示城市名
 *  想加城市：在下面的 CITIES[] 里加一行即可，CITY_COUNT 会自动计算；
 *  中文名里的字必须在 OLED 字库里有字模（可在 tools/cn_chars.txt 里加字后重新生成）
 * =================================================================== */
const CityInfo CITIES[] = {
	{ "北京", "Beijing",   "",     "39.91:116.40" },	/* 0 默认：北京（直辖市） */
	{ "上海", "Shanghai",  "",     "31.23:121.47" },	/* 直辖市 */
	{ "天津", "Tianjin",   "",     "39.08:117.20" },	/* 直辖市 */
	{ "广州", "Guangzhou", "广东", "23.13:113.26" },
	{ "成都", "Chengdu",   "四川", "30.66:104.07" },
	{ "西安", "Xi'an",     "陕西", "34.34:108.94" },
	{ "杭州", "Hangzhou",  "浙江", "30.27:120.16" },
	{ "武汉", "Wuhan",     "湖北", "30.59:114.31" },
	{ "南京", "Nanjing",   "江苏", "32.06:118.80" },
};

/* 城市数量：根据上面数组的长度自动计算 */
const int CITY_COUNT = (int)(sizeof(CITIES) / sizeof(CITIES[0]));

/* WiFi 连接，带超时；已连接时直接返回 true */
bool wifiConnect(void)
{
	if (WiFi.status() == WL_CONNECTED)
	{
		return true;
	}

	Serial.printf("[WiFi] 正在连接 %s ...\r\n", WIFI_SSID);
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);			/* 关闭省电，避免长时间挂机后掉线 */
	WiFi.disconnect();
	WiFi.begin(WIFI_SSID, WIFI_PASS);

	uint32_t start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS)
	{
		delay(200);
	}

	if (WiFi.status() == WL_CONNECTED)
	{
		Serial.printf("[WiFi] 连接成功，IP：%s\r\n", WiFi.localIP().toString().c_str());
		return true;
	}

	Serial.println("[WiFi] 连接超时");
	return false;
}

/* 从 JSON 里取出 "key":"value" 或 "key":value 的值，不依赖第三方库 */
static bool jsonField(const String &src, const char *key, String &out)
{
	String pattern = String("\"") + key + "\":";
	int i = src.indexOf(pattern);
	if (i < 0)
	{
		return false;
	}
	i += pattern.length();
	while (i < (int)src.length() && src[i] == ' ')
	{
		i++;
	}
	if (i >= (int)src.length())
	{
		return false;
	}

	if (src[i] == '"')					/* 字符串值 */
	{
		i++;
		int j = src.indexOf('"', i);
		if (j < 0)
		{
			return false;
		}
		out = src.substring(i, j);
		return out.length() > 0;
	}

	int j = i;							/* 数字值 */
	while (j < (int)src.length() && src[j] != ',' && src[j] != '}')
	{
		j++;
	}
	out = src.substring(i, j);
	out.trim();
	return out.length() > 0;
}

/* 天气代码 → 中文/图标
 * 中文取自这张码表而不是直接用接口返回的文字，
 * 这样保证每个字都能在 OLED 字库里找到字模，不会出现方框问号
 */
static void mapWeatherCode(int code, const char **zh, uint8_t *icon)
{
	if (code == 0)							/* 晴 */
	{
		*zh = "晴";			*icon = ICON_SUNNY;
	}
	else if (code <= 3)						/* 1~3 多云 */
	{
		*zh = "多云";		*icon = ICON_PARTLY;
	}
	else if (code == 4 || code == 9)		/* 阴 */
	{
		*zh = "阴";			*icon = ICON_CLOUDY;
	}
	else if (code <= 8)						/* 5~8 晴间多云 / 大部多云 */
	{
		*zh = "多云";		*icon = ICON_PARTLY;
	}
	else if (code == 10)					/* 阵雨 */
	{
		*zh = "阵雨";		*icon = ICON_RAIN;
	}
	else if (code == 11 || code == 12)		/* 雷阵雨 */
	{
		*zh = "雷阵雨";		*icon = ICON_THUNDER;
	}
	else if (code == 13)					/* 小雨 */
	{
		*zh = "小雨";		*icon = ICON_RAIN;
	}
	else if (code == 14)					/* 中雨 */
	{
		*zh = "中雨";		*icon = ICON_RAIN;
	}
	else if (code <= 18)					/* 15~18 大雨 / 暴雨 */
	{
		*zh = "大雨";		*icon = ICON_RAIN;
	}
	else if (code <= 21)					/* 19~21 阵雪 */
	{
		*zh = "阵雪";		*icon = ICON_SNOW;
	}
	else if (code <= 24)					/* 22~24 中雪 / 大雪 */
	{
		*zh = "中雪";		*icon = ICON_SNOW;
	}
	else if (code == 25)					/* 小雪 */
	{
		*zh = "小雪";		*icon = ICON_SNOW;
	}
	else if (code == 26)					/* 冻雨 */
	{
		*zh = "冻雨";		*icon = ICON_RAIN;
	}
	else if (code == 27)					/* 雨夹雪 */
	{
		*zh = "雨夹雪";		*icon = ICON_SNOW;
	}
	else if (code == 28 || code == 29)		/* 浮尘 / 扬沙 */
	{
		*zh = "浮尘";		*icon = ICON_DUST;
	}
	else if (code == 30 || code == 31)		/* 沙尘暴 */
	{
		*zh = "沙尘";		*icon = ICON_DUST;
	}
	else if (code == 33)					/* 霾 */
	{
		*zh = "霾";			*icon = ICON_FOG;
	}
	else if (code <= 37)					/* 32~37 雾 */
	{
		*zh = "雾";			*icon = ICON_FOG;
	}
	else if (code <= 40)					/* 38~40 大风 / 飓风 */
	{
		*zh = "大风";		*icon = ICON_WIND;
	}
	else									/* 未知 */
	{
		*zh = "未知";		*icon = ICON_CLOUDY;
	}
}

/* 查询某个城市当前天气 */
bool weatherFetch(const CityInfo *city, WeatherData *data)
{
	memset(data, 0, sizeof(WeatherData));

	if (strlen(SENIVERSE_KEY) < 10)
	{
		Serial.println("[API] 请先在 Config.h 里填写心知天气私钥 SENIVERSE_KEY");
		return false;
	}

	String url = "https://api.seniverse.com/v3/weather/now.json?key=";
	url += SENIVERSE_KEY;
	url += "&location=";
	url += city->location;
	url += "&language=zh-Hans&unit=c";

	Serial.printf("[API] GET %s\r\n", url.c_str());

	WiFiClientSecure client;
	client.setInsecure();					/* 跳过证书校验，省去烧录根证书 */

	HTTPClient http;
	http.setConnectTimeout(8000);
	http.setTimeout(8000);
	if (!http.begin(client, url))
	{
		Serial.println("[API] 请求初始化失败");
		return false;
	}

	String payload;
	int httpCode = http.GET();
	if (httpCode == HTTP_CODE_OK)
	{
		payload = http.getString();
	}
	else
	{
		Serial.printf("[API] HTTP 错误码：%d\r\n", httpCode);
		if (httpCode > 0)
		{
			Serial.println(http.getString());
		}
	}
	http.end();

	if (payload.length() == 0)
	{
		return false;
	}

	String text, codeStr, tempStr, updStr;
	bool hasCode = jsonField(payload, "code", codeStr);
	bool hasTemp = jsonField(payload, "temperature", tempStr);
	jsonField(payload, "text", text);
	jsonField(payload, "last_update", updStr);

	if (!hasCode || !hasTemp)
	{
		Serial.println("[API] 返回数据解析失败");
		Serial.println(payload);
		return false;
	}

	/* 坐标是「区县级」查询：心知会把坐标吸附到最近的区县气象站。
	 * 打印接口实际解析到的地名，方便核对 CITIES[] 里填的经纬度有没有落到隔壁县市 */
	String locName;
	if (jsonField(payload, "name", locName))
	{
		Serial.printf("[API] 坐标 %s → 解析到：%s%s\r\n",
		              city->location, locName.c_str(),
		              (locName == city->nameZh) ? "" : "  << 与城市表不符，请检查经纬度");
	}

	int code = codeStr.toInt();
	int temp = tempStr.toInt();
	mapWeatherCode(code, &data->textZh, &data->icon);
	data->temperature = (int16_t)temp;
	data->updated[0] = '\0';
	/* last_update 形如 2026-11-07T15:15:08+08:00，提取 T 后面的 HH:MM */
	int t = updStr.indexOf('T');
	if (t >= 0 && updStr.length() >= (unsigned)(t + 6))
	{
		strncpy(data->updated, updStr.c_str() + t + 1, 5);
		data->updated[5] = '\0';
	}
	data->valid = true;

	Serial.printf("[API] %s  %s  代码:%d  %dC  更新:%s\r\n",
	              city->nameEn, text.c_str(), code, temp, data->updated);
	return true;
}
