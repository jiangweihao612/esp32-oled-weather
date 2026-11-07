# Offscreen mock of the 128x64 OLED screen, built from the real font/icon data
# in OLED_Data.cpp and WeatherIcon.cpp, so the layout can be checked on the PC.
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)

W, H = 128, 64


def load_arrays(path):
    with io.open(path, encoding="utf-8") as f:
        return f.read()


def hex_bytes(block):
    return [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", block)]


src = load_arrays(os.path.join(PROJ, "OLED_Data.cpp"))

# ASCII 8x16
i = src.index("OLED_F8x16[][16]")
block = src[i:src.index("};", i)]
raw = hex_bytes(block)
assert len(raw) == 95 * 16, len(raw)
F8x16 = [raw[k * 16:(k + 1) * 16] for k in range(95)]

# ASCII 6x8
i = src.index("OLED_F6x8[][6]")
block = src[i:src.index("};", i)]
raw = hex_bytes(block)
assert len(raw) == 95 * 6, len(raw)
F6x8 = [raw[k * 6:(k + 1) * 6] for k in range(95)]

# Chinese 16x16
CN = {}
for m in re.finditer(r'"([^"]{1,4})",\s*((?:0x[0-9A-Fa-f]{2},?\s*){32})', src):
    CN[m.group(1)] = hex_bytes(m.group(2))
print("chinese glyphs:", len(CN))

# Icons
isrc = load_arrays(os.path.join(PROJ, "WeatherIcon.cpp"))
ICONS = {}
for m in re.finditer(r"WeatherIcon_(\w+)\[WEATHER_ICON_BYTES\] = \{(.*?)};", isrc, re.S):
    ICONS[m.group(1)] = hex_bytes(m.group(2))
print("icons:", sorted(ICONS))

ICON_ORDER = ["Sunny", "Partly", "Cloudy", "Rain", "Thunder", "Snow", "Fog", "Dust", "Wind"]

screen = [[0] * W for _ in range(H)]


def blend(x, y, data, w, h):
    pages = h // 8
    for col in range(w):
        for p in range(pages):
            b = data[p * w + col]
            for bit in range(8):
                if b & (1 << bit):
                    yy, xx = y + p * 8 + bit, x + col
                    if 0 <= xx < W and 0 <= yy < H:
                        screen[yy][xx] = 1
    return (x, y, w, h)


def text_w(s, font):
    w = 0
    for ch in s:
        w += font if ord(ch) < 128 else (16 if font == 8 else 6)
    return w


def draw_text(x, y, s, font):
    for ch in s:
        if ord(ch) < 128:
            idx = ord(ch) - 32
            if 0 <= idx < 95:
                blend(x, y, F8x16[idx] if font == 8 else F6x8[idx], font, font * 2)
            x += font
        else:
            data = CN.get(ch)
            if data is not None and font == 8:
                blend(x, y, data, 16, 16)
            x += 16 if font == 8 else 6
    return x


def draw_right(right_x, y, s, font):
    draw_text(right_x - text_w(s, font) + 1, y, s, font)


def draw_screen(date, week, time, region, city_zh, temp, text_zh, icon):
    for y in range(H):
        for x in range(W):
            screen[y][x] = 0
    # 第1行：日期 + 星期
    draw_text(0, 0, "%s %s" % (date, week), 8)
    # 第2行：时间（左） + 图标（右，x=96）
    draw_text(0, 16, time, 8)
    blend(96, 16, ICONS[ICON_ORDER[icon]], 32, 32)
    # 第3行：地区 城市（直辖市区为空，只显示城市名）
    if region:
        draw_text(0, 32, "%s %s" % (region, city_zh), 8)
    else:
        draw_text(0, 32, city_zh, 8)
    # 第4行：天气中文（左） + 温度（右）
    draw_text(0, 48, text_zh, 8)
    draw_right(127, 48, "%dC" % temp, 8)


def save(path, scale=4):
    # write a 24-bit BMP by hand (no third party libs available here)
    w, h = W * scale, H * scale
    row_size = (w * 3 + 3) // 4 * 4
    pad = row_size - w * 3
    data = bytearray()
    for y in range(h - 1, -1, -1):
        sy = y // scale
        row = bytearray()
        for x in range(w):
            v = 0 if screen[sy][x // scale] else 255
            row.extend([v, v, v])
        row.extend(b"\x00" * pad)
        data.extend(row)
    header = bytearray()
    header.extend(b"BM")
    header.extend((54 + len(data)).to_bytes(4, "little"))
    header.extend((0).to_bytes(4, "little"))
    header.extend((54).to_bytes(4, "little"))
    header.extend((40).to_bytes(4, "little"))
    header.extend(w.to_bytes(4, "little"))
    header.extend(h.to_bytes(4, "little"))
    header.extend((1).to_bytes(2, "little"))
    header.extend((24).to_bytes(2, "little"))
    header.extend((0).to_bytes(4, "little"))
    header.extend(len(data).to_bytes(4, "little"))
    header.extend((2835).to_bytes(4, "little"))
    header.extend((2835).to_bytes(4, "little"))
    header.extend((0).to_bytes(4, "little"))
    header.extend((0).to_bytes(4, "little"))
    with open(path, "wb") as f:
        f.write(header)
        f.write(data)


# (日期, 星期, 时间, 地区, 城市, 温度, 天气中文, 图标)
CASES = [
    ("2026-11-07", "周六", "21:30:12", "", "北京", 2, "雨夹雪", 5),
    ("2026-11-07", "周六", "21:30:12", "", "上海", 17, "阴", 2),
    ("2026-11-07", "周六", "21:30:12", "", "天津", 3, "小雨", 3),
    ("2026-11-07", "周六", "21:30:12", "广东", "广州", 24, "小雨", 3),
    ("2026-11-07", "周六", "21:30:12", "四川", "成都", 15, "阴", 2),
    ("2026-11-07", "周六", "21:30:12", "陕西", "西安", 10, "小雨", 3),
    ("2026-11-07", "周六", "21:30:12", "浙江", "杭州", 15, "小雨", 3),
    ("2026-11-07", "周六", "21:30:12", "湖北", "武汉", 15, "小雨", 3),
    ("2026-11-07", "周六", "21:30:12", "江苏", "南京", 14, "小雨", 3),
]

for idx, c in enumerate(CASES):
    draw_screen(*c)
    save(os.path.join(HERE, "mock_%d.bmp" % idx))


def draw_loading(l1, l2, l3):
    for y in range(H):
        for x in range(W):
            screen[y][x] = 0
    for i, s in enumerate((l1, l2, l3)):
        if s:
            draw_text(0, 8 + i * 16, s, 8)


# 加载/错误提示页：三行全部 8x16，y=8/24/40
LOADING = [
    ("Loading...", "Guangzhou", "Seniverse API"),
    ("WiFi FAIL", "check SSID/PASS", "in Config.h"),
    ("Fetch Fail", "Nanjing", "auto retry 60s"),
]
for i, (a, b, c) in enumerate(LOADING):
    draw_loading(a, b, c)
    save(os.path.join(HERE, "mock_loading_%d.bmp" % i))
    for s in (a, b, c):
        if text_w(s, 8) > W:
            print("OVERFLOW:", s, text_w(s, 8))

print("done")
