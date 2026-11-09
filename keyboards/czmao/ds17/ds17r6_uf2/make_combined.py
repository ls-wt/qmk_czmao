#!/usr/bin/env python3
"""
合并 uf2boot 引导程序 + QMK 应用，产出 DS17R6_UF2 的整包固件。

内存布局（与 uf2boot 一致）:
  0x08000000 - 0x08003FFF  uf2boot 引导程序 (16KB)
  0x08004000 - 0x0801FFFF  应用 (APM32F103CBT6 = 128KB flash)

依赖（与本脚本同目录）:
  bootloader/uf2boot_czm_apm32f103.hex   引导程序 (Intel HEX，APM32F103 专用)
  czmao_ds17_ds17r6_uf2_via.uf2          应用 (UF2，基址 0x08004000，取最新构建)

产出（同目录）:
  czmao_ds17r6_uf2_combined.bin  (128KB 原始镜像，ST-Link/SWD @0x08000000 烧录)
  czmao_ds17r6_uf2_combined.uf2  (合并 UF2，拖进已有 uf2boot 的 MSC 盘双写)
  czmao_ds17r6_uf2_combined.hex  (Intel HEX，给吃 HEX 的烧录器)
"""
import struct
import os

BOOT_ADDR = 0x08000000
APP_ADDR  = 0x08004000
FLASH_TOTAL = 128 * 1024          # APM32F103CBT6
BOOT_MAX = APP_ADDR - BOOT_ADDR   # 16KB
STM32_FAMILY = 0x5EE21072         # UF2 family id (STM32/APM32)

HERE = os.path.dirname(os.path.abspath(__file__))
BOOT_HEX = os.path.join(HERE, "bootloader", "uf2boot_czm_apm32f103.hex")
APP_UF2  = os.path.join(HERE, "czmao_ds17_ds17r6_uf2_via.uf2")
OUT_BIN  = os.path.join(HERE, "czmao_ds17r6_uf2_combined.bin")
OUT_UF2  = os.path.join(HERE, "czmao_ds17r6_uf2_combined.uf2")
OUT_HEX  = os.path.join(HERE, "czmao_ds17r6_uf2_combined.hex")


def parse_hex(path):
    base = 0
    data = {}
    minaddr = None
    maxaddr = 0
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line.startswith(":"):
                continue
            n = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            typ = int(line[7:9], 16)
            payload = bytes.fromhex(line[9:9 + n * 2])
            if typ == 0:
                a = (base + addr) & 0xFFFFFFFF
                for i, b in enumerate(payload):
                    data[a + i] = b
                if minaddr is None or a < minaddr:
                    minaddr = a
                if a + n > maxaddr:
                    maxaddr = a + n
            elif typ == 2:
                base = int.from_bytes(payload[0:2], "big") << 4
            elif typ == 4:
                base = int.from_bytes(payload[0:2], "big") << 16
            elif typ == 1:
                break
    if minaddr is None:
        raise SystemExit("ERROR: HEX 文件为空或无法解析: " + path)
    buf = bytearray(maxaddr - minaddr)
    for a in range(minaddr, maxaddr):
        buf[a - minaddr] = data.get(a, 0)
    return minaddr, bytes(buf)


def parse_uf2(path):
    blocks = []
    fam = None
    with open(path, "rb") as f:
        blob = f.read()
    i = 0
    while i + 512 <= len(blob):
        blk = blob[i:i + 512]
        magic0, magic1, flags, addr, size, blockno, nblocks, family = struct.unpack("<IIIIIIII", blk[:32])
        if magic0 != 0x0A324655 or magic1 != 0x9E5D5157:
            i += 512
            continue
        chunk = blk[32:32 + size]
        if flags & 0x2000:
            fam = family
        blocks.append((addr, chunk))
        i += 512
    if not blocks:
        raise SystemExit("ERROR: UF2 文件无有效块: " + path)
    blocks.sort()
    minaddr = blocks[0][0]
    maxaddr = max(a + len(c) for a, c in blocks)
    buf = bytearray(maxaddr - minaddr)
    for a, c in blocks:
        buf[a - minaddr:a + len(c)] = c
    return minaddr, bytes(buf), (fam if fam else STM32_FAMILY)


