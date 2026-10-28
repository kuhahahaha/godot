#!/usr/bin/env python3
import difflib,hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent
SDK=Path('/Users/macbookair/Documents/Work/ANGE/godot-cpp');R9=ROOT/'work/sdk-sticky-image-proof-20261028-r9';R10=ROOT/'work/sdk-sticky-source-landed-20261028-r10'
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
commit=subprocess.check_output(['git','-C',str(SDK),'rev-parse','HEAD'],text=True).strip()
files=['include/godot_cpp/classes/wrapped.hpp','include/godot_cpp/godot.hpp','include/godot_cpp/core/class_db.hpp','src/godot.cpp','src/classes/wrapped.cpp','src/core/class_db.cpp']
rows=[]
for path in files:
 data=(SDK/path).read_bytes();blob=subprocess.check_output(['git','-C',str(SDK),'show',commit+':'+path]);assert data==blob
 rows.append({'sdk_relative_path':path,'path':str(SDK/path),'bytes':len(data),'sha256':sha(data),'matches_adopted_commit_blob':True})
pre='a3d64535f2a53595ba653d678e21f45de4116665';pairs=[]
with tempfile.TemporaryDirectory(prefix='k03-identity-') as td:
 temp=Path(td)
 for path in ['include/godot_cpp/classes/wrapped.hpp','src/godot.cpp']:
  p=temp/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(subprocess.check_output(['git','-C',str(SDK),'show',pre+':'+path]))
 subprocess.run(['git','apply',str(R9/'SDK.patch')],cwd=temp,check=True)
 for path in ['include/godot_cpp/classes/wrapped.hpp','src/godot.cpp']:
  private=(temp/path).read_bytes();current=(SDK/path).read_bytes()
  pairs.append({'path':path,'r9_reconstruction':'original SDK '+pre+' plus retained R9 SDK.patch','private_bytes':len(private),'private_sha256':sha(private),'adopted_bytes':len(current),'adopted_sha256':sha(current),'byte_identical':private==current})
  (OUT/(Path(path).name+'-r9-vs-adopted.diff')).write_text(''.join(difflib.unified_diff(private.decode().splitlines(True),current.decode().splitlines(True),fromfile='r9-private/'+path,tofile='adopted/'+path)))
assert pairs[0]['byte_identical'] and not pairs[1]['byte_identical']
# Direct current-source R10 pins close the comment-different R9 identity boundary.
m10=json.loads((R10/'SOURCE_MANIFEST.json').read_text());matches=[]
for row in rows:
 old=m10.get(row['path']);matches.append({'path':row['path'],'r10_recorded_sha256':old,'adopted_sha256':row['sha256'],'exact':old==row['sha256']})
assert all(x['exact'] for x in matches)
archive=SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a';recorded=m10[str(archive)]
artifact={'path':str(archive),'r10_linked_archive_sha256':recorded,'current_sha256':sha(archive.read_bytes()),'matches_complete_adopted_sdk':'not proven; R10 directly compiles adopted godot.cpp/Wrapped.cpp and links this archive only for supporting TUs'}
save(OUT/'IDENTITY.json',{'adopted_sdk_commit':commit,'sdk_files':rows,'r9_private_comparison':pairs,'r10_direct_source_matches':matches,'archive':artifact,'canonical_products':'complete adopted SDK Debug/Release archives and canonical ANGE DSOs remain pending original A owner build/acceptance','missing_r9_pin':'R9 copied src/classes/wrapped.cpp directly but did not separately retain its hash; do not infer exact R9 copied-TU identity from that receipt. R10 includes that file in its before/after pin set and exactly matches the adopted commit.','no_historical_evidence_rewritten':True,'no_runtime_repeated':True})
print('ADOPTED',commit,'R9 Wrapped byte-exact; R9 godot.cpp comment delta retained; R10 all six exact')
