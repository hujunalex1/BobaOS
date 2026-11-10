#!/usr/bin/env python3
import os
import math
from PIL import Image, ImageDraw, ImageFilter

SRC_IMG = '/Users/rebuild/.gemini/antigravity/brain/540f6890-e93b-463c-b618-64371ecee7eb/.user_uploaded/media_1794121101529_e1695028.jpg'
OUT_ROOT = '/Users/rebuild/Documents/nico/ai_project/CubePaw/assets/xiaoyu_emojis'

# 截图中的 21 宫格顺序（3行 x 7列）
EMOJI_GRID_ORDER = [
    # Row 1
    ('01_默认_neutral', 'neutral', '默认'),
    ('02_开心_happy', 'happy', '开心'),
    ('03_大笑_laughing', 'laughing', '大笑'),
    ('04_搞笑_funny', 'funny', '搞笑'),
    ('05_伤心_sad', 'sad', '伤心'),
    ('06_生气_angry', 'angry', '生气'),
    ('07_哭泣_crying', 'crying', '哭泣'),
    # Row 2
    ('08_喜爱_loving', 'loving', '喜爱'),
    ('09_尴尬_embarrassed', 'embarrassed', '尴尬'),
    ('10_惊讶_surprised', 'surprised', '惊讶'),
    ('11_震惊_shocked', 'shocked', '震惊'),
    ('12_思考_thinking', 'thinking', '思考'),
    ('13_眨眼_winking', 'winking', '眨眼'),
    ('14_酷炫_cool', 'cool', '酷炫'),
    # Row 3
    ('15_放松_relaxed', 'relaxed', '放松'),
    ('16_美味_delicious', 'delicious', '美味'),
    ('17_飞吻_kissy', 'kissy', '飞吻'),
    ('18_自信_confident', 'confident', '自信'),
    ('19_困倦_sleepy', 'sleepy', '困倦'),
    ('20_调皮_silly', 'silly', '调皮'),
    ('21_困惑_confused', 'confused', '困惑'),
]

# 手绘深蓝勾线色 & 肤色 & 腮红色
INK = (38, 45, 92, 255)
SKIN = (242, 196, 156, 255)
BLUSH = (238, 128, 108, 180)
RED_MOUTH = (218, 78, 78, 255)
PINK_TONGUE = (245, 138, 148, 255)
TEAR_BLUE = (95, 185, 245, 235)
HEART_RED = (238, 72, 96, 255)
STAR_GOLD = (255, 200, 35, 255)

def clean_base_portrait(size=200):
    raw = Image.open(SRC_IMG).convert('RGBA')
    # 截取上半身 338x338 正方形
    crop = raw.crop((0, 0, 338, 338)).resize((size, size), Image.Resampling.LANCZOS)
    # 修复右上角原图黑边残影 (x: 162..200, y: 0..10)，用左侧黄色背景自然覆盖
    for y in range(12):
        for x in range(160, size):
            src_px = crop.getpixel((size - 1 - x + 15, y + 8))
            crop.putpixel((x, y), src_px)
    return crop

def cover_original_mouth(draw):
    # 抹除原本的小横线嘴巴 (x: 92..106, y: 111..117)
    draw.ellipse([91, 110, 107, 118], fill=SKIN)

def cover_left_eye(draw):
    # 将左眼区域 (46..82, 65..100) 覆盖为肤色，用于画闭眼/眨眼
    draw.ellipse([47, 66, 81, 99], fill=SKIN)

def cover_right_eye(draw):
    # 将右眼区域 (115..150, 68..102) 覆盖为肤色，用于画闭眼/眨眼
    draw.ellipse([116, 69, 149, 102], fill=SKIN)

