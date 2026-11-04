#!/usr/bin/env python3
"""Independent exact failure controls; production source is never edited."""
from pathlib import Path
import json,subprocess,tempfile,hashlib
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
source=(ROOT/'drivers/metal/metal_buffer_size_table.h').read_text()
test=(HERE/'boundary_test.cpp').read_text().replace('"drivers/metal/metal_buffer_size_table.h"','"candidate.h"')
mutants=[
 ('size_table_overlaps_aux','sizes_offset = uint32_t(offset);','sizes_offset = uint32_t(offset) - 8;','table.sizes_offset==1431655760'),
 ('omit_dynamic_allocation_offset','const uint64_t address = p_buffer_address + p_allocation_offset + sizes_offset;','const uint64_t address = p_buffer_address + sizes_offset;','load64(data+8*n)==0x100000+256+8*n+8'),
 ('truncate_oversize_length','p_byte_length > std::numeric_limits<uint32_t>::max()','false','!t.write_length(data,0,uint64_t(UINT32_MAX)+1)')]
results=[]
with tempfile.TemporaryDirectory(prefix='b4-size-table-mutants-') as d:
 d=Path(d);cpp=d/'test.cpp';cpp.write_text(test)
 for name,before,after,diagnostic in mutants:
  assert source.count(before)==1
  candidate=source.replace(before,after)
  (d/'candidate.h').write_text(candidate)
  command=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(d),str(cpp),'-o',str(d/name)]
  compile_result=subprocess.run(command,capture_output=True,text=True)
  assert compile_result.returncode==0,compile_result.stderr
  run=subprocess.run([str(d/name)],capture_output=True,text=True,timeout=10)
  log=compile_result.stdout+compile_result.stderr+run.stdout+run.stderr
  expected_red=run.returncode!=0 and diagnostic in log
  (HERE/(name+'.log')).write_text(log)
  results.append({'name':name,'production_sha256':hashlib.sha256(source.encode()).hexdigest(),'before':before,'after':after,'mutant_sha256':hashlib.sha256(candidate.encode()).hexdigest(),'expected_diagnostic':diagnostic,'rc':run.returncode,'expected_red':expected_red,'log_sha256':hashlib.sha256(log.encode()).hexdigest()})
  assert expected_red,(name,log)
(HERE/'MUTATION_RESULTS.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results))
