import importlib.util
import tempfile
import unittest
import wave
import struct
from pathlib import Path

class SoundTests(unittest.TestCase):
    def test_generated_pcm_is_short_nonclipping_and_fades(self):
        source = Path(__file__).resolve().parents[1] / 'scripts/make_sound.py'
        spec = importlib.util.spec_from_file_location('sound', source)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'sound.wav'
            module.generate(path)
            with wave.open(str(path), 'rb') as f:
                self.assertEqual((f.getnchannels(), f.getsampwidth(), f.getframerate()), (1, 2, 44100))
                self.assertAlmostEqual(f.getnframes()/f.getframerate(), .42, places=3)
                data = f.readframes(f.getnframes())
            samples = struct.unpack('<' + 'h' * (len(data)//2), data)
            self.assertGreater(max(map(abs, samples)), 1000)
            self.assertLess(max(map(abs, samples)), 32767)
            self.assertEqual(samples[0], 0)
            self.assertEqual(samples[-1], 0)