def draw_heart(draw, cx, cy, r, fill=HEART_RED, outline=INK):
    draw.ellipse([cx - r, cy - int(r*0.8), cx + int(r*0.1), cy + int(r*0.2)], fill=fill, outline=outline, width=2)
    draw.ellipse([cx - int(r*0.1), cy - int(r*0.8), cx + r, cy + int(r*0.2)], fill=fill, outline=outline, width=2)
    draw.polygon([(cx - int(r*0.95), cy - int(r*0.1)), (cx + int(r*0.95), cy - int(r*0.1)), (cx, cy + int(r*1.1))], fill=fill)
    draw.line([(cx - int(r*0.95), cy - int(r*0.1)), (cx, cy + int(r*1.1)), (cx + int(r*0.95), cy - int(r*0.1))], fill=outline, width=2)

def draw_star(draw, cx, cy, r, fill=STAR_GOLD, outline=INK):
    pts = [
        (cx, cy - r), (cx + int(r*0.3), cy - int(r*0.3)),
        (cx + r, cy), (cx + int(r*0.3), cy + int(r*0.3)),
        (cx, cy + r), (cx - int(r*0.3), cy + int(r*0.3)),
        (cx - r, cy), (cx - int(r*0.3), cy - int(r*0.3))
    ]
    draw.polygon(pts, fill=fill, outline=outline)

