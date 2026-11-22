import importlib.util
import pathlib
import unittest
p = pathlib.Path(__file__).resolve().parents[1] / 'scripts' / 'verify_replay.py'
spec = importlib.util.spec_from_file_location('verify', p)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    return {'streams': [{'index': 0, 'codec_type': 'video'}, {'index': 1, 'codec_type': 'audio'}],
            'packets': [{'stream_index': i, 'pts_time': str(t), 'dts_time': str(t),
                         'duration_time': '0.1', 'flags': 'K' if t == 0 else ''}
                        for t in [0, 0.1, 0.2] for i in [0, 1]]}


class VerifyTests(unittest.TestCase):
    def test_valid(self):
        self.assertFalse(mod.validate(fixture())[0])

    def test_bad_key(self):
        f = fixture(); f['packets'][0]['flags'] = ''
        self.assertTrue(mod.validate(f)[0])

    def test_bad_dts(self):
        f = fixture(); f['packets'][2]['dts_time'] = '0'
        self.assertTrue(mod.validate(f)[0])

    def test_empty_audio(self):
        f = fixture(); f['packets'] = [p for p in f['packets'] if p['stream_index'] == 0]
        self.assertTrue(mod.validate(f)[0])

    def test_av_offset(self):
        f = fixture()
        for p in f['packets']:
            if p['stream_index'] == 1:
                p['pts_time'] = str(float(p['pts_time'])+1)
                p['dts_time'] = p['pts_time']
        self.assertTrue(mod.validate(f)[0])

    def test_nan(self):
        f = fixture(); f['packets'][0]['pts_time'] = 'nan'
        self.assertTrue(mod.validate(f)[0])


if __name__ == '__main__':
    unittest.main()
