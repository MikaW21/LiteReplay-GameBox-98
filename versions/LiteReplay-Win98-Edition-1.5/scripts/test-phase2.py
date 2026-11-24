from pathlib import Path
import subprocess,json,sys,tempfile
root=Path(__file__).resolve().parents[1]; logs=root/'validation';logs.mkdir(exist_ok=True);results=[]
def run(name,args):
 p=subprocess.run([str(a) for a in args],capture_output=True,text=True)
 (logs/(name+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
 results.append({'name':name,'returncode':p.returncode,'command':[str(a) for a in args]})
 return p.returncode==0
with tempfile.TemporaryDirectory() as d:
 for name in ['core_tests','options_monitor_tests','eq_tests','suite_tests','media_catalog_tests']:
  exe=Path(d)/name
  if run(name+'-build',['g++','-std=c++17','-O2','-UNDEBUG','-Wall','-Wextra','-Werror','-pthread','-I',root/'src',root/'tests'/(name+'.cpp'),'-o',exe]):
   run(name,[exe]+([root/'languages'] if name=='suite_tests' else []))
 run('python-tests',['python3','-m','unittest','discover','-s',root/'tests','-p','test_*.py','-v'])
(logs/'portable-results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
print(json.dumps({'commands':len(results),'failed':[r['name'] for r in results if r['returncode']]}))
sys.exit(int(any(r['returncode'] for r in results)))
