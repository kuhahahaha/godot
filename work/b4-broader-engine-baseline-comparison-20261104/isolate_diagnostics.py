from pathlib import Path
import datetime,hashlib,json,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
hosts=[('old-a2',Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64'),'4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528','a2f009e6cd5251abea5a611eaf9edac50b8749b3'),('new-94b',ROOT/'work/b4-metal-native-build-20261104/attempt-03/host/editor.arm64','965cb0ef6e137dcf17298d504f404a6f53aaa0421d03d080ef36e9b5b9b24eb6','94b06dc4876d153492aa0c0e2f3457d2f5afc92e')]
# Expectations are frozen from the actual negative branches and explicit test assertions, not arbitrary stderr allowances.
jobs=[('texture-callback','[Qualification][T1] marked texture rejects driver callbacks before an ordinary prefix',None,'ERROR: Condition "texture && texture->qualification_return" is true. Returning: ERR_UNAUTHORIZED',2),('partial-pipeline','[Qualification][ChildCapacity] partial native unit returns its exact shader before cold archival',None,'ERROR: Condition "!pipeline.driver_id" is true. Returning: RID()',2),('empty-shader','[Qualification][ChildCapacity] definitive empty unit failure returns its history credit',None,'ERROR: Condition "!shader_id" is true. Returning: RID()',1),('malformed-utf8','policy_container [ShaderCompilePolicy]','Noncanonical and malformed UTF-8 names are rejected before reserialization','Unicode parsing error, some characters were replaced with � (U+FFFD): Invalid UTF-8 leading byte (ff)',1),('foreign-camera','[B4][E15] viewport camera source requires explicit C01 registration',None,'ERROR: Attempted to save a foreign camera source token.',3)]
h=HERE/'runs'/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-single-case-attribution');h.mkdir(parents=True)
rows=[]
for label,host,expected_sha,source in hosts:
 assert sha(host)==expected_sha
 for job,name,subcase,diagnostic,count in jobs:
  out=h/(label+'-'+job);out.mkdir();xml=out/'doctest.xml'
  command=[str(host),'--test','--reporters=xml','--out='+str(xml),'--no-colors=true','--no-skipped-summary=true','--case-sensitive=true','--test-case='+name]
  if subcase:command+=['--subcase='+subcase]
  r=subprocess.run(command,capture_output=True,text=True,timeout=120)
  (out/'stdout.log').write_text(r.stdout);(out/'stderr.log').write_text(r.stderr)
  parsed=ET.parse(xml).getroot();cases=parsed.findall('.//TestCase');stats=parsed.find('OverallResultsTestCases').attrib;assertions=parsed.find('OverallResultsAsserts').attrib
  payload=[line for line in r.stderr.splitlines() if line.startswith('ERROR:') or line.startswith('Unicode parsing error')]
  row={'host_label':label,'host_sha256':expected_sha,'canonical_source':source,'job':job,'name':name,'subcase':subcase,'command':command,'rc':r.returncode,'case_names':[c.attrib['name'] for c in cases],'stats':stats,'assertions':assertions,'expected_diagnostic':diagnostic,'expected_diagnostic_count':count,'observed_diagnostic_payload':payload,'attribution_pass':r.returncode==0 and len(cases)==1 and cases[0].attrib['name']==name and stats['successes']=='1' and stats['failures']=='0' and assertions['failures']=='0' and payload==[diagnostic]*count}
  rows.append(row);(h/'RESULTS.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False)+'\n');print(json.dumps({'host':label,'job':job,'assertions':assertions,'attribution_pass':row['attribution_pass']},ensure_ascii=False),flush=True)
  assert row['attribution_pass'],row
 files={str(p.relative_to(h)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in h.rglob('*') if p.is_file() and p.name!='FILES.json'};(h/'FILES.json').write_text(json.dumps(files,indent=2)+'\n')
print('ATTRIBUTION_DIRECTORY',h)
