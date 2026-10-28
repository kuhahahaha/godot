extends SceneTree
func _initialize():
 var info=Engine.get_version_info()
 var expected=JSON.parse_string(FileAccess.get_file_as_string("res://expected.json"))
 if not info.has("controlled_build_identity"):
  print("E06_IDENTITY_MISSING")
  quit(81)
  return
 var identity=info.controlled_build_identity
 expected.schema=int(expected.schema)
 var exact=identity==expected
 var closed=identity.keys().size()==7
 var original=info.major==4 and info.has("minor") and info.has("string") and info.has("hash") and info.has("timestamp")
 print("E06_LOADED_IDENTITY=",JSON.stringify(identity))
 print("E06_CHECK exact=",exact," closed=",closed," readonly=",identity.is_read_only()," original_fields=",original)
 quit(0 if exact and closed and identity.is_read_only() and original else 82)
