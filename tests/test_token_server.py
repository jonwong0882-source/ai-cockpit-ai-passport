import datetime as dt
import importlib.util
import json
import tempfile
from pathlib import Path
import threading
import unittest
import urllib.request
import urllib.error
from http.server import ThreadingHTTPServer

spec = importlib.util.spec_from_file_location('board_server', Path(__file__).parents[1]/'companion/server.py')
server = importlib.util.module_from_spec(spec); spec.loader.exec_module(server)


class BoardTests(unittest.TestCase):
    def test_calendar_boundaries(self):
        periods = server.periods(dt.date(2026, 3, 1))
        self.assertEqual(periods[3], (dt.date(2026, 2, 1), dt.date(2026, 2, 28)))
        self.assertEqual(periods[2], (dt.date(2026, 2, 16), dt.date(2026, 2, 22)))
        self.assertEqual(server.periods(dt.date(2024,3,1))[3][1], dt.date(2024,2,29))
        self.assertEqual(server.periods(dt.date(2026,1,1))[3][0], dt.date(2025,12,1))

    def test_sum_is_daily_not_cumulative(self):
        usage = {'dailyUsageBuckets': [{'startDate':'2026-10-11', 'tokens':20}, {'startDate':'2026-10-10','tokens':10}]}
        b = server.make_board(usage, {}, {}, dt.date(2026,10,12))
        self.assertEqual(b['periods'][0]['tokens'], 30)
        self.assertEqual(b['periods'][1]['tokens'], 20)
        self.assertEqual(b['periods'][0]['reported_days'], 2)
        self.assertIsNone(b['lifetime'])
        self.assertEqual(b['windows'], [])
        self.assertEqual(b['periods'][0]['target'], 0)

    def test_history_window_and_missing_zero(self):
        today=dt.date(2026,3,1)
        start=today-dt.timedelta(days=50)
        usage={'dailyUsageBuckets':[{'startDate':str(start),'tokens':0},
                                    {'startDate':'2026-02-28','tokens':200},
                                    {'startDate':'2026-03-01','tokens':999}]}
        h=server.make_board(usage,{}, {},today)['history']
        self.assertEqual(h['start'],str(start))
        self.assertEqual(h['end'],'2026-02-28')
        self.assertEqual(len(h['tokens']),50)
        self.assertEqual(h['tokens'][0],0)
        self.assertIsNone(h['tokens'][1])
        self.assertEqual(h['tokens'][-1],200)
        self.assertNotIn(999,h['tokens'])
        missing=server.make_board({}, {}, {},today)['history']
        self.assertEqual(missing['tokens'],[None]*50)

    def test_null_is_unknown(self):
        b = server.make_board({}, {}, {}, dt.date(2026,10,12))
        self.assertIsNone(b['periods'][1]['tokens'])
        self.assertIsNone(b['latest'])

    def test_invalid_and_duplicate_buckets(self):
        row = {'startDate':'2026-10-11','tokens':10}
        with self.assertRaises(ValueError): server.make_board({'dailyUsageBuckets':[row,row]}, {}, {})
        with self.assertRaises(ValueError): server.make_board({'dailyUsageBuckets':[dict(row,tokens=-1)]}, {}, {})

    def test_account_windows_are_not_token_counts(self):
        b = server.make_board({}, {'rateLimits':{'primary':{'windowDurationMins':10080,'usedPercent':18,'resetsAt':1792298737}}}, {})
        self.assertEqual(b['windows'][0]['used'], 18)
        self.assertIsNone(b['periods'][0]['tokens'])

    def test_completion_persistence_and_window(self):
        now=1791820800
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'quota-history.json'
            store=server.Store({'quota_history_file':str(path)})
            def board(used): return {'now':now,'windows':[{'bucket':'codex','minutes':10080,'used':used,'reset':now+100}]}
            b=board(80);store.record_quota(b)
            self.assertEqual(b['completion']['recorded'],0) # current cycle is excluded
            store.record_quota(board(20))
            self.assertEqual(store.quota_records[str(now+100)],80)
            loaded=server.Store({'quota_history_file':str(path)})
            self.assertEqual(loaded.quota_records,store.quota_records)
            self.assertEqual(path.stat().st_mode & 0o777,0o600)
            records={str(now-100):100,str(now-7*86400):50,str(now-31*86400):100,str(now+1):100}
            c=server.quota_completion(records,now)
            self.assertEqual(c['recorded'],2)
            self.assertEqual(c['average'],75)
            self.assertEqual(c['exhausted'],1)
            self.assertEqual(server.quota_completion({},now)['average'],None)
            path.write_text('{broken')
            with self.assertRaises(ValueError): server.Store({'quota_history_file':str(path)})
            self.assertEqual(path.read_text(),'{broken')

    def test_daily_quota_baseline_gap_reset_and_budget(self):
        now=int(dt.datetime(2026,10,12,12,tzinfo=server.TZ).timestamp())
        reset=now+2*86400
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'daily.json';store=server.Store({'quota_daily_file':str(path)})
            def board(stamp,used,end=reset):
                b=server.make_board({}, {}, {},dt.datetime.fromtimestamp(stamp,server.TZ).date())
                b['now']=stamp;b['windows']=[dict(bucket='codex',minutes=10080,used=used,reset=end)]
                return b
            b=board(now,50);store.record_daily(b)
            self.assertIsNone(b['periods'][4]['quota_percent']) # baseline is not consumption
            b=board(now+300,60);store.record_daily(b)
            self.assertEqual(store.daily_state['days']['2026-10-12'],10)
            # Yesterday budget is exactly one seventh of a weekly allowance.
            next_day=board(now+86400,60);store.record_daily(next_day)
            self.assertEqual(next_day['periods'][1]['quota_percent'],70)
            self.assertEqual(next_day['periods'][0]['quota_percent'],10)
            self.assertEqual(next_day['periods'][4]['quota_percent'],round(70/13,2))
            store.record_daily(board(now+86400+300,55))
            store.record_daily(board(now+86400+600,60))
            self.assertEqual(sum(store.daily_state['days'].values()),10) # correction does not double count
            store.record_daily(board(now+86400+2000,80)) # long gap skipped
            store.record_daily(board(now+86400+2300,5,reset+7*86400)) # new cycle baseline
            self.assertEqual(sum(store.daily_state['days'].values()),10)
            reloaded=server.Store({'quota_daily_file':str(path)})
            self.assertEqual(reloaded.daily_state,store.daily_state)
            self.assertEqual(path.stat().st_mode & 0o777,0o600)

    def test_http_auth_and_stale_cache(self):
        store = server.Store({'device_key':'a'*40})
        store.data = server.make_board({}, {}, {}, fetched_at=1)
        http = ThreadingHTTPServer(('127.0.0.1',0), server.handler(store))
        thread = threading.Thread(target=http.serve_forever, daemon=True);thread.start()
        url = f'http://127.0.0.1:{http.server_port}'
        try:
            with self.assertRaises(urllib.error.HTTPError) as ctx: urllib.request.urlopen(url+'/v1/board')
            self.assertEqual(ctx.exception.code,401)
            ctx.exception.close()
            req = urllib.request.Request(url+'/v1/board', headers={'Authorization':'Bearer '+'a'*40})
            value = json.load(urllib.request.urlopen(req))
            self.assertTrue(value['stale'])
            store.data = None
            with self.assertRaises(urllib.error.HTTPError) as ctx: urllib.request.urlopen(req)
            self.assertEqual(ctx.exception.code,503)
            ctx.exception.close()
        finally: http.shutdown();http.server_close();thread.join()


if __name__ == '__main__': unittest.main()
