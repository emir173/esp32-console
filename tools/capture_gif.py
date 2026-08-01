#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
============================================================
  E-OS USB GIF Recorder -- Surekli Kayit Modu (Auto-Color)
============================================================
  ESP32-S3'ten seri porttan surekli kare alir ve GIF
  olarak birlestirir. Renk modu header'dan otomatik algilanir.

  Kayit modu: ESP32'ye 'g' = kayit baslat, 'x' = kayit durdur.
  Batch siniri YOK -> kareler arasinda atlama YOK.

  NOT: GIF formati 60 FPS'i tam desteklemez (centisecond timing).
       Gercek 60 FPS video icin record_video.py kullanin.

  Kullanim:
    python capture_gif.py --port COM9
    python capture_gif.py --frames 150 --scale 4
    python capture_gif.py --seconds 5 --scale 4
    python capture_gif.py --seconds 10 --scale 4 --out snake_gameplay.gif

  Wire format:
    FRAME:BGR_SWAP:160x128:FULL\n  + 40960 byte  (keyframe)
    FRAME:BGR_SWAP:160x128:DELTA\n + delta payload
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
        description='E-OS USB GIF Recorder -- Surekli Kayit (Auto-Color)')
    parser.add_argument('--port', type=str, default=None,
                        help='Seri port (orn: COM9). Bos = otomatik tespit.')
    parser.add_argument('--baud', type=int, default=921600,
                        help='Baud hizi (varsayilan: 921600)')
    parser.add_argument('--frames', type=int, default=0,
                        help='Toplam kare sayisi (0 = --seconds kullan)')
    parser.add_argument('--seconds', type=float, default=5.0,
                        help='Kayit suresi saniye (varsayilan: 5). --frames verilirse yok sayilir.')
    parser.add_argument('--out', type=str, default=None,
                        help='Cikti dosya yolu (bos = otomatik timestamp)')
    parser.add_argument('--scale', type=int, default=1,
                        help='Tamsayi buyutme carpani (orn: 4 -> 640x512). 1 = Orjinal.')
    parser.add_argument('--smooth', action='store_true',
                        help='Buyuturken pikselleri yumusat (LANCZOS).')
    parser.add_argument('--fps', type=int, default=0,
                        help='GIF FPS (0 = gercek alim hizini kullan)')
    # Geri uyumlu renk flag'lari (eski firmware icin fallback)
    parser.add_argument('--no-swap-bytes', action='store_true',
                        help='Eski format icin byte swap KAPALI (doom/direkt fb)')
    parser.add_argument('--rgb565', action='store_true',
                        help='Eski format icin RGB565 (TFT_* sabitleri)')
    args = parser.parse_args()

    # Kare/sure modu
    use_frame_limit = args.frames > 0
    target_frames = args.frames if use_frame_limit else 0
    target_seconds = args.seconds

    # Geri uyumlu default renk modu
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
        out_path = os.path.join(capture_dir, f"gameplay_{ts}.gif")

    # --- Seri port baglan ---
    port_name = args.port or auto_detect_port()
    if not port_name:
        print("[HATA] Seri port bulunamadi. --port parametresi ile belirtin.")
        sys.exit(1)

    print(f"[..] Baglaniliyor -> {port_name} @ {args.baud} baud...")
    try:
        ser = open_serial(port_name, args.baud, timeout=0.5)
    except Exception as e:
        print(f"[HATA] {port_name} acilamadi!")
        print(f"       Arduino IDE Serial Monitor'un KAPALI oldugundan emin olun.")
        print(f"       Detay: {e}")
        sys.exit(1)

    time.sleep(0.5)
    ser.reset_input_buffer()

    # --- Kayit baslat: 'k' (keyframe) + 'g' (surekli kayit ON) ---
    if use_frame_limit:
        print(f"[..] {target_frames} kare yakalanacak (scale: {args.scale}x)")
    else:
        print(f"[..] {target_seconds:.1f} saniye yakalanacak (scale: {args.scale}x)")

    print(f"[..] Cihaza 'k' (keyframe) + 'g' (kayit baslat) komutu gonderiliyor...")
    ser.write(b"k")
    ser.flush()
    ser.write(b"g")
    ser.flush()

    # --- Kareleri topla (surekli kayit, batch siniri YOK) ---
    frames = []
    buf = bytearray()
    cache = FrameCache()
    last_frame_time = time.time()
    TIMEOUT = 10.0  # 10 saniye boyunca hic kare gelmezse zaman asimi

    color_mode_str = ""
    first_frame_time = 0
    raw_frames = []
    recording_start = 0  # ilk kare geldiginde ayarlanir

    def should_stop():
        """Kayit durdurma kosulu"""
        if use_frame_limit:
            return len(raw_frames) >= target_frames
        elif recording_start > 0:
            return (time.time() - recording_start) >= target_seconds
        return False

    try:
        while not should_stop():
            if time.time() - last_frame_time > TIMEOUT:
                print("\n[HATA] Zaman asimi! Cihazdan kare gelmiyor.")
                break

            chunk = read_chunk(ser)
            if chunk:
                buf.extend(chunk)

            # Tum tam kareleri cikar
            while True:
                result = find_next_header(buf, default_swap, default_bgr)
                if result is None:
                    break

                dec = extract_frame(buf, result, cache)
                if dec is None:
                    break

                consumed_end, bgr_img, is_gif, swap, bgr, fw, fh = dec
                del buf[:consumed_end]

                if not is_gif or bgr_img is None:
                    continue

                raw_frames.append((bgr_img.copy(), fw, fh))

                if first_frame_time == 0:
                    first_frame_time = time.time()
                    recording_start = time.time()
                last_frame_time = time.time()

                if not color_mode_str:
                    color_mode_str = "BGR_SWAP" if (swap and bgr) else \
                                     "RGB_SWAP" if (swap and not bgr) else \
                                     "BGR_NOSWAP" if (not swap and bgr) else "CUSTOM"

                # Durum gostergesi
                elapsed = time.time() - recording_start if recording_start > 0 else 0
                if use_frame_limit:
                    print(f"\r  Alinan kare: {len(raw_frames)}/{target_frames}"
                          f"  renk: {color_mode_str}  cap: {fw}x{fh}"
                          f"  sure: {elapsed:.1f}s", end='', flush=True)
                else:
                    print(f"\r  Alinan kare: {len(raw_frames)}"
                          f"  renk: {color_mode_str}  cap: {fw}x{fh}"
                          f"  sure: {elapsed:.1f}s / {target_seconds:.1f}s", end='', flush=True)

                if should_stop():
                    break

    except KeyboardInterrupt:
        print("\n\n[!!] Kayit durduruldu (Ctrl+C).")

    # --- Kayit durdur: 'x' komutu ---
    try:
        ser.write(b"x")
        ser.flush()
    except Exception:
        pass

    # --- GIF isleme ve kaydetme ---
    if len(raw_frames) > 0:
        print(f"\n[..] {len(raw_frames)} kare isleniyor, lutfen bekleyin...")

        # Bilgi icin gercek FPS hesapla
        if len(raw_frames) > 1 and first_frame_time > 0:
            total_time = last_frame_time - first_frame_time
            if total_time > 0:
                real_fps = (len(raw_frames) - 1) / total_time
            else:
                real_fps = 30.0
            
            # Dinamik FPS: GIF hizini oyunun gercek USB aktarim hizina esitle
            gif_delay_ms = max(20, int(1000.0 / real_fps))
            print(f"     Yakalama hizi: {real_fps:.1f} FPS | GIF otomatik uyarlandi: {1000.0/gif_delay_ms:.1f} FPS ({gif_delay_ms} ms delay)")
        else:
            gif_delay_ms = 33

        # --fps override (manuel kontrol)
        if args.fps > 0:
            gif_delay_ms = max(20, int(1000.0 / args.fps))
            print(f"     FPS override: {args.fps} FPS (GIF delay: {gif_delay_ms} ms)")

        for i, (bgr_img, fw, fh) in enumerate(raw_frames):
            rgb_img = bgr_img[..., ::-1]
            img = Image.fromarray(rgb_img)
            img = upscale_to_out(img, fw, fh, fw * args.scale, fh * args.scale, smooth=args.smooth)
            frames.append(img)

        print(f"[OK] GIF birlestiriliyor: {out_path}")
        frames[0].save(
            out_path,
            save_all=True,
            append_images=frames[1:],
            duration=gif_delay_ms,
            loop=0
        )

        # Dosya boyutu
        try:
            file_size = os.path.getsize(out_path)
            if file_size > 1024 * 1024:
                size_str = f"{file_size / (1024*1024):.1f} MB"
            else:
                size_str = f"{file_size / 1024:.0f} KB"
        except Exception:
            size_str = "?"

        total_secs = len(raw_frames) * gif_delay_ms / 1000.0
        print(f"[OK] Islem basarili! Renk modu: {color_mode_str}")
        print(f"     Kare: {len(raw_frames)} | Sure: {total_secs:.1f}s | Boyut: {size_str}")
    else:
        print("\n[HATA] Hic kare alinamadi.")

    ser.close()


if __name__ == '__main__':
    main()
