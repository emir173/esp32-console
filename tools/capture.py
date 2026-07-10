#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
============================================================
  E-OS USB Screenshot -- Tek Kare PNG Kaydi (Auto-Color)
============================================================
  ESP32-S3'ten seri porttan tek bir ekran goruntusu alir ve
  PNG olarak kaydeder. Renk modu header'dan otomatik algilanir.

  Kullanim:
    python capture.py --port COM9
    python capture.py               (otomatik port tespiti)

  Wire format:
    SHOT:BGR_SWAP\n + 40960 byte  (yeni format, auto-color)
    SHOT:E-OS\n     + 40960 byte  (eski format, default BGR_SWAP)
============================================================
"""

import sys
import os
import time
import argparse
from datetime import datetime

# Windows konsolunda Turkce karakter destegi
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

# Shared module
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from eos_capture import (
    WIDTH, HEIGHT, FRAME_BYTES,
    find_next_header, rgb565_to_bgr, upscale_to_out,
    FrameCache, extract_frame,
    auto_detect_port, open_serial, read_chunk,
)
from PIL import Image


def main():
    parser = argparse.ArgumentParser(
        description='E-OS USB Screenshot -- Tek Kare PNG (Auto-Color)')
    parser.add_argument('--port', type=str, default=None,
                        help='Seri port (orn: COM9). Bos = otomatik tespit.')
    parser.add_argument('--baud', type=int, default=921600,
                        help='Baud hizi (varsayilan: 921600)')
    parser.add_argument('--out', type=str, default=None,
                        help='Cikti dosya yolu (bos = otomatik timestamp)')
    # Geri uyumlu renk flag'lari (eski firmware icin fallback)
    parser.add_argument('--no-swap-bytes', action='store_true',
                        help='Eski format icin byte swap KAPALI (doom/direkt fb)')
    parser.add_argument('--rgb565', action='store_true',
                        help='Eski format icin RGB565 (TFT_* sabitleri)')
    parser.add_argument('--scale', type=int, default=1,
                        help='Tamsayi buyutme carpanı (orn: 4 -> 640x512). 1 = Orjinal.')
    parser.add_argument('--smooth', action='store_true',
                        help='Buyuturken pikselleri yumusat (LANCZOS). Konsol ekranini taklit eder.')
    args = parser.parse_args()

    # Geri uyumlu default renk modu (eski SHOT:E-OS formati icin)
    default_swap = not args.no_swap_bytes
    default_bgr  = not args.rgb565

    # --- Cikti dosya yolu ---
    script_dir  = os.path.dirname(os.path.abspath(__file__))
    parent_dir  = os.path.dirname(script_dir)
    capture_dir = os.path.join(parent_dir, "captures")
    os.makedirs(capture_dir, exist_ok=True)

    if args.out:
        out_path = args.out if os.path.isabs(args.out) else os.path.join(capture_dir, args.out)
    else:
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = os.path.join(capture_dir, f"screenshot_{ts}.png")

    # --- Seri port baglan ---
    port_name = args.port or auto_detect_port()
    if not port_name:
        print("[HATA] Seri port bulunamadi. --port parametresi ile belirtin.")
        sys.exit(1)

    print(f"[..] Baglaniliyor -> {port_name} @ {args.baud} baud...")
    try:
        ser = open_serial(port_name, args.baud, timeout=2.0)
    except Exception as e:
        print(f"[HATA] {port_name} acilamadi!")
        print(f"       Arduino IDE Serial Monitor'un KAPALI oldugundan emin olun.")
        print(f"       Detay: {e}")
        sys.exit(1)

    time.sleep(0.5)
    ser.reset_input_buffer()
    print("[..] Cihaza 's' (screenshot) komutu gonderiliyor...")

    ser.write(b"s")
    ser.flush()

    # --- SHOT header + 40960 byte bekle ---
    buf = bytearray()
    cache = FrameCache()
    deadline = time.time() + 15.0  # 15 saniye zaman asimi (ESP32 harita yuklemesi uzun surebilir)
    captured = False
    last_s_time = time.time()

    print("[..] Veri bekleniyor (buyuk chunk okuma)...")

    while time.time() < deadline:
        # Eger cihaz harita yukluyorsa ilk 's' komutu silinmis olabilir, her 1 saniyede bir tekrar gonder
        if time.time() - last_s_time > 1.0:
            ser.write(b"s")
            ser.flush()
            last_s_time = time.time()

        chunk = read_chunk(ser)  # max(in_waiting, 65536) -- usbser.sys fix
        if chunk:
            buf.extend(chunk)

        # SHOT header ara (auto-color + auto-resolution + delta modu parse)
        result = find_next_header(buf, default_swap, default_bgr)
        if result is None:
            continue

        # FULL/DELTA decode (SHOT hep FULL; delta cache'i gunceller)
        dec = extract_frame(buf, result, cache)
        if dec is None:
            continue  # payload tam degil, daha fazla veri bekle

        consumed_end, bgr_img, is_gif, swap, bgr, fw, fh = dec
        del buf[:consumed_end]

        if is_gif or bgr_img is None:
            # FRAME (video) veya decode edilemeyen delta -> atla
            continue

        # SHOT decoded -> PNG kaydet
        # PIL icin BGR -> RGB
        rgb_img = bgr_img[..., ::-1]  # numpy kanal ters cevir (sifir kopya)
        img = Image.fromarray(rgb_img)

        # capture -> display (160x128) upscale
        img = upscale_to_out(img, fw, fh, fw * args.scale, fh * args.scale, smooth=args.smooth)

        img.save(out_path)
        captured = True

        mode_name = "BGR_SWAP" if (swap and bgr) else \
                    "RGB_SWAP" if (swap and not bgr) else \
                    "BGR_NOSWAP" if (not swap and bgr) else "CUSTOM"
        print(f"[OK] Ekran goruntusu kaydedildi: {out_path}")
        print(f"     Renk modu (auto): {mode_name} | swap={swap} | bgr565={bgr}")
        upscale_type = "LANCZOS" if args.smooth else "NEAREST"
        print(f"     Capture: {fw}x{fh} -> {fw * args.scale}x{fh * args.scale} ({upscale_type} upscale)")
        break

    if not captured:
        print("[HATA] Zaman asimi. Cihazdan yanit alinamadi.")

    ser.close()


if __name__ == '__main__':
    main()
