from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
OLD=Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64')
OLD_SHA='4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(OLD)==OLD_SHA
h=HERE/'runs'/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-old-a2');h.mkdir(parents=True)
new=json.loads((ROOT/'work/b4-broader-engine-recheck-20261104/RESULTS.json').read_text());execute=next(r for r in new['rows'] if r['name']=='execute')
command=execute['command'].copy();command[0]=str(OLD)
command=[('--out='+str(h/'doctest.xml')) if c.startswith('--out=') else c for c in command]
r=subprocess.run(command,capture_output=True,text=True,timeout=120)
(h/'stdout.log').write_text(r.stdout);(h/'stderr.log').write_text(r.stderr)
xml=ET.parse(h/'doctest.xml').getroot();cases=[c.attrib['name'] for c in xml.findall('.//TestCase')]
summary=xml.find('OverallResultsTestCases').attrib;assertions=xml.find('OverallResultsAsserts').attrib
result={'host':str(OLD),'host_sha256':OLD_SHA,'canonical_source':'a2f009e6cd5251abea5a611eaf9edac50b8749b3','command':command,'rc':r.returncode,'case_count':len(cases),'case_names':cases,'case_summary':summary,'assertion_summary':assertions,'stderr_sha256':sha(h/'stderr.log'),'scope':'old-source CPU/doctest baseline, not source94b or live GPU','clean':False}
(h/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');shutil.copyfile(Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/BUILD_IDENTITY.json'),h/'BUILD_IDENTITY-old-a2.json')
print(json.dumps({'run_dir':str(h),'rc':r.returncode,'cases':len(cases),'summary':summary,'assertions':assertions,'stderr':r.stderr}))
