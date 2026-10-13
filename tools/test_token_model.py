#!/usr/bin/env python3
"""Compile the actual JSON parser against the selected IDF's pinned cJSON."""
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('board_server',root/'companion/server.py')
server=importlib.util.module_from_spec(spec);spec.loader.exec_module(server)
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='token-model-') as tmp:
    tmp=Path(tmp)
    usage={'dailyUsageBuckets':[{'startDate':'2026-10-11','tokens':20},{'startDate':'2026-10-10','tokens':10}]}
    fixture=tmp/'fixture.json';fixture.write_text(json.dumps(server.make_board(usage,{}, {},dt.date(2026,10,12))))
    binary=tmp/'test'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
                    '-I'+str(root/'main'),'-I'+str(cjson),str(root/'tests/test_token_model.c'),
                    str(root/'main/token_model.c'),str(cjson/'cJSON.c'),'-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary),str(fixture)],check=True)
