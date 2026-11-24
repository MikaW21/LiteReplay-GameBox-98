#!/usr/bin/env python3
"""Validate container structure/timestamps. This does NOT measure lip-sync."""
import argparse
import json
import math
import subprocess
import sys


def validate(info, expected_audio=1, tolerance=0.25):
    streams = {s['index']: s for s in info.get('streams', [])}
    videos = [i for i, s in streams.items() if s['codec_type'] == 'video']
    audios = [i for i, s in streams.items() if s['codec_type'] == 'audio']
    errors = []
    if len(videos) != 1:
        errors.append(f'Expected 1 video track, found {len(videos)}')
    if len(audios) != expected_audio:
        errors.append(f'Expected {expected_audio} audio tracks, found {len(audios)}')
    times = {i: [] for i in streams}
    last_dts = {}
    for packet in info.get('packets', []):
        i = packet['stream_index']
        if i not in streams:
            errors.append('Packet references unknown stream')
            continue
        try:
            pts, dts = float(packet['pts_time']), float(packet['dts_time'])
            duration = float(packet.get('duration_time', 0))
            if not all(math.isfinite(v) for v in (pts, dts, duration)):
                raise ValueError('nonfinite timestamps')
        except (KeyError, TypeError, ValueError):
            errors.append(f'Missing or invalid timestamp in stream {i}')
            continue
        if dts < -0.001 or pts < -0.001:
            errors.append(f'Negative timestamp in stream {i}')
        if i in last_dts and dts <= last_dts[i]:
            errors.append(f'Non-increasing DTS in stream {i}')
        last_dts[i] = dts
        if not times[i] and i in videos and 'K' not in packet.get('flags', ''):
            errors.append('Video does not start on a keyframe')
        times[i].append((pts, pts + duration))
    ranges = {}
    for i, entries in times.items():
        if not entries:
            errors.append(f'Empty stream {i}')
        else:
            ranges[i] = (min(t[0] for t in entries), max(t[1] for t in entries))
    if len(videos) == 1 and videos[0] in ranges:
        vstart, vend = ranges[videos[0]]
        if vend <= vstart:
            errors.append('Video duration is not positive')
        for i in audios:
            if i in ranges:
                start, end = ranges[i]
                if abs(start-vstart) > tolerance or abs(end-vend) > tolerance:
                    errors.append(f'Stream {i} A/V endpoint difference exceeds {tolerance}s')
    return errors, ranges


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('file')
    ap.add_argument('--ffprobe', default='ffprobe')
    ap.add_argument('--audio-tracks', type=int, default=1, choices=[0, 1, 2])
    ap.add_argument('--tolerance', type=float, default=0.25)
    ap.add_argument('--json', dest='output')
    args = ap.parse_args()
    try:
        result = subprocess.run([args.ffprobe, '-v', 'error', '-show_streams', '-show_packets',
                                 '-show_format', '-of', 'json', args.file],
                                check=True, capture_output=True, text=True, encoding='utf-8', timeout=120)
        info = json.loads(result.stdout)
        errors, ranges = validate(info, args.audio_tracks, args.tolerance)
        report = {'file': args.file, 'errors': errors, 'stream_ranges_seconds': ranges,
                  'note': 'Container checks only; perceptual sync and full decode require separate tests.'}
        if args.output:
            with open(args.output, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)
        print(json.dumps(report, indent=2, ensure_ascii=False))
        return 1 if errors else 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
