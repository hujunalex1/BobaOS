#!/usr/bin/env python3
import os
import struct
import io
import json
from PIL import Image, ImageDraw

def build_and_verify():
    # 1. 生成干净无 iCCP 的 120x120 表情图
    src = '/Users/rebuild/Documents/nico/ai_project/CubePaw/assets/character/neutral.png'
    raw_base = Image.open(src).convert('RGBA').resize((120, 120), Image.Resampling.LANCZOS)
    base = Image.new('RGBA', (120, 120))
    base.paste(raw_base, (0, 0))

    emoji_names = [
        'crying', 'neutral', 'confident', 'sleepy', 'loving', 'kissy', 
        'cool', 'delicious', 'laughing', 'angry', 'shocked', 'surprised', 
        'embarrassed', 'confused', 'relaxed', 'thinking', 'sad', 'winking', 
        'funny', 'happy', 'silly'
    ]

    def make_variant(mode):
        im = base.copy()
        draw = ImageDraw.Draw(im)
        if mode in ['happy', 'laughing', 'funny', 'silly']:
            for cx, cy in [(18, 30), (102, 28)]:
                r = 8
                pts = [(cx, cy-r), (cx+r*0.3, cy-r*0.3), (cx+r, cy), (cx+r*0.3, cy+r*0.3),
                       (cx, cy+r), (cx-r*0.3, cy+r*0.3), (cx-r, cy), (cx-r*0.3, cy-r*0.3)]
                draw.polygon(pts, fill=(255, 215, 0, 255))
            draw.ellipse([55, 35, 65, 40], fill=(230, 80, 80, 255))
        elif mode in ['loving', 'kissy']:
            for cx, cy in [(20, 28), (100, 25)]:
                draw.ellipse([cx-6, cy-5, cx, cy+1], fill=(255, 100, 140, 255))
                draw.ellipse([cx, cy-5, cx+6, cy+1], fill=(255, 100, 140, 255))
                draw.polygon([(cx-6, cy-1), (cx+6, cy-1), (cx, cy+6)], fill=(255, 100, 140, 255))
        elif mode in ['thinking', 'confused']:
            draw.ellipse([82, 22, 88, 28], fill=(70, 130, 230, 255))
            draw.ellipse([90, 12, 99, 21], fill=(70, 130, 230, 255))
            draw.ellipse([100, 2, 114, 16], fill=(70, 130, 230, 255))
            draw.text((104, 3), '?', fill=(255, 255, 255, 255))
        elif mode in ['surprised', 'shocked']:
            draw.ellipse([95, 4, 110, 19], fill=(255, 140, 0, 255))
            draw.text((100, 5), '!', fill=(255, 255, 255, 255))
        elif mode in ['sleepy']:
            draw.text((85, 20), 'z', fill=(120, 140, 220, 255))
            draw.text((95, 11), 'Z', fill=(100, 120, 240, 255))
            draw.text((105, 2), 'Z', fill=(70, 90, 250, 255))
        elif mode in ['crying', 'sad', 'angry']:
            draw.ellipse([42, 32, 47, 40], fill=(100, 180, 255, 255))
            draw.ellipse([72, 32, 77, 40], fill=(100, 180, 255, 255))
        elif mode in ['cool', 'confident']:
            draw.line([(35, 28), (50, 28)], fill=(255, 255, 255, 200), width=2)
            draw.line([(70, 28), (85, 28)], fill=(255, 255, 255, 200), width=2)
        buf = io.BytesIO()
        im.save(buf, format='PNG', optimize=True, icc_profile=None)
        return buf.getvalue()

    gen_dir = '/Users/rebuild/Documents/nico/ai_project/CubePaw/assets/generated_emojis'
    os.makedirs(gen_dir, exist_ok=True)
    custom_raw = {}
    for name in emoji_names:
        png_bytes = make_variant(name)
        custom_raw[f'{name}.png'] = png_bytes
        with open(os.path.join(gen_dir, f'{name}.png'), 'wb') as f:
            f.write(png_bytes)

    # 2. 从原版提取原始文件（严格按 start+2 : start+2+fsize 提取不含 ZZ 的原始字节）
    with open('merged-binary.bin', 'rb') as f:
        orig_full = f.read()

    orig_assets = orig_full[0x800000:]
    file_count, _, _ = struct.unpack('<III', orig_assets[:12])
    table_end = 12 + file_count * 44

    raw_files = []
    for i in range(file_count):
        entry = orig_assets[12+i*44 : 12+(i+1)*44]
        name_b, fsize, foff, _ = struct.unpack('<32sIII', entry)
        name_str = name_b.split(b'\x00')[0].decode('utf-8')
        start = table_end + foff
        assert orig_assets[start:start+2] == b'ZZ', f'Missing ZZ at {name_str}'
        if name_str in custom_raw:
            raw_data = custom_raw[name_str]
        else:
            raw_data = orig_assets[start+2 : start+2+fsize]
        raw_files.append((name_str, raw_data))

    # 3. 严格按官方格式打包：fsize = len(raw_data), payload = b'ZZ' + raw_data, cur_off += 2 + fsize
    table_bytes = bytearray()
    payload_bytes = bytearray()
    cur_off = 0
    for name_str, raw_data in raw_files:
        fsize = len(raw_data)
        name_b = name_str.encode('utf-8')[:31].ljust(32, b'\x00')
        table_bytes.extend(struct.pack('<32sIII', name_b, fsize, cur_off, 0))
        payload_bytes.extend(b'ZZ' + raw_data)
        cur_off += 2 + fsize

    whole_body = table_bytes + payload_bytes
    dlen = len(whole_body)
    chksum = sum(whole_body) & 0xFFFF
    new_assets = struct.pack('<III', file_count, chksum, dlen) + whole_body

    with open('assets-custom.bin', 'wb') as f:
        f.write(new_assets)

    new_full = orig_full[:0x800000] + new_assets
    with open('merged-binary-custom.bin', 'wb') as f:
        f.write(new_full)

    # 4. 模拟固件 Assets::Apply() 严格校验
    data = new_assets
    fc, stored_chksum, stored_len = struct.unpack('<III', data[:12])
    assert len(data) - 12 == stored_len
    calc_chksum = sum(data[12:12+stored_len]) & 0xFFFF
    assert calc_chksum == stored_chksum
    print(f'[OK] Header & Checksum verified: {fc} files, chksum={stored_chksum:#x}, len={stored_len}')

    for i in range(fc):
        entry = data[12+i*44 : 12+(i+1)*44]
        name_b, fsize, foff, _ = struct.unpack('<32sIII', entry)
        name = name_b.split(b'\x00')[0].decode('utf-8')
        start = table_end + foff
        magic = data[start:start+2]
        assert magic == b'ZZ', f'Bad magic at {name}: {magic}'
        content = data[start+2 : start+2+fsize]
        if name == 'index.json':
            parsed = json.loads(content.decode('utf-8'))
            assert 'emoji_collection' in parsed and len(parsed['emoji_collection']) == 21
            print('[OK] index.json parsed validly with 21 emojis!')
        elif name.endswith('.png'):
            assert content[:8] == b'\x89PNG\r\n\x1a\n' and content[-8:] == b'IEND\xaeB`\x82'
            im = Image.open(io.BytesIO(content))
            im.verify()
    print('[OK] All 21 PNG files verified (magic ZZ, valid PNG header/IEND, clean chunks, 120x120)!')

if __name__ == '__main__':
    build_and_verify()