def bin2uf2(data, base, fam, out):
    chunk = 256
    nblocks = (len(data) + chunk - 1) // chunk
    with open(out, "wb") as f:
        for b in range(nblocks):
            off = b * chunk
            c = data[off:off + chunk]
            if len(c) < chunk:
                c = c + b"\x00" * (chunk - len(c))
            hdr = struct.pack("<IIIIIIII", 0x0A324655, 0x9E5D5157, 0x2000, base + off, chunk, b, nblocks, fam)
            hdr += c
            hdr += b"\x00" * (476 - chunk)
            hdr += struct.pack("<I", 0x0AB16F30)
            f.write(hdr)
    print("  合并 UF2 -> %s (family=0x%08X, blocks=%d)" % (out, fam, nblocks))


def bin2hex(data, base, out):
    def checksum(b):
        return (~sum(b) + 1) & 0xFF

    def emit_ext_lin(upper):
        b = [0x02, 0x00, 0x00, 0x04, (upper >> 8) & 0xFF, upper & 0xFF]
        return ":" + "".join("%02X" % x for x in b + [checksum(b)]) + "\n"

    def emit_data(addr, chunk):
        b = [len(chunk), (addr >> 8) & 0xFF, addr & 0xFF, 0x00] + list(chunk)
        return ":" + "".join("%02X" % x for x in b + [checksum(b)]) + "\n"

    lines = []
    cur_upper = None
    i = 0
    while i < len(data):
        full = base + i
        upper = (full >> 16) & 0xFFFF
        if upper != cur_upper:
            lines.append(emit_ext_lin(upper))
            cur_upper = upper
        chunk = min(16, len(data) - i, 0x10000 - (full & 0xFFFF))
        lines.append(emit_data(full & 0xFFFF, data[i:i + chunk]))
        i += chunk
    lines.append(":00000001FF\n")
    with open(out, "w") as f:
        f.writelines(lines)
    print("  合并 HEX -> %s (%d 行)" % (out, len(lines)))


def main():
    bl_min, bl = parse_hex(BOOT_HEX)
    app_min, app, fam = parse_uf2(APP_UF2)

    if bl_min != BOOT_ADDR:
        raise SystemExit("ERROR: 引导程序基址 %#010X != 期望 %#010X" % (bl_min, BOOT_ADDR))
    if len(bl) > BOOT_MAX:
        raise SystemExit("ERROR: 引导程序 %d 字节 > 16KB 上限" % len(bl))
    if app_min != APP_ADDR:
        raise SystemExit("ERROR: 应用基址 %#010X != 期望 %#010X" % (app_min, APP_ADDR))
    if len(app) > FLASH_TOTAL - BOOT_MAX:
        raise SystemExit("ERROR: 应用 %d 字节超出剩余空间" % len(app))

    img = bytearray([0xFF] * FLASH_TOTAL)
    img[0:len(bl)] = bl
    img[BOOT_MAX:BOOT_MAX + len(app)] = app

    with open(OUT_BIN, "wb") as f:
        f.write(img)
    bin2uf2(img, BOOT_ADDR, fam, OUT_UF2)
    bin2hex(img, BOOT_ADDR, OUT_HEX)

    print("合并完成: %s" % OUT_BIN)
    print("  引导程序: %s (%d 字节 @0x%08X)" % (os.path.basename(BOOT_HEX), len(bl), BOOT_ADDR))
    print("  应用    : %s (%d 字节 @0x%08X)" % (os.path.basename(APP_UF2), len(app), APP_ADDR))
    print("  整包镜像: %d 字节 (%dKB)" % (len(img), len(img) // 1024))
    print("  应用占用: %d/%d 字节 (%.1f%%)" % (len(app), FLASH_TOTAL - BOOT_MAX, 100 * len(app) / (FLASH_TOTAL - BOOT_MAX)))


if __name__ == "__main__":
    main()
