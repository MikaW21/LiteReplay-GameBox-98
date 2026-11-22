"""Generate LiteReplay's original two-note save notification; no third-party audio."""
import math
from pathlib import Path
import struct
import wave

def generate(path):
    rate = 44100
    samples = []
    for i in range(int(rate * .42)):
        t = i / rate
        value = 0.0
        for start, frequency in [(0, 880), (.15, 1174.66)]:
            age = t - start
            if 0 <= age < .24:
                envelope = min(1, age / .008) * math.exp(-age * 18) * min(1, (.24-age)/.02)
                value += .26 * envelope * math.sin(2 * math.pi * frequency * age)
        samples.append(round(32767 * value))
    with wave.open(str(path), 'wb') as f:
        f.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        f.writeframes(struct.pack('<' + 'h' * len(samples), *samples))
if __name__ == '__main__':
    generate(Path(__file__).resolve().parents[1] / 'bin/windows-x64/save.wav')
