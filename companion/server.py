#!/usr/bin/env python3
"""Read-only Codex account bridge. No prompts, turns, or raw auth are exported."""
import argparse
import calendar
import datetime as dt
import hmac
import json
import math
import os
from pathlib import Path
import queue
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
KEYS = ('recent_week', 'yesterday', 'last_week', 'last_month', 'month_to_date')
TZ = ZoneInfo('Asia/Shanghai')


def rpc_snapshot(binary='codex', timeout=40):
    proc = subprocess.Popen([binary, 'app-server'], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    inbox = queue.Queue()
    def read():
        for line in proc.stdout:
            try: inbox.put(json.loads(line))
            except ValueError: pass
        inbox.put(None)
    threading.Thread(target=read, daemon=True).start()
    def call(i, method, params=None):
        message = dict(id=i, method=method)
        if params is not None: message['params'] = params
        proc.stdin.write(json.dumps(message) + '\n'); proc.stdin.flush()
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0: raise TimeoutError('Codex account request timed out')
            item = inbox.get(timeout=remaining)
            if item is None: raise RuntimeError('Codex app-server exited')
            if item.get('id') == i:
                if 'error' in item: raise RuntimeError('Codex account request failed')
                return item['result']
    try:
        call(1, 'initialize', {'clientInfo': {'name': 'token_board', 'version': '0.1.0'}})
        proc.stdin.write('{"method":"initialized"}\n'); proc.stdin.flush()
        return call(2, 'account/usage/read'), call(3, 'account/rateLimits/read')
    finally:
        proc.terminate()
        try: proc.wait(timeout=5)
        except subprocess.TimeoutExpired: proc.kill(); proc.wait()
        proc.stdin.close(); proc.stdout.close()


def periods(today):
    yesterday = today - dt.timedelta(days=1)
    monday = today - dt.timedelta(days=today.weekday())
    month = today.replace(day=1)
    previous_end = month - dt.timedelta(days=1)
    return [(today-dt.timedelta(days=7), yesterday), (yesterday, yesterday),
            (monday-dt.timedelta(days=7), monday-dt.timedelta(days=1)),
            (previous_end.replace(day=1), previous_end), (month, today)]


def make_board(usage, limits, goals, today=None, fetched_at=None):
    today = today or dt.datetime.now(TZ).date()
    buckets = usage.get('dailyUsageBuckets')
    daily = {}
    if buckets is not None:
        for b in buckets:
            day = dt.date.fromisoformat(b['startDate'])
            value = b['tokens']
            if not isinstance(value, int) or value < 0: raise ValueError('Invalid token bucket')
            if day in daily: raise ValueError('Duplicate daily bucket')
            daily[day] = value
    rows = []
    for key, (start, end) in zip(KEYS, periods(today)):
        selected = {d:n for d,n in daily.items() if start <= d <= end}
        # Missing dates are not silently asserted to be zero usage.
        rows.append(dict(key=key, start=str(start), end=str(end),
                         tokens=sum(selected.values()) if buckets is not None else None,
                         reported_days=len(selected), days=(end-start).days+1,
                         target=goals.get(key, 0)))
    windows = []
    all_limits = limits.get('rateLimitsByLimitId') or {'codex': limits.get('rateLimits') or {}}
    for key, group in all_limits.items():
        for slot in ('primary', 'secondary'):
            w = group.get(slot)
            if w:
                windows.append(dict(bucket=key, minutes=w['windowDurationMins'],
                                    used=w['usedPercent'], reset=w.get('resetsAt')))
    history_start = today - dt.timedelta(days=50)
    history_end = today - dt.timedelta(days=1)
    history = dict(start=str(history_start), end=str(history_end),
                   tokens=[daily.get(history_start+dt.timedelta(days=i)) for i in range(50)])
    summary = usage.get('summary') or {}
    return dict(version=1, source='OpenAI account/usage/read', timezone='Asia/Shanghai',
                bucket_timezone='unspecified_by_service', fetched_at=fetched_at or int(time.time()),
                now=int(time.time()), stale=False, latest=str(max(daily)) if daily else None,
                periods=rows, windows=windows, history=history,
                lifetime=summary.get('lifetimeTokens'), streak=summary.get('currentStreakDays'),
                longest_streak=summary.get('longestStreakDays'), peak=summary.get('peakDailyTokens'))


def quota_completion(records, now):
    rows = [dict(reset=int(reset), used=value) for reset,value in records.items()
            if now-30*86400 < int(reset) <= now]
    rows.sort(key=lambda row: row['reset'])
    rows = rows[-5:]
    return dict(weeks=rows, average=round(sum(row['used'] for row in rows)/len(rows),1) if rows else None,
                recorded=len(rows), exhausted=sum(row['used']>=100 for row in rows),
                basis='observed_peak_not_final_settlement')


def quota_periods(board, daily):
    for row in board['periods']:
        values=[v for date,v in daily.items() if row['start']<=date<=row['end']]
        row['quota_reported_days']=len(values)
        row['quota_percent']=round(sum(values)*7/row['days'],2) if values else None
        row['quota_points']=round(sum(values),2) if values else None


def atomic_private_json(path,value):
    temporary=path.with_suffix('.tmp')
    fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
    with os.fdopen(fd,'w') as stream: json.dump(value,stream)
    os.chmod(temporary,0o600);os.replace(temporary,path)


class Store:
    def __init__(self, config):
        self.config = config
        self.lock = threading.Lock()
        self.data = None
        self.error = None
        self.quota_records = {}
        self.quota_path = Path(config['quota_history_file']) if config.get('quota_history_file') else None
        if self.quota_path and self.quota_path.exists():
            try:
                raw=json.loads(self.quota_path.read_text())
                if not isinstance(raw,dict): raise ValueError('Invalid quota history')
                for key,value in raw.items():
                    if not key.isdigit() or not 0 < int(key) <= 4102444800 or isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or not 0 <= value <= 100:
                        raise ValueError('Invalid quota history')
                self.quota_records=raw
            except (OSError,ValueError):
                raise ValueError('Quota history unreadable; preserve and repair the history file')
        self.daily_path=Path(config['quota_daily_file']) if config.get('quota_daily_file') else None
        self.daily_state={'sample':None,'days':{}}
        if self.daily_path and self.daily_path.exists():
            try:
                state=json.loads(self.daily_path.read_text())
                if not isinstance(state,dict) or not isinstance(state.get('days'),dict): raise ValueError('Invalid daily history')
                for date,value in state['days'].items():
                    dt.date.fromisoformat(date)
                    if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or value<0: raise ValueError('Invalid daily history')
                sample=state.get('sample')
                if sample is not None and (not isinstance(sample,dict) or not isinstance(sample.get('time'),int) or not isinstance(sample.get('reset'),int) or not isinstance(sample.get('used'),(int,float)) or not math.isfinite(sample['used']) or not 0<=sample['used']<=100): raise ValueError('Invalid daily sample')
                self.daily_state=state
            except (OSError,ValueError): raise ValueError('Daily quota history unreadable; preserve and repair it')
    def record_daily(self,board):
        now=board['now'];previous=self.daily_state['sample']
        cutoff=str(dt.datetime.fromtimestamp(now,TZ).date()-dt.timedelta(days=90))
        days={date:value for date,value in self.daily_state['days'].items() if date>=cutoff}
        sample=previous
        for window in board['windows']:
            reset,used=window.get('reset'),window.get('used')
            if window['bucket']!='codex' or window['minutes']!=10080 or not isinstance(reset,int) or not now<reset<=now+8*86400 or not isinstance(used,(int,float)) or not math.isfinite(used) or not 0<=used<=100: continue
            date=str(dt.datetime.fromtimestamp(now,TZ).date())
            if previous and previous['reset']==reset and 0<now-previous['time']<=900 and str(dt.datetime.fromtimestamp(previous['time'],TZ).date())==date:
                days[date]=days.get(date,0)+max(0,used-previous['used'])
            sample=dict(time=now,reset=reset,used=max(previous['used'],used) if previous and previous['reset']==reset else used)
            break
        state=dict(sample=sample,days=days)
        if state!=self.daily_state and self.daily_path: atomic_private_json(self.daily_path,state)
        self.daily_state=state
        quota_periods(board,days)
    def record_quota(self,board):
        now=board['now']
        records={key:value for key,value in self.quota_records.items() if int(key)>now-90*86400}
        for window in board['windows']:
            reset,used=window.get('reset'),window.get('used')
            if window['bucket']=='codex' and window['minutes']==10080 and isinstance(reset,int) and now < reset <= now+8*86400 and isinstance(used,(int,float)) and not isinstance(used,bool) and math.isfinite(used) and 0<=used<=100:
                key=str(reset);records[key]=max(records.get(key,0),used)
        if records!=self.quota_records and self.quota_path:
            temporary=self.quota_path.with_suffix('.tmp')
            fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
            with os.fdopen(fd,'w') as stream: json.dump(records,stream)
            os.chmod(temporary,0o600);os.replace(temporary,self.quota_path)
        self.quota_records=records
        board['completion']=quota_completion(records,now)
    def refresh(self):
        try:
            usage, limits = rpc_snapshot(self.config['codex_binary'])
            board = make_board(usage, limits, self.config.get('goals', {}))
            self.record_quota(board)
            self.record_daily(board)
            with self.lock: self.data = board; self.error = None
        except Exception:
            with self.lock: self.error = 'upstream_unavailable'
    def view(self):
        with self.lock:
            if self.data is None: return None
            value = dict(self.data)
            value['now'] = int(time.time())
            if 'completion' in value: value['completion']=quota_completion(self.quota_records,value['now'])
            value['stale'] = bool(self.error) or time.time()-value['fetched_at'] > 600
            return value
    def loop(self):
        while True:
            self.refresh()
            time.sleep(300)


def handler(store):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_): pass
        def answer(self, code, value):
            blob = json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode()
            self.send_response(code); self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(blob)))
            self.end_headers(); self.wfile.write(blob)
        def do_GET(self):
            if self.path == '/health':
                return self.answer(200, {'status': 'ok', 'upstream_ready': store.view() is not None})
            supplied = self.headers.get('Authorization', '')
            if not hmac.compare_digest(supplied, 'Bearer '+store.config['device_key']):
                return self.answer(401, {'error': 'unauthorized'})
            if self.path != '/v1/board': return self.answer(404, {'error': 'not_found'})
            data = store.view()
            if data is None: return self.answer(503, {'error': 'upstream_unavailable'})
            self.answer(200, data)
    return Handler


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--config', type=Path, default=ROOT/'.local/server.json')
    p.add_argument('--once', action='store_true')
    args = p.parse_args()
    config = json.loads(args.config.read_text())
    if len(config.get('device_key', '')) < 32: raise SystemExit('Missing device key')
    config.setdefault('quota_history_file',str(args.config.with_name('quota-history.json')))
    config.setdefault('quota_daily_file',str(args.config.with_name('quota-daily.json')))
    store = Store(config)
    if args.once:
        store.refresh()
        value = store.view()
        if value is None: raise SystemExit('Upstream unavailable; sign in to Codex on this Mac')
        print(json.dumps(value, ensure_ascii=False, indent=2)); return
    threading.Thread(target=store.loop, daemon=True).start()
    server = ThreadingHTTPServer(('127.0.0.1', 18741), handler(store))
    server.serve_forever()


if __name__ == '__main__': main()
