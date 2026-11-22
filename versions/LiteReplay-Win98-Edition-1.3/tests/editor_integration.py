"""Exercise the exact C++ export argument builder with a host FFmpeg."""
import json
import array
import math
import pathlib
import subprocess
import sys
import tempfile

def run(args):
    return subprocess.run(args, check=True, capture_output=True)

def info(path):
    return json.loads(run(['ffprobe','-v','error','-show_streams','-show_format','-of','json',str(path)]).stdout)

with tempfile.TemporaryDirectory(prefix='litecut-test-') as directory:
    d = pathlib.Path(directory)
    a, b = d/'视频 & source.mp4', d/'silent.mp4'
    run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=s=640x360:r=30:d=3','-f','lavfi','-i','sine=frequency=600:duration=3','-c:v','mpeg4','-q:v','4','-c:a','aac','-shortest',str(a)])
    run(['ffmpeg','-v','error','-f','lavfi','-i','color=c=blue:s=320x240:r=24:d=3','-c:v','mpeg4','-q:v','4',str(b)])
    source_bytes = a.read_bytes()
    for idx, source in enumerate([a,b]):
        output=d/f'segment_{idx}.mp4'
        args=run([sys.argv[1],str(source),str(output),'1' if idx==0 else '0']).stdout.split(b'\0')[:-1]
        run(['ffmpeg']+[x.decode() for x in args])
        media=info(output)
        video=next(x for x in media['streams'] if x['codec_type']=='video')
        audio=next(x for x in media['streams'] if x['codec_type']=='audio')
        assert (video['width'],video['height'])==(1280,720)
        assert video['r_frame_rate']=='30/1'
        assert (audio['sample_rate'],audio['channels'])==('48000',2)
        assert abs(float(media['format']['duration'])-1.5)<.10
    (d/'list.txt').write_text("file 'segment_0.mp4'\nfile 'segment_1.mp4'\n")
    output=d/'final.mp4'
    run(['ffmpeg','-v','error','-nostdin','-n','-f','concat','-safe','1','-i',str(d/'list.txt'),'-map','0:v:0','-map','0:a:0','-c','copy','-movflags','+faststart',str(output)])
    media=info(output)
    assert abs(float(media['format']['duration'])-3)<.15
    def rms(path, start):
        raw=run(['ffmpeg','-v','error','-ss',str(start),'-i',str(path),'-t','0.5','-map','0:a:0','-ac','1','-f','f32le','-']).stdout
        samples=array.array('f'); samples.frombytes(raw)
        return math.sqrt(sum(x*x for x in samples)/len(samples))
    assert rms(output, 2.0)<.001
    ratio=rms(output, .3)/rms(a, .55)
    assert .28 < ratio < .55, ratio  # AAC + mono-to-stereo downmix; definitely attenuated.
    run(['ffmpeg','-v','error','-i',str(output),'-f','null','-'])
    assert a.read_bytes()==source_bytes
    # Overwrite refusal must leave an existing destination unchanged.
    before=output.read_bytes()
    args=run([sys.argv[1],str(a),str(output),'1']).stdout.split(b'\0')[:-1]
    attempt=subprocess.run(['ffmpeg']+[x.decode() for x in args],capture_output=True)
    assert output.read_bytes()==before, (attempt.returncode, attempt.stderr.decode())
    assert b"already exists" in attempt.stderr.lower() or attempt.returncode!=0
    print('PASS: mixed audio/no-audio sources, exact C++ args, trim/scale/concat, decode and overwrite protection')
