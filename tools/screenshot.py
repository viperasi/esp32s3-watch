#!/usr/bin/env python3
"""Capture screenshots from the Pragmata Watch via USB Serial JTAG."""

import serial
import struct
import sys
import time
import re

PORT = '/dev/cu.usbmodem1401'
BAUD = 115200

def capture_screenshot(port=PORT, output='screenshot.png'):
    ser = serial.Serial(port, BAUD, timeout=10)

    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.5)
    ser.reset_input_buffer()

    print("Waiting for screenshot...")
    start = time.time()
    all_data = b''

    while time.time() - start < 120:
        chunk = ser.read(65536)
        if chunk:
            all_data += chunk
        if b'<<<SCR_END>>>' in all_data:
            break
        if time.time() - start > 15 and b'<<<SCR_START' not in all_data:
            text = all_data.decode('utf-8', errors='replace')
            lines = [l.strip() for l in text.split('\n') if 'SCREENSHOT' in l]
            for l in lines: print(f"  {l}")
            ser.close()
            raise TimeoutError("No start marker")

    ser.close()
    print(f"Received {len(all_data)} bytes in {time.time()-start:.1f}s")

    # Parse
    text = all_data.decode('utf-8', errors='replace')
    match = re.search(r'<<<SCR_START:W=(\d+):H=(\d+)>>>', text)
    if not match:
        raise IOError("No start marker in text")

    w = int(match.group(1))
    h = int(match.group(2))
    print(f"Image: {w}x{h}")

    # Extract hex lines between markers
    start_marker_end = match.end()
    end_match = re.search(r'<<<SCR_END>>>', text[start_marker_end:])
    if not end_match:
        raise IOError("No end marker")

    body = text[start_marker_end:start_marker_end + end_match.start()]

    # Parse hex lines
    hex_lines = []
    for line in body.strip().split('\n'):
        line = line.strip()
        if not line:
            continue
        # Remove any log output that got mixed in
        hex_chars = ''.join(c for c in line if c in '0123456789abcdef')
        if hex_chars:
            hex_lines.append(hex_chars)

    print(f"Parsed {len(hex_lines)} hex lines")

    # Decode all pixels
    pixel_data = bytearray()
    for hl in hex_lines:
        try:
            pixel_data.extend(bytes.fromhex(hl))
        except ValueError:
            continue

    expected = w * h * 2
    print(f"Pixel data: {len(pixel_data)} bytes (expected {expected})")

    if len(pixel_data) < expected:
        # Pad with zeros for missing rows
        print(f"Padding {expected - len(pixel_data)} missing bytes")
        pixel_data.extend(b'\x00' * (expected - len(pixel_data)))

    # Convert RGB565 -> RGB888
    from PIL import Image
    rgb888 = bytearray(w * h * 3)
    for i in range(w * h):
        pixel = struct.unpack_from('<H', pixel_data, i * 2)[0]
        rgb888[i * 3]     = ((pixel >> 11) & 0x1F) << 3
        rgb888[i * 3 + 1] = ((pixel >> 5) & 0x3F) << 2
        rgb888[i * 3 + 2] = (pixel & 0x1F) << 3

    img = Image.frombytes('RGB', (w, h), bytes(rgb888))
    img.save(output)
    print(f"Saved: {output} ({w}x{h})")

if __name__ == '__main__':
    output = sys.argv[1] if len(sys.argv) > 1 else 'screenshot.png'
    capture_screenshot(output=output)
