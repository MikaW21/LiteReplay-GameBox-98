import json,pathlib,subprocess,sys,tempfile

def run(args):return subprocess.run(args,check=True,capture_output=True)
with tempfile.TemporaryDirectory() as temp:
 d=pathlib.Path(temp)
 for size,rate,tag in [('640x360','30000/1001',None),('360x640','24','LiteReplayRecordingV1;width=360;height=640;fps=24;mbps=2')]:
  source=d/(size+'.mp4');out=d/(size+'-out.mp4');probe=d/'probe.txt'
  args=['ffmpeg','-v','error','-f','lavfi','-i',f'testsrc2=s={size}:r={rate}:d=2','-c:v','mpeg4','-b:v','2M']
  if tag:args+=['-metadata','comment='+tag]
  run(args+[str(source)])
  entries='format=duration,bit_rate:format_tags=comment:stream=codec_type,width,height,avg_frame_rate,r_frame_rate,bit_rate:stream_side_data=rotation'
  probe.write_bytes(run(['ffprobe','-v','error','-show_entries',entries,'-of','default',str(source)]).stdout)
  a=run([sys.argv[1],str(source),str(probe),str(out)]).stdout.split(b'\0')[:-1]
  a=[x.decode() for x in a];assert '-maxrate' in a and '-b:v' in a and '-q:v' not in a
  run(['ffmpeg']+a)
  info=json.loads(run(['ffprobe','-v','error','-show_streams','-of','json',str(out)]).stdout)
  video=next(x for x in info['streams'] if x['codec_type']=='video')
  assert (video['width'],video['height'])==tuple(map(int,size.split('x')))
  assert video['r_frame_rate']==('30000/1001' if rate=='30000/1001' else '24/1')
  run(['ffmpeg','-v','error','-i',str(out),'-f','null','-'])
  if tag:assert a[a.index('-b:v')+1]=='2.000000M'
 print('PASS: actual source probe -> source-limited export -> decode; NTSC and portrait; recording bitrate tag')
