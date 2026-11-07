#ifndef __CONFIG_H
#define __CONFIG_H

/* ===================================================================
 *  这是配置模板。用法：
 *    复制本文件为 Config.h，然后把下面三处改成你自己的信息。
 *    Config.h 已在 .gitignore 里，不会被提交到仓库。
 *      Windows:  copy Config.example.h Config.h
 *      macOS/Linux: cp Config.example.h Config.h
 * =================================================================== */

/* 1. WiFi 名称和密码
 *    注意：ESP32 只支持 2.4GHz 频段，连不上 5G 频段的热点
 */
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASS       "your_wifi_password"

/* 2. 心知天气 API 私钥
 *    注册并登录 https://www.seniverse.com/  →  控制台  →  添加 API 密钥
 *    免费版每天有调用次数限制，够个人使用
 */
#define SENIVERSE_KEY   "your_seniverse_private_key"

/* ===================================================================
 *  下面的参数一般不用改
 * =================================================================== */

/* 天气自动刷新间隔（毫秒）。免费版每天免费额度有限，不建议小于 10 分钟 */
#define REFRESH_INTERVAL_MS   (10 * 60 * 1000UL)

/* 刷新失败后的重试间隔（毫秒） */
#define RETRY_INTERVAL_MS     (60 * 1000UL)

/* WiFi 连接超时（毫秒） */
#define WIFI_TIMEOUT_MS       (15000UL)

/* 按键：一端接 GPIO23，另一端接 GND（按下为低电平） */
#define BUTTON_PIN            23

/* 长按判定时间（毫秒）：短按切换城市，长按刷新当前城市 */
#define BUTTON_LONG_MS        1200UL

#endif
