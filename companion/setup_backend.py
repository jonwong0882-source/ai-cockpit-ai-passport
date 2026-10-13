#!/usr/bin/env python3
"""Create private bridge config and optionally install a macOS login service."""
import argparse,json,os,plistlib,secrets,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--install',action='store_true');p.add_argument('--codex');p.add_argument('--config-dir',type=Path,default=ROOT/'.local');args=p.parse_args()
 args.config_dir.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(args.config_dir,0o700)
 file=args.config_dir/'server.json'
 if not file.exists():
  binary=args.codex or shutil.which('codex')
  if not binary:raise SystemExit('Codex not found. Install/sign in to Codex, or pass --codex /path/to/codex.')
  value=dict(device_key=secrets.token_hex(32),codex_binary=str(Path(binary).resolve()))
  fd=os.open(file,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
  with os.fdopen(fd,'w') as f:json.dump(value,f,indent=2)
 print('Private configuration ready:',file)
 if not args.install:return
 if sys.platform!='darwin':raise SystemExit('--install requires macOS; use server.py --config to run elsewhere.')
 label='com.ai-cockpit.backend';agent=Path.home()/'Library/LaunchAgents'/f'{label}.plist';agent.parent.mkdir(parents=True,exist_ok=True)
 if agent.exists():raise SystemExit('Login service already exists. Preserve it and use launchctl kickstart to restart.')
 config=dict(Label=label,ProgramArguments=[sys.executable,str(ROOT/'companion/server.py'),'--config',str(file.resolve())],WorkingDirectory=str(ROOT),RunAtLoad=True,KeepAlive=True,StandardOutPath=str(args.config_dir.resolve()/'server.log'),StandardErrorPath=str(args.config_dir.resolve()/'server.error.log'),EnvironmentVariables={'PATH':str(Path.home()/'.local/bin')+':/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin'})
 agent.write_bytes(plistlib.dumps(config));subprocess.run(['launchctl','bootstrap',f'gui/{os.getuid()}',str(agent)],check=True)
 print('Login service installed. Check http://127.0.0.1:18741/health. Only one bridge may use port 18741.')
if __name__=='__main__':main()