def render_emotion(base_im, key):
    im = base_im.copy()
    draw = ImageDraw.Draw(im)

    if key == 'neutral':
        # 1. 默认：原汁原味的小玉
        pass

    elif key == 'happy':
        # 2. 开心：温暖微笑弧线 + 脸颊旁小闪星
        cover_original_mouth(draw)
        draw.arc([88, 104, 110, 119], start=15, end=165, fill=INK, width=3)
        draw_star(draw, 22, 42, 11)
        draw_star(draw, 178, 38, 9)

    elif key == 'laughing':
        # 3. 大笑：弯弯笑眼 + 张大开心笑嘴 + 音符火花
        cover_left_eye(draw)
        cover_right_eye(draw)
        draw.arc([50, 76, 78, 98], start=195, end=345, fill=INK, width=4)
        draw.arc([119, 79, 147, 101], start=195, end=345, fill=INK, width=4)
        cover_original_mouth(draw)
        draw.chord([87, 104, 111, 122], start=0, end=180, fill=RED_MOUTH, outline=INK, width=3)
        draw.chord([92, 113, 106, 121], start=0, end=180, fill=PINK_TONGUE)

    elif key == 'funny':
        # 4. 搞笑：大小眼搞怪 + 歪嘴笑 + 搞笑十字星
        draw.ellipse([58, 76, 70, 88], fill=(255, 255, 255, 255))
        draw.ellipse([61, 79, 67, 85], fill=INK)
        cover_original_mouth(draw)
        draw.arc([86, 105, 114, 120], start=0, end=155, fill=INK, width=3)
        draw.chord([98, 112, 110, 124], start=0, end=180, fill=PINK_TONGUE, outline=INK, width=2)
        draw_star(draw, 24, 32, 12)

    elif key == 'sad':
        # 5. 伤心：八字委屈眉 + 向下撇嘴 + 眼角泪珠
        draw.line([(48, 60), (74, 52)], fill=INK, width=3)
        draw.line([(122, 54), (148, 63)], fill=INK, width=3)
        cover_original_mouth(draw)
        draw.arc([89, 112, 109, 125], start=195, end=345, fill=INK, width=3)
        draw.ellipse([48, 92, 57, 104], fill=TEAR_BLUE, outline=INK, width=2)

    elif key == 'angry':
        # 6. 生气：倒八字怒眉 + 撅嘴 + 额头手绘十字青筋符号
        draw.line([(50, 52), (80, 64)], fill=INK, width=4)
        draw.line([(146, 54), (116, 66)], fill=INK, width=4)
        cover_original_mouth(draw)
        draw.arc([88, 111, 110, 124], start=200, end=340, fill=INK, width=3)
        # 动漫生气十字青筋 (右上额头)
        cx, cy = 152, 42
        draw.arc([cx-12, cy-12, cx-2, cy-2], start=0, end=90, fill=HEART_RED, width=3)
        draw.arc([cx+2, cy-12, cx+12, cy-2], start=90, end=180, fill=HEART_RED, width=3)
        draw.arc([cx+2, cy+2, cx+12, cy+12], start=180, end=270, fill=HEART_RED, width=3)
        draw.arc([cx-12, cy+2, cx-2, cy+12], start=270, end=360, fill=HEART_RED, width=3)

    elif key == 'crying':
        # 7. 哭泣：瀑布泪水 + 哇哇哭嘴
        cover_original_mouth(draw)
        draw.rounded_rectangle([89, 109, 109, 121], radius=5, fill=RED_MOUTH, outline=INK, width=3)
        # 两条晶莹眼泪流下
        draw.polygon([(54, 96), (68, 96), (65, 132), (51, 132)], fill=TEAR_BLUE, outline=INK)
        draw.polygon([(128, 99), (142, 99), (145, 134), (131, 134)], fill=TEAR_BLUE, outline=INK)

    elif key == 'loving':
        # 8. 喜爱：眼中闪烁小爱心 + 周围漂浮红心 + 甜美微笑
        cover_original_mouth(draw)
        draw.arc([88, 104, 110, 119], start=15, end=165, fill=INK, width=3)
        draw_heart(draw, 64, 82, 9)
        draw_heart(draw, 133, 85, 9)
        draw_heart(draw, 24, 44, 12)
        draw_heart(draw, 176, 38, 11)

    elif key == 'embarrassed':
        # 9. 尴尬：额头大汗珠 + 脸颊红晕加深 + 波浪小嘴
        draw.ellipse([38, 88, 72, 112], fill=(240, 110, 100, 130))
        draw.ellipse([122, 90, 156, 114], fill=(240, 110, 100, 130))
        cover_original_mouth(draw)
        draw.arc([87, 110, 99, 118], start=180, end=360, fill=INK, width=3)
        draw.arc([97, 110, 109, 118], start=0, end=180, fill=INK, width=3)
        # 大汗滴 (右上)
        draw.polygon([(164, 24), (154, 46), (174, 46)], fill=TEAR_BLUE, outline=INK)
        draw.chord([154, 38, 174, 56], start=0, end=180, fill=TEAR_BLUE, outline=INK, width=2)

    elif key == 'surprised':
        # 10. 惊讶：O型小圆嘴 + 挑眉 + 惊叹号
        draw.arc([48, 48, 78, 64], start=190, end=350, fill=INK, width=3)
        draw.arc([118, 50, 148, 66], start=190, end=350, fill=INK, width=3)
        cover_original_mouth(draw)
        draw.ellipse([93, 108, 105, 121], fill=RED_MOUTH, outline=INK, width=3)
        # 手绘感叹号
        draw.rounded_rectangle([172, 18, 180, 42], radius=3, fill=HEART_RED, outline=INK, width=2)
        draw.ellipse([172, 47, 180, 55], fill=HEART_RED, outline=INK, width=2)

    elif key == 'shocked':
        # 11. 震惊：瞳孔缩小高光 + 大张嘴 + 闪电震波线
        draw.ellipse([54, 72, 74, 92], fill=(255, 255, 255, 255), outline=INK, width=2)
        draw.ellipse([61, 79, 67, 85], fill=INK)
        draw.ellipse([123, 75, 143, 95], fill=(255, 255, 255, 255), outline=INK, width=2)
        draw.ellipse([130, 82, 136, 88], fill=INK)
        cover_original_mouth(draw)
        draw.ellipse([90, 106, 108, 124], fill=RED_MOUTH, outline=INK, width=3)
        draw.line([(16, 22), (28, 36), (20, 38), (32, 52)], fill=INK, width=3)
        draw.line([(182, 22), (170, 36), (178, 38), (166, 52)], fill=INK, width=3)

    elif key == 'thinking':
        # 12. 思考：云朵思考泡泡 + 大问号 + 嘟嘴思考
        cover_original_mouth(draw)
        draw.ellipse([95, 111, 103, 118], fill=INK)
        # 思考泡泡
        draw.ellipse([142, 46, 150, 54], fill=(255, 255, 255, 255), outline=INK, width=2)
        draw.ellipse([152, 34, 163, 45], fill=(255, 255, 255, 255), outline=INK, width=2)
        draw.ellipse([160, 6, 196, 38], fill=(255, 255, 255, 255), outline=INK, width=3)
        # 手绘问号
        draw.arc([171, 12, 185, 24], start=180, end=45, fill=HEART_RED, width=3)
        draw.line([(178, 23), (178, 27)], fill=HEART_RED, width=3)
        draw.ellipse([176, 30, 180, 34], fill=HEART_RED)

    elif key == 'winking':
        # 13. 眨眼：右眼俏皮眨眼 (<) + 微笑 + 小星星
        cover_right_eye(draw)
        draw.line([(120, 78), (138, 86), (120, 94)], fill=INK, width=4, joint='curve')
        cover_original_mouth(draw)
        draw.arc([88, 104, 110, 119], start=15, end=165, fill=INK, width=3)
        draw_star(draw, 174, 38, 11)

    elif key == 'cool':
        # 14. 酷炫：潮流黑墨镜 + 高光反光条 + 酷酷自信嘴角
        draw.rounded_rectangle([42, 68, 88, 98], radius=8, fill=(25, 28, 38, 250), outline=INK, width=3)
        draw.rounded_rectangle([110, 70, 156, 100], radius=8, fill=(25, 28, 38, 250), outline=INK, width=3)
        draw.line([(88, 80), (110, 81)], fill=INK, width=5)
        draw.line([(36, 78), (42, 80)], fill=INK, width=4)
        draw.line([(156, 82), (162, 80)], fill=INK, width=4)
        # 墨镜白斜线高光
        draw.line([(50, 90), (64, 74)], fill=(255, 255, 255, 210), width=3)
        draw.line([(118, 92), (132, 76)], fill=(255, 255, 255, 210), width=3)
        cover_original_mouth(draw)
        draw.arc([88, 104, 112, 118], start=10, end=140, fill=INK, width=3)

    elif key == 'relaxed':
        # 15. 放松：双眼温柔闭合弧线 + 惬意微笑 + 小音符
        cover_left_eye(draw)
        cover_right_eye(draw)
        draw.arc([49, 74, 79, 94], start=15, end=165, fill=INK, width=4)
        draw.arc([118, 77, 148, 97], start=15, end=165, fill=INK, width=4)
        cover_original_mouth(draw)
        draw.arc([89, 105, 109, 118], start=15, end=165, fill=INK, width=3)
        # 音符
        draw.ellipse([166, 36, 176, 44], fill=HEART_RED, outline=INK, width=2)
        draw.line([(175, 40), (175, 20), (186, 24)], fill=INK, width=3)

    elif key == 'delicious':
        # 16. 美味：微笑舔舌头 + 闪亮星星
        cover_original_mouth(draw)
        draw.arc([86, 104, 112, 119], start=10, end=170, fill=INK, width=3)
        draw.chord([98, 109, 113, 123], start=340, end=200, fill=PINK_TONGUE, outline=INK, width=2)
        draw_star(draw, 22, 42, 10)
        draw_star(draw, 176, 40, 10)

    elif key == 'kissy':
        # 17. 飞吻：右眼眨眼 + 3字嘟嘟飞吻嘴 + 飞出的大红心
        cover_right_eye(draw)
        draw.arc([119, 77, 147, 97], start=195, end=345, fill=INK, width=4)
        cover_original_mouth(draw)
        draw.arc([94, 107, 104, 115], start=270, end=110, fill=HEART_RED, width=3)
        draw.arc([94, 113, 104, 121], start=250, end=90, fill=HEART_RED, width=3)
        draw_heart(draw, 132, 108, 12)
        draw_heart(draw, 168, 88, 9)

    elif key == 'confident':
        # 18. 自信：扬眉 + 阳光大微笑 + 闪耀星芒
        draw.arc([48, 52, 80, 66], start=190, end=350, fill=INK, width=3)
        draw.arc([116, 54, 148, 68], start=190, end=350, fill=INK, width=3)
        cover_original_mouth(draw)
        draw.chord([87, 105, 111, 120], start=0, end=180, fill=(255, 255, 255, 255), outline=INK, width=3)
        draw_star(draw, 174, 34, 13)

    elif key == 'sleepy':
        # 19. 困倦：闭眼安睡 + 鼻涕泡/睡眠泡泡 + Zzz
        cover_left_eye(draw)
        cover_right_eye(draw)
        draw.arc([49, 76, 79, 94], start=15, end=165, fill=INK, width=4)
        draw.arc([118, 79, 148, 97], start=15, end=165, fill=INK, width=4)
        # Zzz 手绘大字
        for x0, y0, sz in [(142, 42, 10), (156, 26, 13), (174, 8, 16)]:
            draw.line([(x0, y0), (x0+sz, y0), (x0, y0+sz), (x0+sz, y0+sz)], fill=INK, width=3)

    elif key == 'silly':
        # 20. 调皮：左眼眨眼 + 大吐舌头搞怪
        cover_left_eye(draw)
        draw.line([(52, 76), (72, 84), (52, 92)], fill=INK, width=4)
        cover_original_mouth(draw)
        draw.line([(87, 112), (111, 112)], fill=INK, width=3)
        draw.rounded_rectangle([92, 112, 106, 127], radius=6, fill=PINK_TONGUE, outline=INK, width=3)
        draw.line([(99, 113), (99, 122)], fill=INK, width=2)

    elif key == 'confused':
        # 21. 困惑：一高一低疑惑眉 + 斜线小嘴 + 两个问号
        draw.arc([48, 48, 78, 62], start=190, end=350, fill=INK, width=3)
        draw.line([(118, 62), (146, 58)], fill=INK, width=3)
        cover_original_mouth(draw)
        draw.line([(90, 116), (108, 111)], fill=INK, width=3)
        # 两个手绘问号
        for qx, qy in [(22, 24), (168, 20)]:
            draw.arc([qx, qy, qx+14, qy+12], start=180, end=45, fill=INK, width=3)
            draw.line([(qx+7, qy+11), (qx+7, qy+16)], fill=INK, width=3)
            draw.ellipse([qx+5, qy+19, qx+9, qy+23], fill=INK)

    return im

