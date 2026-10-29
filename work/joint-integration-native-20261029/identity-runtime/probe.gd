extends SceneTree
func _initialize():
 var info=Engine.get_version_info()
 var identity=info.get("controlled_build_identity",{})
 print("CHECKPOINT_CORE_IDENTITY=",JSON.stringify(identity))
 var valid=identity.get("schema",0)==1 and identity.get("compiler_route","")=="BuiltinGlslangV1" and identity.get("platform","")=="macos" and identity.get("architecture","")=="arm64" and identity.get("target_kind","")=="Executable" and identity.keys().size()==7 and identity.is_read_only()
 for name in ["core_stamp","compiler_stamp"]:
  var stamp=identity.get(name,"")
  valid=valid and stamp.length()==64 and stamp.is_valid_hex_number()
 print("CHECKPOINT_IDENTITY_VALID=",valid)
 quit(0 if valid else 81)
