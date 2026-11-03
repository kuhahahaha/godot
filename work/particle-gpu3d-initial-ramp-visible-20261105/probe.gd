extends SceneTree
const OUT := "/Users/macbookair/Documents/Work/godot_ANGE/work/particle-gpu3d-initial-ramp-visible-20261105"
var observations:Array[Dictionary]=[]
func _initialize()->void: call_deferred("run")
func wait_frames(n:int)->void:
	for i in n: await process_frame
	await RenderingServer.frame_post_draw
func ramp_with(c:Color)->GradientTexture1D:
	var g:=Gradient.new(); g.set_color(0,c); g.set_color(1,c); var t:=GradientTexture1D.new(); t.gradient=g; return t
func capture(label:String)->Dictionary:
	var image:=root.get_texture().get_image(); var red:=0; var green:=0; var blue:=0
	for y in image.get_height():
		for x in image.get_width():
			var p:=image.get_pixel(x,y)
			if p.r>.5 and p.g<.25 and p.b<.25: red+=1
			if p.g>.5 and p.r<.25 and p.b<.25: green+=1
			if p.b>.5 and p.r<.25 and p.g<.25: blue+=1
	var saved:=image.save_png(OUT.path_join(label+".png"))==OK; var row={"stage":label,"red":red,"green":green,"blue":blue,"saved":saved}; observations.append(row); print("GPU3D_INITIAL_RAMP_STAGE %s red=%d green=%d blue=%d saved=%s"%[label,red,green,blue,str(saved)]); return row
func run()->void:
	var world:=Node3D.new(); root.add_child(world)
	var camera:=Camera3D.new(); camera.projection=Camera3D.PROJECTION_ORTHOGONAL; camera.size=3.0; camera.position=Vector3(0,0,5); camera.current=true; world.add_child(camera)
	var material:=StandardMaterial3D.new(); material.shading_mode=BaseMaterial3D.SHADING_MODE_UNSHADED; material.vertex_color_use_as_albedo=true
	var mesh:=QuadMesh.new(); mesh.size=Vector2(.20,.20); mesh.material=material
	var p:=GPUParticles3D.new(); p.amount=8; p.lifetime=1.0; p.fixed_fps=30; p.use_fixed_seed=true; p.seed=12345; p.visibility_aabb=AABB(Vector3(-1,-1,-1),Vector3(2,2,2)); p.draw_pass_1=mesh
	var mat:=ParticleProcessMaterial.new(); mat.gravity=Vector3.ZERO; mat.initial_velocity_min=0.0; mat.initial_velocity_max=0.0
	var old:=ramp_with(Color.RED); mat.color_initial_ramp=old; p.process_material=mat; p.emitting=true; world.add_child(p)
	await wait_frames(60); var initial:=capture("initial_red"); old.gradient.set_color(0,Color.GREEN); old.gradient.set_color(1,Color.GREEN); await wait_frames(20); var updated:=capture("updated_green")
	var replacement:=ramp_with(Color.BLUE); mat.color_initial_ramp=replacement; await wait_frames(20); var replaced:=capture("replaced_blue"); old.gradient.set_color(0,Color.YELLOW); old.gradient.set_color(1,Color.YELLOW); await wait_frames(20); var detached:=capture("detached_old_modified")
	var passed:bool=initial.red>0 and initial.green==0 and updated.green>0 and replaced.blue>0 and detached.blue==replaced.blue and detached.red==0
	for row in observations: passed=passed and row.saved
	print("GPU3D_INITIAL_RAMP_RESULT renderer=%s driver=%s passed=%s observations=%s"%[RenderingServer.get_current_rendering_method(),RenderingServer.get_current_rendering_driver_name(),str(passed),JSON.stringify(observations)]); quit(0 if passed else 1)
