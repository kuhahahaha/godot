from pathlib import Path
import ctypes,hashlib,json,struct
HERE=Path(__file__).resolve().parent
lib=ctypes.CDLL('/opt/homebrew/lib/libzstd.dylib')
lib.ZSTD_decompress.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t];lib.ZSTD_decompress.restype=ctypes.c_size_t
rows=[]
for file in sorted(HERE.glob('*.metalbin')):
 data=file.read_bytes();off=data.find(bytes.fromhex('28b52ffd'));assert off>=16
 # ShaderHeader is 4 uint32s immediately before compressed code, per container to_bytes.
 stage,csize,flags,dsize=struct.unpack_from('<4I',data,off-16);assert stage==4 and flags&2==0 and 0<csize<=len(data)-off
 buf=ctypes.create_string_buffer(dsize);actual=lib.ZSTD_decompress(buf,dsize,data[off:off+csize],csize);assert actual==dsize,(actual,dsize)
 raw=buf.raw;extra=(off+csize+3)&~3
 # Metal StageData: 3 uint32, SHA256Digest[32], source_size, library_size...
 expected=data[extra+12:extra+44];source_size,library_size=struct.unpack_from('<2I',data,extra+44)
 assert source_size+library_size==dsize
 source=raw[:source_size];assert hashlib.sha256(source).digest()==expected
 out=file.with_suffix('.metal');out.write_bytes(source)
 text=source.decode();hits=[{'line':n,'text':line} for n,line in enumerate(text.splitlines(),1) if 'spvBufferSizeConstants' in line or '[[id(' in line]
 rows.append({'container':str(file),'container_sha256':hashlib.sha256(data).hexdigest(),'stage':stage,'compression_flags':flags,'compressed_size':csize,'source_size':source_size,'library_size':library_size,'source_sha256':expected.hex(),'source':str(out),'relevant_lines':hits})
 print(file.name,'source bytes',source_size,'hash verified',expected.hex())
(HERE/'MSL_EXTRACTION.json').write_text(json.dumps({'rows':rows,'actual_host_translation':True,'pipeline_compile_or_dispatch':False},indent=2)+'\n')
