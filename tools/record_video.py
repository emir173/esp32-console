#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
============================================================
  E-OS USB Video Recorder -- RGB565 -> MP4/AVI (Dinamik FPS)
============================================================
  ESP32-S3'ten seri porttan sizdirilan raw RGB565 framebuffer
  karelerini OpenCV VideoWriter ile kaliteli bir video dosyasina
  (MP4/AVI) donusturur.

  Renk modu (BGR_SWAP / RGB_SWAP / BGR_NOSWAP) header'dan
  OTOMATIK algilanir -- manuel --rgb565 / --no-swap-bytes
  flag'leri yalnizca geri uyumlu (eski firmware) icin fallback.

  FPS MODU (onemli -- video hizlanma sorunu cozumu):
    --fps-mode auto  (varsayilan): Kareler bellekte toplanir, kayit
                     bitince GERCEK ortalam alim FPS'i (orn: Doom 27.0)
                     MP4'e yazilir. Video her zaman gercek zamanli (1x)
                     oynatilir. --fps flag'i yalnizca fixed modda anlamlidir.
    --fps-mode fixed : Eski davranis. --fps (varsayilan 60) ile CANLI
                     yazilir. Alim hizi dusukse video hizli oynar.

  Kullanim:
    python record_video.py --port COM9
    python record_video.py --port COM9 --preview --scale 3
    python record_video.py --port COM9 --max-frames 600 --out oyun.mp4
    python record_video.py --port COM9 --fps-mode fixed --fps 30

  Durdurmak icin: Ctrl+C (video guvenli sekilde finalize edilir)
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

# Shared module (auto-color, numpy decode, serial utils, read_chunk)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from eos_capture import (
    WIDTH, HEIGHT,
    find_next_header, rgb565_to_bgr, upscale_np_nearest,
    FrameCache, extract_frame,
    auto_detect_port, open_serial, read_chunk,
)
import numpy as np

# cv2 (OpenCV) lazy import -- --help cv2 olmadan da calissin
cv2 = None

def _import_cv2():
    global cv2
    if cv2 is None:
        try:
            import cv2 as _cv2
            cv2 = _cv2
        except ImportError:
            print("[HATA] OpenCV (cv2) yuklu degil!")
            print("       Kurmak icin: pip install opencv-python")
            sys.exit(1)
    return cv2


# =
#  VIDEOWRITOR OLUSTUR (codec fallback'li)
# =
def create_video_writer(path, fps, codec_str, w, h):
    """
    VideoWriter olustur. Belirtilen codec calismazsa alternatif dener.
    MP4 icin: mp4v -> avc1 -> H264
    AVI icin: XVID -> MJPG -> mp4v
    """
    _import_cv2()
    ext = os.path.splitext(path)[1].lower()
    if ext == '.avi':
        fallbacks = [codec_str, 'XVID', 'MJPG', 'mp4v']
    elif ext == '.mov':
        fallbacks = [codec_str, 'avc1', 'mp4v', 'H264']
    else:  # .mp4
        fallbacks = [codec_str, 'mp4v', 'avc1', 'H264']

    # Ayni codec'i tekrar deneme
    seen = set()
    for codec in fallbacks:
        if codec in seen:
            continue
        seen.add(codec)
        fourcc = cv2.VideoWriter_fourcc(*codec)
        writer = cv2.VideoWriter(path, fourcc, float(fps), (w, h), isColor=True)
        if writer.isOpened():
            return writer
        writer.release()

    print(f"[HATA] Hicbir video codec acilamadi: {fallbacks}")
    print("       OpenCV'yi ffmpeg destegiyle kurmayi deneyin:")
    print("       pip install opencv-python (veya opencv-contrib-python)")
    return None


def _codec_ok(path, codec_str, w, h):
    """Codec'in bu yolda calisabilir oldugunu test et (yazmadan)."""
    w = create_video_writer(path, 1, codec_str, w, h)
    if w is not None:
        w.release()
        # Test dosyasini sil (bos dosya birakma)
        try:
            if os.path.exists(path):
                os.remove(path)
        except Exception:
            pass
        return True
    return False


