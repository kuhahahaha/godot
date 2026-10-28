#!/usr/bin/env python3
"""Independent Float144 direct mapping expectation; actual decoder is renderer source."""
from pathlib import Path
import json,math,struct
P=Path(__file__).resolve().parent
expected=[(.2,.3,.4,0),(0,.6,.8,0),(1,0,0,0),(0,-.8,.6,0),(-.1,.2,.3,0),(.8,0,.6,0),(0,1,0,0),(-.6,0,.8,0)]
rows={}
for k in ['mobile','clustered']:
 data=(P/'runtime'/f'{k}.readback.bin').read_bytes();assert len(data)==128
 values=list(struct.iter_unpack('<4f',data))
 checks=[all(math.isfinite(a) and abs(a-b)<1e-5 for a,b in zip(row,e)) for row,e in zip(values,expected)]
 rows[k]={'values':values,'expected':expected,'vectors_match':checks,'pass':all(checks)}
(P/'DECODER_CHECK.json').write_text(json.dumps(rows,indent=2)+'\n')
print({k:v['vectors_match'] for k,v in rows.items()})
raise SystemExit(0 if all(r['pass'] for r in rows.values()) else 1)
