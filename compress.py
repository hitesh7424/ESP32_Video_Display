#!/usr/bin/env python3
"""
Extract frames from MP4 and create binary frame data for ESP32
"""

import cv2
import argparse
from pathlib import Path


def pack_1bit_frame(gray_frame, target_width, target_height):
    """Resize a grayscale frame and pack pixels into 1-bit-per-pixel bytes."""
    # Resize to target size
    resized = cv2.resize(
        gray_frame, (target_width, target_height), interpolation=cv2.INTER_AREA
    )

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
                    byte |= 1 << (7 - j)
        frame_bytes.append(byte)

    return frame_bytes


def extract_and_pack_frames(
    video_path, output_path, target_width=128, target_height=64
):
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

        frame_bytes = pack_1bit_frame(gray, target_width, target_height)

        output_data.extend(frame_bytes)
        frame_count += 1

        if frame_count % 50 == 0:
            print(
                f"  Processed {frame_count}/{total_frames} frames ({len(output_data)} bytes)"
            )

    cap.release()

    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Write raw binary data
    with open(output_path, "wb") as f:
        f.write(output_data)

    print(f"\nOutput: {output_path}")
    print(f"Frames: {frame_count}")
    print(f"Total size: {len(output_data)} bytes")
    print(f"Per frame: {len(output_data) // frame_count} bytes")
    print(f"Duration: {frame_count / fps:.1f}s @ {fps} FPS")

    return frame_count, len(output_data)


def extract_and_pack_multi(video_path, output_configs):
    """
    Extract frames once and generate multiple output binaries.
    output_configs: list of tuples (output_path, width, height)
    """
    cap = cv2.VideoCapture(str(video_path))
    if not cap.isOpened():
        print(f"Error: Cannot open {video_path}")
        return False

    fps = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    print(f"Video: {total_frames} frames @ {fps} FPS")
    print("Generating outputs:")
    for output_path, width, height in output_configs:
        print(f"  - {output_path} ({width}x{height})")

    output_buffers = {output_path: bytearray() for output_path, _, _ in output_configs}
    frame_count = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # Convert to grayscale once per source frame
        if len(frame.shape) == 3:
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        else:
            gray = frame

        for output_path, width, height in output_configs:
            frame_bytes = pack_1bit_frame(gray, width, height)
            output_buffers[output_path].extend(frame_bytes)

        frame_count += 1

        if frame_count % 50 == 0:
            print(f"  Processed {frame_count}/{total_frames} frames")

    cap.release()

    print()
    for output_path, width, height in output_configs:
        data = output_buffers[output_path]
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "wb") as f:
            f.write(data)

        per_frame = len(data) // frame_count if frame_count else 0
        print(f"Output: {output_path}")
        print(f"  Resolution: {width}x{height}")
        print(f"  Frames: {frame_count}")
        print(f"  Total size: {len(data)} bytes")
        print(f"  Per frame: {per_frame} bytes")

    if frame_count and fps:
        print(f"Duration: {frame_count / fps:.1f}s @ {fps} FPS")

    return frame_count, {str(path): len(buf) for path, buf in output_buffers.items()}


def parse_resolution(value):
    """Parse resolution text in the form WIDTHxHEIGHT (for example: 128x64)."""
    cleaned = value.lower().replace("*", "x")
    parts = cleaned.split("x")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(
            f"Invalid resolution '{value}'. Use format WIDTHxHEIGHT (example: 128x64)."
        )

    try:
        width = int(parts[0])
        height = int(parts[1])
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"Invalid resolution '{value}'. Width and height must be integers."
        ) from exc

    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError(
            f"Invalid resolution '{value}'. Width and height must be positive."
        )

    return width, height


def build_output_configs(base_output_path, resolutions):
    """Build output file list from base output path and requested resolutions."""
    unique_resolutions = list(dict.fromkeys(resolutions))

    if len(unique_resolutions) == 1:
        width, height = unique_resolutions[0]
        return [(base_output_path, width, height)]

    output_configs = []
    for width, height in unique_resolutions:
        file_name = f"{base_output_path.stem}_{width}x{height}{base_output_path.suffix}"
        output_configs.append((base_output_path.with_name(file_name), width, height))

    return output_configs


def parse_args():
    parser = argparse.ArgumentParser(
        description="Extract MP4 frames and generate 1-bit binary data for OLED displays."
    )
    parser.add_argument(
        "--video",
        type=Path,
        default=Path("./videos/Video Project 4.mp4"),
        help="Input video path (default: ./videos/Video Project 4.mp4)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("./data/video.bin"),
        help=(
            "Output file path or base file path. "
            "If multiple sizes are requested, files are suffixed as _<width>x<height>."
        ),
    )
    parser.add_argument(
        "--size",
        action="append",
        type=parse_resolution,
        metavar="WIDTHxHEIGHT",
        help=(
            "Target size. Use multiple --size flags for multiple outputs. "
            "Example: --size 128x32 --size 128x64"
        ),
    )

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    video_path = args.video
    sizes = args.size if args.size else [(128, 32), (128, 64)]
    output_configs = build_output_configs(args.output, sizes)

    if video_path.exists():
        if len(output_configs) == 1:
            output_path, width, height = output_configs[0]
            extract_and_pack_frames(video_path, output_path, width, height)
        else:
            extract_and_pack_multi(video_path, output_configs)
    else:
        print(f"Error: {video_path} not found")
