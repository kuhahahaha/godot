#!/usr/bin/env python3
"""Freeze an observed original compiler/SPIR-V/native-container pairing."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
P=Path(__file__).resolve().parent

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(4*1024*1024),b''):
            h.update(b)
    return {'bytes':p.stat().st_size,'sha256':h.hexdigest()}
command=json.loads((P/'COMPILE_NATIVE_COMMAND.json').read_text())
paths=[Path(command[0]),P/'input/generate.gd',P/'input/unit.json',Path(__file__)]
paths += [P/'input'/f'{kind}.{ext}' for kind in ('capture','body') for ext in ('comp','spv')]
before={str(p):sha(p) for p in paths}
metadata=json.loads((P/'input/unit.json').read_text())
pairs=json.loads((P/'ORIGINAL_COMPILER_PAIR.json').read_text())
for i,kind in enumerate(('capture','body')):
    assert sha(P/'input'/f'{kind}.comp')['sha256']==metadata['physical'][i]['source_digest']
    assert sha(P/'input'/f'{kind}.spv')['sha256']==pairs[i]['spirv_sha256']
with (P/'PAIRED_NATIVE_GENERATION.log').open('w') as log:
    run=subprocess.run(command,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=log,stderr=subprocess.STDOUT,timeout=60)
(P/'PAIRED_NATIVE_GENERATION.rc').write_text(str(run.returncode)+'\n')
assert run.returncode==0
text=(P/'PAIRED_NATIVE_GENERATION.log').read_text()
assert 'Metal ' in text and 'Apple M3 Pro' in text and 'ERROR:' not in text
after={str(p):sha(p) for p in paths}
assert before==after,'Compiler/core/source inputs changed during native generation'
compiler=Path(shutil.which('glslangValidator')).resolve()
receipt={'scope':'passive artifact provenance for this native substrate control; no operation/completion/U08 authority','recipe_digest':metadata['recipe_digest'],'unit_digest':metadata['unit_digest'],'profile_digest':metadata['profile_digest'],'required_compile_flags':metadata['required_compile_flags'],'core':before[command[0]],'glslang':{'path':str(compiler),**sha(compiler)},'command':command,'before':before,'after':after,'rc':run.returncode,'log':sha(P/'PAIRED_NATIVE_GENERATION.log'),'physical':[]}
for i,kind in enumerate(('capture','body')):
    receipt['physical'].append({'index':i,'identity':metadata['physical'][i]['identity'],'interface_digest':metadata['physical'][i]['interface_digest'],'source':sha(P/'input'/f'{kind}.comp'),'spirv':sha(P/'input'/f'{kind}.spv'),'container':sha(P/'input'/f'{kind}.bin')})
(P/'NATIVE_GENERATION_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
lines=['#pragma once','#include <array>','#include <cstdint>']
def array(name,digest):
    return 'inline constexpr std::array<uint8_t,32> '+name+'={'+','.join(str(x) for x in bytes.fromhex(digest))+'};'
lines.append(array('expected_generation_receipt',sha(P/'NATIVE_GENERATION_RECEIPT.json')['sha256']))
for i,physical in enumerate(receipt['physical']):
    for part in ('source','spirv','container'):
        lines.append(array(f'expected_{part}_{i}',physical[part]['sha256']))
lines += ['inline constexpr std::array<std::array<uint8_t,32>,2> expected_sources={expected_source_0,expected_source_1};','inline constexpr std::array<std::array<uint8_t,32>,2> expected_spirvs={expected_spirv_0,expected_spirv_1};','inline constexpr std::array<std::array<uint8_t,32>,2> expected_containers={expected_container_0,expected_container_1};']
(P/'expected_artifacts.h').write_text('\n'.join(lines)+'\n')
print('Observed native generation rc0; exact original source/SPIR-V, actual compiler/core and native containers frozen.',flush=True)
