#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
============================================================
  E-OS USB Capture -- Ortak Kutuphane (Shared Module)
============================================================
  ESP32-S3 USB framebuffer dump sisteminin Python tarafinda
  tum capture script'leri (capture.py, capture_gif.py,
  record_video.py) tarafindan paylasilan ortak fonksiyonlar.

  Icerik:
    - COLOR_MODES    : Header suffix -> (swap_bytes, is_bgr565)
    - rgb565_to_bgr  : numpy vektorize renk donusumu (~0.3ms/kare)
    - find_next_header: Buffer'da FRAME/SHOT header bul + renk modu parse
    - auto_detect_port: ESP32-S3 USB CDC portu otomatik bul
    - open_serial    : Seri port baglan (DTR/RTS false)
    - read_chunk     : Windows usbser.sys darbogazini asan buyuk parca oku

  Wire format (dev_tools.h) — SATIR BAZLI DELTA SIKISTIRMA:
    Yeni format (renk + cozunurluk + delta modu, 4 field):
      FRAME:BGR_SWAP:160x128:FULL\n   + 40960 byte               (keyframe)
      FRAME:BGR_SWAP:160x128:DELTA\n  + [u16 nRows][nRows x (u16 row + 320B)]
      SHOT:RGB_SWAP:160x128:FULL\n    + 40960 byte               (screenshot)
    Geri uyumlu (eski firmware, mod field yok -> FULL varsay):
      FRAME:BGR_SWAP:70x56\n        + (W*H*2) byte   (downscale'li eski FW)
      FRAME:BGR_SWAP\n              + 40960 byte     (en eski, 160x128)
      SHOT:E-OS\n                   + 40960 byte     (en eski)

  Renk modu suffix anlami:
    BGR_SWAP   : swap=True,  bgr=True   (RGB() makrosu + sprite)
    RGB_SWAP   : swap=True,  bgr=False  (TFT_* sabitleri + sprite)
    BGR_NOSWAP : swap=False, bgr=True   (RGB_FIX() + direkt fb / doom)

  Delta modu (4. field):
    FULL  : tam kare (W*H*2 byte) -> FrameCache.set_full()
    DELTA : [u16 nRows] + nRows x (u16 satir_no + W*2 byte) -> FrameCache.apply_delta()
    None  : eski FW (field yok) -> FULL varsayilir

  Cozunurluk: tam 160x128 (downscale KALDIRILDI, delta ile bant genisligi dusuruldu).
  Eski FW'de field yok -> 160x128. find_next_header() 7-tuple doner (mode ek).
 ============================================================
"""

import numpy as np
import serial
import serial.tools.list_ports
import time

# = SABITLER =
# OUT_*  : cikti (display) cozunurlugu -- Python buraya upscale eder.
# WIDTH/HEIGHT: geri uyum icin default frame cozunurlugu (eski FW 160x128).
OUT_WIDTH   = 160
OUT_HEIGHT  = 128
WIDTH       = 160
HEIGHT      = 128
FRAME_BYTES = WIDTH * HEIGHT * 2   # 40960 byte (eski FW default; yeni FW dynamic)

# = RENK MODLARI =
# Header suffix -> (swap_bytes, is_bgr565)
#   swap_bytes : True  = big-endian oku (TFT_eSPI sprite byte swap)
#                False = little-endian (direkt framebuffer / RGB_FIX)
#   is_bgr565  : True  = B ust 5 bitte, R alt 5 bitte (RGB() makrosu / RGB_FIX)
#                False = R ust 5 bitte, B alt 5 bitte (TFT_* sabitleri)
COLOR_MODES = {
    'BGR_SWAP':   (True,  True),
    'RGB_SWAP':   (True,  False),
    'BGR_NOSWAP': (False, True),
}

# Geri uyumlu default (eski FRAME\n / SHOT:E-OS\n formati icin)
DEFAULT_SWAP = True
DEFAULT_BGR  = True


# ============================================================
#  RGB565 -> BGR888 DONUSUMU (numpy vektorize)
# ============================================================
def rgb565_to_bgr(raw_bytes, swap_bytes=True, is_bgr565=True, w=WIDTH, h=HEIGHT):
    """
    Raw RGB565/BGR565 bytelarini numpy ile vektorize BGR888'e cevirir.
    OpenCV BGR formatina uygun [B, G, R] kanal siralamasi dondurur.
    PIL icin: rgb = bgr[..., ::-1] ile BGR->RGB cevrilebilir.

    w, h : frame cozunurlugu (yeni FW header'dan dynamic, eski FW 160x128).
    Performans: ~0.3ms/kare (PIL loop: ~50ms/kare -> ~150x hiz).
    """
    # 1) Raw bytelari 16-bit piksellere cevir
    if swap_bytes:
        buf = np.frombuffer(raw_bytes, dtype='>u2')   # big-endian (TFT_eSPI)
    else:
        buf = np.frombuffer(raw_bytes, dtype='<u2')   # little-endian (direkt fb)

    buf = buf.reshape(h, w)

    # 2) Bit kaydirma ile kanallari cikar (tam vektorize, loop yok)
    upper = (buf >> 11) & 0x1F    # ust 5 bit
    mid   = (buf >> 5)  & 0x3F    # orta 6 bit
    lower = buf & 0x1F            # alt 5 bit

    # 3) 5/6 bit -> 8 bit genisletme (bit-replication -> smooth gradient)
    u8 = ((upper << 3) | (upper >> 2)).astype(np.uint8)
    m8 = ((mid   << 2) | (mid   >> 4)).astype(np.uint8)
    l8 = ((lower << 3) | (lower >> 2)).astype(np.uint8)

    # 4) BGR565: upper=B, lower=R -> [B,G,R] = [u8, m8, l8]
    #    RGB565: upper=R, lower=B -> [B,G,R] = [l8, m8, u8]
    if is_bgr565:
        bgr = np.stack([u8, m8, l8], axis=-1)
    else:
        bgr = np.stack([l8, m8, u8], axis=-1)

    return np.ascontiguousarray(bgr, dtype=np.uint8)


# ============================================================
#  HEADER PARSE -- Buffer'da FRAME/SHOT header bul + renk + WxH parse
# ============================================================
def _parse_resolution(field, default_w=WIDTH, default_h=HEIGHT):
    """ '70x56' -> (70, 56). Gecersizse default. """
    try:
        s = field.decode('ascii', errors='ignore')
        if 'x' in s:
            w_s, h_s = s.split('x', 1)
            return int(w_s), int(h_s)
    except Exception:
        pass
    return default_w, default_h


def find_next_header(buf, default_swap=None, default_bgr=None):
    """
    Buffer'da bir sonraki gecerli FRAME/SHOT header'ini bulur, renk modunu
    ve capture cozunurlugunu (WxH) otomatik parse eder.

    Parametreler:
      buf            : bytearray - serial'dan biriken ham veri
      default_swap   : bool - eski format (suffix yok) icin swap defaultu
      default_bgr    : bool - eski format (suffix yok) icin bgr defaultu

    Geri donus:
      (header_end, is_gif, swap_bytes, is_bgr565, w, h, mode) veya None
      header_end : header'dan sonraki ilk byte indeksi (pixel data baslangici)
      w, h       : frame cozunurlugu (eski FW -> 160x128)
      mode       : 'FULL' | 'DELTA' | None (eski FW, field yok -> FULL varsay)

    Wire format:
      FRAME:<COLOR>:<WxH>:<MODE>\n  (yeni, delta)   SHOT:<COLOR>:<WxH>:FULL\n (yeni)
      FRAME:<COLOR>:<WxH>\n         (orta)          SHOT:<COLOR>\n            (eski)
      FRAME\n                       (en eski)       SHOT:E-OS\n              (en eski)
      MODE = FULL | DELTA (yoksa None -> cagiran FULL varsayar)

    False-match eleri: binary veri icindeki rastgele "FRAME"/"SHOT"
    dizileri gecerli header formatina uymuyorsa atlanir.
    """
    if default_swap is None:
        default_swap = DEFAULT_SWAP
    if default_bgr is None:
        default_bgr = DEFAULT_BGR

    pos = 0
    while pos < len(buf):
        # FRAME veya SHOT ara
        f_idx = buf.find(b'FRAME', pos)
        s_idx = buf.find(b'SHOT', pos)

        # En erken bulunu sec
        if f_idx >= 0 and (s_idx < 0 or f_idx <= s_idx):
            idx, is_gif = f_idx, True
        elif s_idx >= 0:
            idx, is_gif = s_idx, False
        else:
            return None

        # Newline ara (header satirinin sonu)
        nl = buf.find(b'\n', idx)
        if nl < 0:
            # Header tamamlanmamis, daha fazla veri bekle
            return None

        header = bytes(buf[idx:nl])
        header_end = nl + 1

        # ':' ile bol -> [KOMUT, COLOR, WxH, MODE, ...]
        parts = header.split(b':')
        cmd = parts[0]
        color_field = parts[1] if len(parts) >= 2 else b''
        res_field   = parts[2] if len(parts) >= 3 else b''
        mode_field  = parts[3] if len(parts) >= 4 else b''

        # Renk modunu cikar. False-match eleme: eger bir color_field varsa
        # (yani header 'FRAME:xxx' formatinda) bu ya COLOR_MODES'de olmali ya
        # da bilinen legacy marker (SHOT:E-OS) olmali. Aksi halde binary verideki
        # rastgele 'FRAME:garbage\n' false-match'leri stream sync'i bozar.
        # Bare 'FRAME'/'SHOT' (field yok) -> default renk (en eski FW).
        # (COLOR_MODES string key'leri -> bytes field decode edilip karsilastirilir.)
        color_str = color_field.decode('ascii', errors='ignore') if color_field else ''
        if color_field == b'':
            s, b = default_swap, default_bgr
        elif color_str in COLOR_MODES:
            s, b = COLOR_MODES[color_str]
        elif color_field == b'E-OS':   # legacy SHOT:E-OS
            s, b = default_swap, default_bgr
        else:
            pos = idx + 1               # false-match: atla
            continue

        # Cozunurlugu cikar (yeni FW), eski FW -> 160x128
        w, h = _parse_resolution(res_field)

        # Delta modunu cikar (4. field). False-match eleme: bos degilse
        # FULL veya DELTA olmali; aksi halde binary verideki rastgele
        # 'FRAME:...:...:garbage\n' false-match'leri atlanir.
        # Bos (eski FW) -> None (cagiran FULL varsayar).
        mode_str = mode_field.decode('ascii', errors='ignore') if mode_field else ''
        if mode_field == b'':
            mode = None
        elif mode_str in ('FULL', 'DELTA'):
            mode = mode_str
        else:
            pos = idx + 1               # false-match: atla
            continue

        # Komut dogrula (FRAME/SHOT)
        if is_gif and cmd == b'FRAME':
            return (header_end, True, s, b, w, h, mode)
        if (not is_gif) and cmd == b'SHOT':
            return (header_end, False, s, b, w, h, mode)

        # False-match: bu pozisyondan sonra tekrar ara
        pos = idx + 1

    return None


# ============================================================
#  DELTA FRAME DECODER -- Satir bazli delta patch + cache
# ============================================================
class FrameCache:
    """
    ESP32'nin satir bazli delta encoder'inin Python tarafindaki karsiligi.
    Onceki karenin raw RGB565 bytelarini (W*H*2) tutar; gelen delta
    satir patch'lerini uygular. Tam kare (FULL) gelirse cache'i tamamen
    degistirir.

    Akis:
      FULL  -> set_full(raw)       : cache = raw, valid=True
      DELTA -> apply_delta(payload): cache'e satir patch'leri uygula
      BGR cikti -> rgb565_to_bgr(cache.raw, swap, bgr, w, h)

    Senkronizasyon: ESP32 _prevFrame'i sadece GONDERDIGI karelerle
    gunceller. Python cache'i de sadece GELEN karelerle gunceller.
    Drop edilen kareler iki tarafi da tutarli tutar -> delta zinciri bozulmaz.
    """
    def __init__(self):
        self.raw = bytearray()
        self.w = 0
        self.h = 0
        self.valid = False

    def set_full(self, raw, w, h):
        """Tam kare (keyframe) ile cache'i sifirla."""
        self.raw = bytearray(raw)
        self.w, self.h = w, h
        self.valid = True

    def apply_delta(self, payload, w, h):
        """
        Delta payload'ini cache'e uygula.
        payload: [u16 nRows] + nRows x (u16 satir_no + W*2 byte)
        Cozunurluk uyumsuzlugu (keyframe bekleniyor) guvenligi: reset.
        """
        if not self.valid or self.w != w or self.h != h:
            self.raw = bytearray(w * h * 2)
            self.w, self.h = w, h
            self.valid = True
        n_rows = int.from_bytes(payload[:2], 'little')
        off = 2
        rb = w * 2
        for _ in range(n_rows):
            row = int.from_bytes(payload[off:off + 2], 'little')
            off += 2
            start = row * rb
            # Seri veri bozulmasinda gecersiz satir numarasini atla
            if row < h:
                self.raw[start:start + rb] = payload[off:off + rb]
            off += rb


def extract_frame(buf, result, cache):
    """
    find_next_header() sonucunu alir, FULL/DELTA modunu cozer ve kareyi
    decode eder. Delta modunda cache'e patch uygular. Eksik payload -> None
    (cagiran daha fazla veri bekler).

    Parametreler:
      buf    : bytearray - serial'dan biriken ham veri (mutate edilmez)
      result : find_next_header() 7-tuple'i
      cache  : FrameCache - delta decoder durumu (mutate edilir)

    Geri donus:
      (consumed_end, bgr_img, is_gif, swap, bgr, w, h)
        consumed_end : buf'ta tuketilen son byte indeksi -> del buf[:consumed_end]
        bgr_img      : numpy (h,w,3) BGR veya None
                       (delta geldi ama cache bos -> out of sync, atla)
      None -> payload tam degil, daha fazla veri bekle

    Not: Cache HER zaman guncellenir (atlanan turler dahil), boylece ESP32'nin
    _prevFrame'i ile Python cache'i senkron kalir.
    """
    header_end, is_gif, swap, bgr, w, h, mode = result
    frame_bytes = w * h * 2
    is_delta = (mode == 'DELTA')

    if not is_delta:
        # FULL (mode 'FULL' veya None/eski FW): sabit boyut
        if len(buf) < header_end + frame_bytes:
            return None
        raw = bytes(buf[header_end:header_end + frame_bytes])
        cache.set_full(raw, w, h)
        bgr_img = rgb565_to_bgr(raw, swap, bgr, w, h)
        return (header_end + frame_bytes, bgr_img, is_gif, swap, bgr, w, h)

    # DELTA: [u16 nRows] + nRows x (u16 row + w*2 byte)
    if len(buf) < header_end + 2:
        return None
    n_rows = int.from_bytes(buf[header_end:header_end + 2], 'little')
    record_size = 2 + w * 2
    payload_size = 2 + n_rows * record_size
    if len(buf) < header_end + payload_size:
        return None
    payload = bytes(buf[header_end:header_end + payload_size])
    if not cache.valid:
        # Delta geldi ama keyframe yok (stream out of sync).
        # Byte'lari tuket (buffer ilerlesin) ama goruntu uretme.
        # ESP32 periyodik keyframe (her 60 kare) ile resync eder.
        return (header_end + payload_size, None, is_gif, swap, bgr, w, h)
    cache.apply_delta(payload, w, h)
    bgr_img = rgb565_to_bgr(cache.raw, swap, bgr, w, h)
    return (header_end + payload_size, bgr_img, is_gif, swap, bgr, w, h)


# ============================================================
#  UPSCALE -- capture cozunurlugunden display cozunurlugune
# ============================================================
def upscale_to_out(img, w, h, out_w=OUT_WIDTH, out_h=OUT_HEIGHT, smooth=False):
    """
    PIL Image'i (w,h) cozunurlukten (out_w,out_h) upscale eder.
    smooth=True ise LANCZOS (yumusak), False ise NEAREST (chunky piksel) kullanir.
    (w,h)==(out_w,out_h) ise dokunmez.
    PIL lazy import edilir.
    """
    if (w, h) != (out_w, out_h):
        from PIL import Image
        resample = Image.LANCZOS if smooth else Image.NEAREST
        img = img.resize((out_w, out_h), resample)
    return img


def upscale_np_nearest(bgr_img, w, h, out_w=OUT_WIDTH, out_h=OUT_HEIGHT, smooth=False):
    """
    numpy BGR array'i (h,w,3) -> (out_h,out_w,3) upscale (OpenCV yolu).
    smooth=True ise INTER_CUBIC (yumusak), False ise INTER_NEAREST.
    (w,h)==(out_w,out_h) ise kopyasiz doner. cv2 lazim (cagrı yerinde import).
    """
    if (w, h) == (out_w, out_h):
        return bgr_img
    import cv2
    interp = cv2.INTER_CUBIC if smooth else cv2.INTER_NEAREST
    return cv2.resize(bgr_img, (out_w, out_h), interpolation=interp)


# ============================================================
#  SERI PORT UTILS
# ============================================================
def auto_detect_port():
    """ESP32-S3 USB CDC portunu otomatik bul (HWCDC/JTAG/CH340)."""
    candidates = []
    for port in serial.tools.list_ports.comports():
        desc = (port.description or '').lower()
        hwid = (port.hwid or '').lower()
        if any(kw in desc for kw in ['usb', 'jtag', 'serial', 'ch340', 'cp210', 'ftdi']):
            if any(kw in hwid for kw in ['pid:303a', 'vid:303a', 'pid:1a86', 'pid:10c4']):
                candidates.insert(0, port.device)  # ESP32-S3 USB oncelikli
            else:
                candidates.append(port.device)
    return candidates[0] if candidates else None


def open_serial(port=None, baud=921600, timeout=0.2):
    """
    Seri port baglan. DTR/RTS False -> ESP32-S3 USB-Serial-JTAG (HWCDC/JTAG)
    modunda otomatik reset ENGELLENIR. HWCDC peripheral'inda DTR ve RTS
    sinyalleri dogrudan reset (EN/CHIP_PU) ve boot (GPIO0) pinlerine baglidir;
    True kombinasyonu cihazi reset+boot mode'a sokar. Bu yuzden dtr/rts
    property'leri open()'dan ONCE False ayarlanir; kapali portta _dtrstate/
    _rtsstate saklanir, open()->_reconfigure_port() bunlari atomik uygular.
    open'dan SONRA setDTR/setRTS cagrilamaz: (1,1)->(0,0) gecisi ureterek
    reseti tetikler (onceki kodun reset sorununun sebebi). Kapali port
    Windows'ta (0,0)'da oldugu icin (0,0)->(0,0) gecis = reset yok. USBCDC
    (USB-OTG/TinyUSB) modunda zararsiz (connected
    flag donanim seviyesinde). Port verilmezse auto_detect_port() cagirilir.
    Basarili: Serial nesnesi. Hata: exception firlatir.
    """
    if port is None:
        port = auto_detect_port()
    if port is None:
        raise ValueError("Seri port bulunamadi. --port ile belirtin.")

    # dsrdtr/rtscts kapali (donanimsal akis kontrolu DTR/RTS'i degistirmesin)
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = timeout
    ser.dsrdtr = False
    ser.rtscts = False
    
    # dtr/rts property'leri open()'dan ONCE ayarlanir: kapali portta _dtrstate/
    # _rtsstate saklar, open()->_reconfigure_port() atomik uygular. setDTR/
    # setRTS sadece acik portta calisir; open'dan sonra cagrilinca (1,1)->(0,0)
    # gecisi ureterek reseti tetikler. Iki hat ayni seviyede (0,0) = reset
    # kosulu (DTR=0 & RTS=1) yok; kapali port (0,0) -> (0,0) gecis yok = reset yok.
    ser.dtr = False
    ser.rts = False
    
    ser.open()
        
    return ser


def read_chunk(ser, min_chunk=65536):
    """
    Windows usbser.sys darbogazini asan buyuk parca okuma.
    ser.in_waiting ile mevcut veriyi okur, minimum 65536 byte ister.
    Bu, kucuk parca okumanin yarattigi USB transaction overhead'i engeller.

    Seri port timeout'u dolunca elindekiyle doner (bloklamaz).
    """
    to_read = max(ser.in_waiting, min_chunk)
    return ser.read(to_read)