def main():
    dir_ordered = os.path.join(OUT_ROOT, '01_按控制台顺序直接拖入_21张')
    dir_en = os.path.join(OUT_ROOT, '02_标准英文名_21张')
    os.makedirs(dir_ordered, exist_ok=True)
    os.makedirs(dir_en, exist_ok=True)

    base_200 = clean_base_portrait(200)

    # 创建一张 7列 x 3行 的预览大图 (每格 200x200)
    sheet = Image.new('RGBA', (7 * 200, 3 * 200), (255, 255, 255, 255))

    for idx, (ordered_name, en_key, cn_label) in enumerate(EMOJI_GRID_ORDER):
        im_200 = render_emotion(base_200, en_key)
        # 控制台通常推荐 120x120 或 160x160 (不超过 240x240)
        im_160 = im_200.resize((160, 160), Image.Resampling.LANCZOS)

        # 保存不带 icc_profile 的纯净 PNG
        im_160.save(os.path.join(dir_ordered, f'{ordered_name}.png'), format='PNG', optimize=True, icc_profile=None)
        im_160.save(os.path.join(dir_en, f'{en_key}.png'), format='PNG', optimize=True, icc_profile=None)

        r = idx // 7
        c = idx % 7
        sheet.paste(im_200, (c * 200, r * 200))

    sheet_path = os.path.join(OUT_ROOT, '小玉21表情全家福预览.png')
    sheet.save(sheet_path, format='PNG', optimize=True)
    print(f'Successfully generated 21 Xiaoyu emojis in {OUT_ROOT}!')
    print(f'Preview sheet saved at {sheet_path}')

if __name__ == '__main__':
    main()
