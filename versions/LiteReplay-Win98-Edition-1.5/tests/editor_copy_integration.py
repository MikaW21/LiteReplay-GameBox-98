import hashlib,json,pathlib,subprocess,sys,tempfile

def run(args):return subprocess.run(args,check=True,capture_output=True)
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);source=p/'source.mp4';dest=p/'copy.mp4'
 run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=s=320x240:r=30:d=4','-f','lavfi','-i','sine=duration=4','-c:v','mpeg4','-g','30','-c:a','aac','-shortest',str(source)])
 before=hashlib.sha256(source.read_bytes()).digest()
 args=[x.decode() for x in run([sys.argv[1],str(source),str(dest)]).stdout.split(b'\0')[:-1]]
 run(['ffmpeg']+args)
 out=json.loads(run(['ffprobe','-v','error','-show_streams','-of','json',str(dest)]).stdout)
 assert next(s for s in out['streams'] if s['codec_type']=='video')['codec_name']=='mpeg4'
 run(['ffmpeg','-v','error','-i',str(dest),'-f','null','-'])
 assert hashlib.sha256(source.read_bytes()).digest()==before
 saved=dest.read_bytes();subprocess.run(['ffmpeg']+args,capture_output=True);assert dest.read_bytes()==saved
 print('PASS: exact C++ stream-copy arguments, output decode, codec unchanged and source/output protection; frame-accurate boundaries not claimed')
