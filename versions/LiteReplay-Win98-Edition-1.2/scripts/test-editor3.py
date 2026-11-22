from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
b=r/'validation/editor3';b.mkdir(exist_ok=True)
with (r/'validation/editor3-tests.log').open('w') as log:
 for name in ['audio_gain_tests','editor_progress_tests','core_tests','eq_tests','options_monitor_tests','suite_tests','media_catalog_tests','encoder_policy_tests','thumbnail_cache_tests','screenshot_geometry_tests','clipboard_dib_tests','editor_core_tests','editor_history_tests','editor_preview_tests','editor_render_args','editor_copy_args']:
  cmd=['g++','-std=c++17','-O2','-pthread','-I'+str(r/'src'),str(r/'tests'/f'{name}.cpp'),'-o',str(b/name)]
  subprocess.run(cmd,check=True,stdout=log,stderr=log)
  if not name.endswith('_args'):subprocess.run([str(b/name)]+([str(r/'languages')] if name=='suite_tests' else []),check=True,stdout=log,stderr=log)
  log.write('PASS '+name+'\n');log.flush()
 for script,arg in [('editor_integration.py','editor_render_args'),('editor_copy_integration.py','editor_copy_args')]:
  subprocess.run(['python3',str(r/'tests'/script),str(b/arg)],check=True,stdout=log,stderr=log)
 print('All portable and real-media tests passed')
