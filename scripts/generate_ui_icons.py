#!/usr/bin/env python3
"""
生成 CubePaw OS 专用高精度超椭圆 (Squircle) 应用图标集 (供 ESP32-S3 LVGL 固件与预览使用)
遵循 Stageshow craft.md 规范：
- 严禁使用粗糙 Emoji 充当 UI 图标
- 统一视觉权重、圆润几何边缘、Filled 实心主体搭配一致的描边
- 嵌套内外圆角对齐 (Outer Radius = Inner Radius + Padding)
"""
import os
from PIL import Image, ImageDraw

OUT_DIR = '/Users/rebuild/Documents/nico/ai_project/CubePaw/assets/ui_icons'
os.makedirs(OUT_DIR, exist_ok=True)

# 4x 超采样抗锯齿绘制 (绘制在 192x192 上，缩放到 48x48)
SS = 4
SIZE = 48 * SS
INK = (38, 45, 92, 255)
WHITE = (255, 255, 255, 255)

def new_tile(bg_top, bg_bot):
    im = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)
    # 绘制圆角背景卡片与顶部高光
    draw.rounded_rectangle([4, 4, SIZE-5, SIZE-5], radius=14*SS, fill=bg_bot, outline=INK, width=2*SS)
    draw.rounded_rectangle([4, 4, SIZE-5, SIZE-14], radius=14*SS, fill=bg_top, outline=INK, width=2*SS)
    return im, draw

def save_icon(im, name):
    out = im.resize((48, 48), Image.Resampling.LANCZOS)
    out.save(os.path.join(OUT_DIR, f'{name}.png'), format='PNG', optimize=True, icc_profile=None)

def build_icons():
    # 1. AI 对话图标 (圆润麦克风 + 声波对话气泡)
    im, d = new_tile((255, 118, 102, 255), (235, 82, 66, 255))
    # 聊天气泡主体
    d.rounded_rectangle([38, 36, 154, 128], radius=36, fill=WHITE, outline=INK, width=6)
    d.polygon([(64, 124), (50, 150), (90, 124)], fill=WHITE)
    d.line([(62, 125), (50, 150), (88, 125)], fill=INK, width=6, joint='curve')
    # 气泡内三条圆润声波柱
    for x, h in [(74, 24), (96, 44), (118, 28)]:
        d.rounded_rectangle([x-6, 82-h//2, x+6, 82+h//2], radius=6, fill=(255, 105, 88, 255))
    save_icon(im, 'icon_ai_chat')

    # 2. 音乐盒图标 (饱满双八分音符连梁)
    im, d = new_tile((72, 198, 255, 255), (36, 158, 228, 255))
    # 左右音符符头
    d.ellipse([42, 104, 86, 142], fill=WHITE, outline=INK, width=6)
    d.ellipse([104, 92, 148, 130], fill=WHITE, outline=INK, width=6)
    # 符干与顶部斜连梁
    d.polygon([(74, 120), (74, 52), (146, 38), (146, 108), (132, 108), (132, 62), (88, 70), (88, 120)],
              fill=WHITE, outline=INK, width=6)
    save_icon(im, 'icon_music')

    # 3. 小玉相册图标 (山峦风景与暖阳相框)
    im, d = new_tile((255, 196, 77, 255), (242, 158, 38, 255))
    d.rounded_rectangle([34, 38, 158, 148], radius=22, fill=WHITE, outline=INK, width=6)
    # 太阳圆点
    d.ellipse([54, 56, 82, 84], fill=(255, 122, 89, 255))
    # 双层山峦剪影
    d.polygon([(44, 138), (82, 92), (118, 138)], fill=(92, 184, 255, 255))
    d.polygon([(82, 138), (118, 78), (150, 138)], fill=INK)
    save_icon(im, 'icon_gallery')

    # 4. 4合1 游戏厅图标 (圆润掌机手柄 + 十字键 + AB键)
    im, d = new_tile((88, 226, 142, 255), (46, 188, 104, 255))
    d.rounded_rectangle([28, 52, 164, 138], radius=38, fill=WHITE, outline=INK, width=6)
    # 左侧十字方向键 (D-Pad)
    d.rounded_rectangle([58, 74, 72, 116], radius=4, fill=INK)
    d.rounded_rectangle([44, 88, 86, 102], radius=4, fill=INK)
    # 右侧 AB 双彩色按键
    d.ellipse([124, 72, 144, 92], fill=(255, 105, 97, 255), outline=INK, width=4)
    d.ellipse([104, 94, 124, 114], fill=(72, 198, 255, 255), outline=INK, width=4)
    save_icon(im, 'icon_games')

    # 5. 时钟表盘图标 (精密圆形钟面 + 10点10分指针)
    im, d = new_tile((196, 122, 255, 255), (156, 82, 224, 255))
    d.ellipse([36, 36, 156, 156], fill=WHITE, outline=INK, width=6)
    # 12/3/6/9 刻度点
    for cx, cy in [(96, 48), (144, 96), (96, 144), (48, 96)]:
        d.ellipse([cx-4, cy-4, cx+4, cy+4], fill=INK)
    # 时针与分针
    d.line([(96, 96), (72, 76)], fill=INK, width=8)
    d.line([(96, 96), (124, 68)], fill=(255, 105, 97, 255), width=7)
    d.ellipse([88, 88, 104, 104], fill=INK)
    save_icon(im, 'icon_clock')

    # 6. 网络与设置图标 (精密圆润齿轮 + Wi-Fi 信号弧)
    im, d = new_tile((144, 164, 184, 255), (104, 124, 144, 255))
    import math
    # 8 齿标准齿轮：外圈齿顶半径 62，齿根半径 46，每齿占 22.5°
    pts = []
    teeth = 8
    for i in range(teeth * 4):
        ang = math.radians(i * 360 / (teeth * 4) - 90)
        r = 62 if (i % 4) in (1, 2) else 46
        pts.append((96 + r * math.cos(ang), 96 + r * math.sin(ang)))
    d.polygon(pts, fill=WHITE, outline=INK, width=6)
    d.ellipse([74, 74, 118, 118], fill=(104, 124, 144, 255), outline=INK, width=6)
    save_icon(im, 'icon_settings')

    print(f'Successfully generated 6 crisp Squircle icons in {OUT_DIR}')

if __name__ == '__main__':
    build_icons()
