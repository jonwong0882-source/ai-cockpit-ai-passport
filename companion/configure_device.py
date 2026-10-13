#!/usr/bin/env python3
"""Prepare private API settings and write only NVS when explicitly requested."""
import argparse,csv,json,os,subprocess,sys
from pathlib import Path
from urllib.parse import urlsplit
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--config',type=Path,default=ROOT/'.local/server.json');p.add_argument('--url',required=True);p.add_argument('--port');p.add_argument('--prepare-only',action='store_true');p.add_argument('--confirm-reset-settings',action='store_true');args=p.parse_args()
 url=urlsplit(args.url)
 if url.scheme!='https' or not url.hostname or url.username or url.password or url.fragment or len(args.url.encode())>=256 or any(ord(c)<32 for c in args.url):raise SystemExit('Use your own HTTPS board URL, without credentials or fragments; max 255 bytes.')
 key=json.loads(args.config.read_text())['device_key']
 if not isinstance(key,str) or not 32<=len(key)<=80 or any(ord(c)<33 or ord(c)>126 for c in key):raise SystemExit('Invalid device key.')
 private=args.config.parent;private.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(private,0o700)
 source=private/'device-settings.csv';image=private/'device-settings.bin'
 fd=os.open(source,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
 with os.fdopen(fd,'w',newline='') as f:
  writer=csv.writer(f);writer.writerows([['key','type','encoding','value'],['token_board','namespace','',''],['api_url','data','string',args.url],['api_key','data','string',key]])
 subprocess.run([sys.executable,'-m','esp_idf_nvs_partition_gen','generate',str(source),str(image),'0x6000'],check=True,stdout=subprocess.DEVNULL)
 os.chmod(image,0o600)
 if args.prepare_only:print('Private device settings prepared; no device opened or reset.');return
 if not args.port or not args.confirm_reset_settings:raise SystemExit('No write: use --port and --confirm-reset-settings. This resets saved Wi-Fi and device settings; provision Wi-Fi afterward.')
 subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','--port',args.port,'--before','default_reset','--after','hard_reset','write_flash','--flash_size','8MB','0x9000',str(image)],check=True)
 print('API settings written. Connect Wi-Fi using the BLE setup menu.')
if __name__=='__main__':main()
