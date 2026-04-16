#!/usr/bin/env python3
"""
Extract frames from MP4 and create binary frame data for ESP32
"""

import cv2
import numpy as np
import struct
from pathlib import Path

def extract_and_pack_frames(video_path, output_path, target_width=128, target_height=64):
    """
    Extract frames from video and pack them as raw 1-bit data.
    Each frame is 128x64 = 8192 bits = 1024 bytes
    """
    
    cap = cv2.VideoCapture(str(video_path))
    if not cap.isOpened():
        print(f"Error: Cannot open {video_path}")
        return False
    
    fps = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    print(f"Video: {total_frames} frames @ {fps} FPS")
    print(f"Target: {target_width}x{target_height} per frame")
    
    output_data = bytearray()
    frame_count = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        # Convert to grayscale
        if len(frame.shape) == 3:
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        else:
            gray = frame
        
        # Resize to target size
        resized = cv2.resize(gray, (target_width, target_height), interpolation=cv2.INTER_AREA)
        
        # Convert to 1-bit using threshold at 128
        _, binary = cv2.threshold(resized, 128, 255, cv2.THRESH_BINARY)
        
        # Pack into bytes (each bit is 1 pixel, 8 pixels per byte)
        frame_bytes = bytearray()
        pixels = binary.flatten()
        
        for i in range(0, len(pixels), 8):
            byte = 0
            for j in range(8):
                if i + j < len(pixels):
                    # 1 for white (255), 0 for black (0)
                    if pixels[i + j] > 127:
                        byte |= (1 << (7 - j))
            frame_bytes.append(byte)
        
        output_data.extend(frame_bytes)
        frame_count += 1
        
        if frame_count % 50 == 0:
            print(f"  Processed {frame_count}/{total_frames} frames ({len(output_data)} bytes)")
    
    cap.release()
    
    # Write raw binary data
    with open(output_path, 'wb') as f:
        f.write(output_data)
    
    print(f"\nOutput: {output_path}")
    print(f"Frames: {frame_count}")
    print(f"Total size: {len(output_data)} bytes")
    print(f"Per frame: {len(output_data) // frame_count} bytes")
    print(f"Duration: {frame_count / fps:.1f}s @ {fps} FPS")
    
    return frame_count, len(output_data)

if __name__ == '__main__':
    video_path = Path("./videos/Video Project 4.mp4")
    output_path = Path("./data/video.bin")
    
    if video_path.exists():
        frame_count, data_size = extract_and_pack_frames(video_path, output_path)
    else:
        print(f"Error: {video_path} not found")