# =
#  ANA KAYIT FONKSIYONU
# =
def main():
    parser = argparse.ArgumentParser(
        description='E-OS USB Video Recorder -- RGB565 -> MP4/AVI (Auto-Color, Dinamik FPS)',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('--port', type=str, default=None,
                        help='Seri port (orn: COM9). Bos = otomatik tespit.')
    parser.add_argument('--baud', type=int, default=921600,
                        help='Baud hizi (varsayilan: 921600)')
    parser.add_argument('--fps', type=int, default=60,
                        help='Cikti video FPS (yalnizca --fps-mode fixed; '
                             'auto modda gercek alim FPS kullanilir)')
    parser.add_argument('--fps-mode', choices=['auto', 'fixed'], default='auto',
                        help='auto (varsayilan): bitince ortalam alim FPS ile yaz '
                             '(gercek zamanli 1x). fixed: --fps ile canli yaz.')
    parser.add_argument('--out', type=str, default=None,
                        help='Cikti dosya yolu (bos = otomatik timestamp)')
    parser.add_argument('--codec', type=str, default='mp4v',
                        help="FourCC codec: mp4v, avc1, H264, XVID, MJPG")
    parser.add_argument('--ext', type=str, default='mp4',
                        help='Dosya uzantisi: mp4, avi, mov (varsayilan: mp4)')
    parser.add_argument('--preview', action='store_true',
                        help='Canli onizleme penceresi goster')
    parser.add_argument('--scale', type=int, default=1,
                        help='Tamsayi buyutme: 1, 2, 3, 4 (varsayilan: 1)')
    parser.add_argument('--smooth', action='store_true',
                        help='Buyuturken pikselleri yumusat (INTER_CUBIC). Konsol ekranini taklit eder.')
    parser.add_argument('--max-frames', type=int, default=0,
                        help='Maksimum kare (0 = sinirsiz, Ctrl+C ile durdur)')
    parser.add_argument('--seconds', type=float, default=0.0,
                        help='Kayit suresi saniye (0 = sinirsiz, Ctrl+C ile durdur)')
    # Geri uyumlu renk flag'lari (eski firmware icin fallback -- yeni firmware auto)
    parser.add_argument('--no-swap-bytes', action='store_true',
                        help='Eski format icin byte swap KAPALI (doom/direkt fb)')
    parser.add_argument('--rgb565', action='store_true',
                        help='Eski format icin RGB565 (TFT_* sabitleri)')
    args = parser.parse_args()

    fps_mode = args.fps_mode

    # Geri uyumlu default renk modu (eski FRAME\n formati icin)
    # Yeni firmware'de header'dan otomatik algilanir, bu flag'ler goz ardi edilir
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
        out_path = os.path.join(capture_dir, f"video_{ts}.{args.ext}")

    # --- Video cozunurlugu (scale uygula) ---
    out_w = WIDTH  * args.scale
    out_h = HEIGHT * args.scale

    # --- Writer hazirligi ---
    #   auto  : kareleri bellekte topla, writer'i bitince gercek FPS ile olustur.
    #           Codec'i basinda test et (kotu surpriz olmasin), sonra release.
    #   fixed : writer'i basinda --fps ile olustur, dongude canli yaz.
    buffered_frames = []          # auto modda kareleri burada biriktir
    writer = None
    if fps_mode == 'fixed':
        writer = create_video_writer(out_path, args.fps, args.codec, out_w, out_h)
        if writer is None:
            sys.exit(1)
        print(f"[OK] VideoWriter hazir (fixed) | FPS={args.fps} | {out_path}")
    else:
        # auto: codec test (yazmadan)
        if not _codec_ok(out_path, args.codec, out_w, out_h):
            sys.exit(1)
        print(f"[OK] Codec testi gecti (auto) | kareler bellekte biriktirilecek, "
              f"FPS kayit bitince hesaplanacak")
        if args.max_frames <= 0:
            print(f"[!] UYARI: --fps-mode auto + sinirsiz kayit kareleri BELLEKTE tutar.")
            print(f"    Uzun kayitlarda RAM dolar. Sinir icin --max-frames 600 onerilir.")

    # --- Seri port baglan (shared module) ---
    port_name = args.port or auto_detect_port()
    if not port_name:
        print("[HATA] Seri port bulunamadi. --port parametresi ile belirtin.")
        sys.exit(1)

    print(f"[..] Baglaniliyor -> {port_name} @ {args.baud} baud...")
    try:
        ser = open_serial(port_name, args.baud, timeout=0.2)
    except Exception as e:
        print(f"[HATA] {port_name} acilamadi!")
        print(f"       Arduino IDE Serial Monitor'un KAPALI oldugundan emin olun.")
        print(f"       Detay: {e}")
        sys.exit(1)

    time.sleep(0.5)
    ser.reset_input_buffer()
    print(f"[OK] Baglanti kuruldu. Renk modu: OTOMATIK (header'dan algilanir)")
    print(f"     Geri uyumlu fallback: {'BGR_SWAP' if (default_swap and default_bgr) else 'OZEL'}")
    print(f"     Scale: {args.scale}x | Chunk: 65536 byte (usbser.sys fix)")
    print(f"     FPS modu: {fps_mode}")

    # --- ilk 'k' (keyframe) + 'g' (surekli kayit baslat) komutunu gonder ---
    print("[..] Cihaza 'k' (keyframe) + 'g' (kayit baslat) komutu gonderiliyor...")
    ser.write(b"k")
    ser.flush()
    ser.write(b"g")
    ser.flush()

    # --- Kayit dongusu ---
    buf = bytearray()
    cache = FrameCache()
    frames_written = 0
    start_time = time.time()
    fps_timer = time.time()
    fps_counter = 0
    actual_fps = 0.0
    detected_color = ""  # Ilk karede algilanan renk modu


    print(f"[OK] Kayit basladi! {'Onizleme: Acik' if args.preview else 'Onizleme: Kapali'}")
    print("     Durdurmak icin: Ctrl+C\n")

    try:
        while True:
            if args.max_frames > 0 and frames_written >= args.max_frames:
                break
            if args.seconds > 0.0 and (time.time() - start_time) >= args.seconds:
                break

            # Serial'den buyuk parca oku (Windows usbser.sys darbogazini asar)
            chunk = read_chunk(ser)  # max(ser.in_waiting, 65536)
            if chunk:
                buf.extend(chunk)

            # Tum tam kareleri cikar (auto-color + auto-resolution + delta modu)
            while True:
                result = find_next_header(buf, default_swap, default_bgr)
                if result is None:
                    break

                # FULL/DELTA decode + cache patch
                dec = extract_frame(buf, result, cache)
                if dec is None:
                    break  # payload tam degil, daha fazla veri bekle

                consumed_end, bgr_img, is_gif, swap, bgr, fw, fh = dec
                del buf[:consumed_end]

                # Sadece FRAME (video kare) isle, SHOT ve decode edilemeyen
                # delta'yi (out of sync) atla. Cache yine de guncellendi.
                if not is_gif or bgr_img is None:
                    continue

                # Ilk karede algilanan renk modunu goster
                if not detected_color:
                    detected_color = "BGR_SWAP" if (swap and bgr) else \
                                     "RGB_SWAP" if (swap and not bgr) else \
                                     "BGR_NOSWAP" if (not swap and bgr) else "CUSTOM"
                    print(f"[OK] Renk modu algilandi: {detected_color}"
                          f" (swap={swap}, bgr565={bgr}) | cap: {fw}x{fh}\n")

                # capture -> out_w x out_h upscale
                if args.smooth:
                    bgr_img = upscale_np_nearest(bgr_img, fw, fh, out_w, out_h, smooth=True)
                else:
                    bgr_img = upscale_np_nearest(bgr_img, fw, fh)
                    if args.scale > 1:
                        # hizli tam sayi buyutme (chunky piksel)
                        bgr_img = np.repeat(
                            np.repeat(bgr_img, args.scale, axis=0),
                            args.scale, axis=1
                        )

                # Kareyi cikisa gonder
                if fps_mode == 'fixed':
                    writer.write(bgr_img)
                else:
                    # auto: bellekte biriktir (her kare yeni array -> referans yeterli)
                    buffered_frames.append(bgr_img)
                frames_written += 1
                fps_counter += 1

                # Canli onizleme
                if args.preview:
                    cv2.imshow('E-OS Video Recorder (q = durdur)', bgr_img)
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord('q'):
                        raise KeyboardInterrupt

                # Saniyelik istatistik
                now = time.time()
                if now - fps_timer >= 1.0:
                    actual_fps = fps_counter / (now - fps_timer)
                    fps_timer = now
                    fps_counter = 0
                    elapsed = now - start_time
                    print(
                        f"\r  Kare: {frames_written:>6d} | "
                        f"Alim: {actual_fps:5.1f} FPS | "
                        f"Sure: {elapsed:6.1f}s",
                        end='', flush=True
                    )



    except KeyboardInterrupt:
        print("\n\n[!!] Kayit durduruldu (Ctrl+C).")

    except Exception as e:
        print(f"\n[HATA] Beklenmeyen hata: {e}")

    finally:
        # --- Kayit durdur: 'x' komutu ---
        try:
            ser.write(b"x")
            ser.flush()
        except Exception:
            pass
        # --- Temizlik ---
        if args.preview:
            cv2.destroyAllWindows()
        ser.close()

        elapsed = time.time() - start_time
        avg_fps = frames_written / elapsed if elapsed > 0 else 0

        # --- auto mod: gercek FPS ile writer olustur ve kareleri yaz ---
        if fps_mode == 'auto' and frames_written > 0:
            # MP4 FPS min 1; sifira bolme engelle. 1 ondalik yuvarla.
            out_fps = round(avg_fps, 1) if avg_fps > 0 else 1
            if out_fps < 1:
                out_fps = 1
            print(f"\n[..] Bellekteki {frames_written} kare {out_fps:.1f} FPS ile "
                  f"yaziliyor (gercek zamanli 1x)...")
            writer = create_video_writer(out_path, out_fps, args.codec, out_w, out_h)
            if writer is None:
                print("[HATA] VideoWriter olusturulamadi! Kareler kaydedilemedi.")
                frames_written = 0
            else:
                for f in buffered_frames:
                    writer.write(f)
                writer.release()
            buffered_frames.clear()
        elif fps_mode == 'fixed':
            if writer is not None:
                writer.release()

        # Dosya boyutu
        try:
            file_size = os.path.getsize(out_path)
            if file_size > 1024 * 1024:
                size_str = f"{file_size / (1024*1024):.1f} MB"
            else:
                size_str = f"{file_size / 1024:.0f} KB"
        except:
            size_str = "?"

        cikti_fps = (round(avg_fps, 1) if (fps_mode == 'auto' and avg_fps > 0)
                     else args.fps)

        print(f"\n{'='*50}")
        print(f"  Video kaydedildi: {out_path}")
        print(f"  Toplam kare     : {frames_written}")
        print(f"  Sure            : {elapsed:.1f} sn")
        print(f"  Ortalama alim   : {avg_fps:.1f} FPS")
        print(f"  Cikti FPS       : {cikti_fps}  ({fps_mode} modu)")
        print(f"  Dosya boyutu    : {size_str}")
        print(f"  Cozunurluk      : {out_w}x{out_h}")
        print(f"  Renk modu       : {detected_color or '(kare alinmadi)'}")
        if fps_mode == 'fixed' and avg_fps > 0 and avg_fps < args.fps * 0.85:
            print(f"\n  [!] UYARI: Alim hizi ({avg_fps:.1f}) hedef FPS'ten dusuk.")
            print(f"      Video {args.fps} FPS oynatilacak -> gercek sureden daha hizli gorunebilir.")
            print(f"      Gercek zamanli kayit icin: --fps-mode auto  (onerilir)")
        print(f"{'='*50}")


if __name__ == '__main__':
    main()
