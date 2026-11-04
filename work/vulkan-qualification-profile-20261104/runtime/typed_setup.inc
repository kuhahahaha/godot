 GDExtensionQualificationSourceV1 body_source_identity{};
 body_source_identity.session_serial=request.qualification_session_serial;body_source_identity.generation=request.record_generation;
 memcpy(body_source_identity.digest,request.body_map_digest,32);
 GDExtensionQualificationTextureCreateV1 tq{};tq.abi=abi;tq.source=body_source_identity;tq.library_generation=library_generation;tq.create_attempt=1;tq.width=2;tq.height=2;tq.depth=1;tq.texture_type=2;tq.format=1;tq.usage=1;
 require(invoke_rt(bridge,12,&tq,sizeof(tq),nullptr,0,&texture,sizeof(texture)).native_error,"typed_texture_create");
 check(texture.state==1 && texture.kind==1 && texture.resource.handle_serial && texture.rid && texture.object_serial && texture.byte_size==64 && !memcmp(&texture.source,&body_source_identity,sizeof(body_source_identity)),"original_typed_texture");
 GDExtensionQualificationResourceBackingV1 replay_resource{};
 require(invoke_rt(bridge,12,&tq,sizeof(tq),nullptr,0,&replay_resource,sizeof(replay_resource)).native_error,"typed_texture_replay");check(!memcmp(&texture,&replay_resource,sizeof(texture)),"texture_exact_replay");
 GDExtensionQualificationSamplerCreateV1 sq{};sq.abi=abi;sq.source=body_source_identity;sq.library_generation=library_generation;sq.create_attempt=1;sq.repeat_u=sq.repeat_v=sq.repeat_w=2;
 require(invoke_rt(bridge,13,&sq,sizeof(sq),nullptr,0,&sampler,sizeof(sampler)).native_error,"typed_sampler_create");
 check(sampler.state==1 && sampler.kind==2 && sampler.resource.handle_serial && sampler.rid && sampler.object_serial && !memcmp(&sampler.source,&body_source_identity,sizeof(body_source_identity)),"original_typed_sampler");
 require(invoke_rt(bridge,13,&sq,sizeof(sq),nullptr,0,&replay_resource,sizeof(replay_resource)).native_error,"typed_sampler_replay");check(!memcmp(&sampler,&replay_resource,sizeof(sampler)),"sampler_exact_replay");
 const float pixels[16]={11,2,3,1,23,5,7,1,37,11,13,1,53,17,19,1};
 GDExtensionQualificationTextureUploadV1 image_upload{};image_upload.abi=abi;image_upload.ticket=ticket;image_upload.texture=texture.resource;image_upload.record_generation=request.record_generation;image_upload.parent_serial=preparation.parent_serial;image_upload.object_serial=texture.object_serial;image_upload.expected_revision=revision;image_upload.byte_size=sizeof(pixels);CC_SHA256(pixels,sizeof(pixels),image_upload.payload_digest);
 GDExtensionQualificationUploadSegmentV1 image_segments[2]{};GDExtensionQualificationUploadReportV1 image_report{};
 auto invalid_image=image_upload;invalid_image.payload_digest[0]^=1;
 auto denied=invoke_rt(bridge,14,&invalid_image,sizeof(invalid_image),pixels,sizeof(pixels),&image_report,sizeof(image_report),image_segments,2);
 check(denied.native_error==ERR_INVALID_DATA && !image_report.known_count && !image_report.revision,"bad_pixel_digest_no_acceptance");
 uint32_t image_busy_returns=0;
 for(uint32_t pass=0;pass<5000;++pass){
  auto returned=invoke_rt(bridge,14,&image_upload,sizeof(image_upload),pixels,sizeof(pixels),&image_report,sizeof(image_report),image_segments,2);
  if(returned.native_error==ERR_BUSY && returned.reason==19){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
  require(returned.native_error,"typed_texture_upload");
  check(image_report.state==1 || image_report.state==4,"texture_recording_not_failed_unknown");
  if(image_report.state==1)break;
  ++image_busy_returns;image_upload.expected_revision=image_report.revision;
 }
 check(image_report.state==1 && image_report.upload_index==UINT32_MAX && image_report.known_count==2 && image_report.out_written==2 && image_report.appended_end==64 && image_report.unappended_begin==64 && !image_report.native_error,"texture_full_recording_acceptance");
 for(uint32_t row=0;row<2;++row){const auto &s=image_segments[row];check(s.state==1 && s.upload_index==UINT32_MAX && s.segment_index==row && s.backing_serial==texture.object_serial && s.byte_begin==row*32 && s.byte_end==(row+1)*32 && s.graph_instance && s.graph_frame,"actual_texture_rows");}
 GDExtensionQualificationUploadReportV1 image_replay{};GDExtensionQualificationUploadSegmentV1 replay_segments[2]{};
 require(invoke_rt(bridge,14,&image_upload,sizeof(image_upload),pixels,sizeof(pixels),&image_replay,sizeof(image_replay),replay_segments,2).native_error,"texture_upload_exact_replay");
 check(!memcmp(&image_report,&image_replay,sizeof(image_report)) && !memcmp(image_segments,replay_segments,sizeof(image_segments)),"texture_upload_replay_same_segments");
 revision=image_report.revision;
 for(uint32_t phase=0;phase<2;++phase){for(uint32_t set_index=0;set_index<2;++set_index){
  GDExtensionQualificationUniformBindingV1 bindings[3]{};
  uint32_t binding_count=set_index==0?3:1;
  for(uint32_t n=0;n<binding_count;++n){uint32_t index=set_index==0?n:3;bindings[n].binding=n;bindings[n].type=1;bindings[n].resource={backings[index].backing.registry_instance,backings[index].backing.handle_serial};bindings[n].rid=backings[index].rid;bindings[n].object_serial=backings[index].backing_serial;}
  if(phase==1 && set_index==1){binding_count=2;auto &x=bindings[1];x.binding=1;x.type=4;x.resource=sampler.resource;x.rid=sampler.rid;x.object_serial=sampler.object_serial;x.secondary=texture.resource;x.secondary_rid=texture.rid;x.secondary_serial=texture.object_serial;}
  GDExtensionQualificationSetCreateV1 q{};q.abi=abi;q.source=body_source_identity;memcpy(q.source.digest,phase==0?request.capture_map_digest:request.body_map_digest,32);q.library_generation=library_generation;q.create_attempt=1;q.ticket=ticket;q.unit=units[phase].unit;q.shader_rid=units[phase].shader_rid;q.shader_serial=units[phase].shader_serial;q.shader_set=set_index;q.binding_count=binding_count;
  if(phase==1 && set_index==1){
   auto wrong=q;wrong.source.digest[0]^=1;wrong.create_attempt=99;
   GDExtensionQualificationResourceBackingV1 refused{};
   auto bad=invoke_rt(bridge,15,&wrong,sizeof(wrong),bindings,binding_count*sizeof(bindings[0]),&refused,sizeof(refused));
   check(bad.native_error==ERR_UNAUTHORIZED && !refused.resource.handle_serial && !refused.rid,"typed_wrong_phase_source_refused");
   wrong=q;wrong.binding_count=1;wrong.create_attempt=98;
   bad=invoke_rt(bridge,15,&wrong,sizeof(wrong),bindings,sizeof(bindings[0]),&refused,sizeof(refused));
   check(bad.native_error==ERR_INVALID_DATA && !refused.resource.handle_serial && !refused.rid,"typed_missing_reflection_binding_refused");
   auto forged=bindings[1];bindings[1].secondary={};wrong=q;wrong.create_attempt=97;
   bad=invoke_rt(bridge,15,&wrong,sizeof(wrong),bindings,binding_count*sizeof(bindings[0]),&refused,sizeof(refused));bindings[1]=forged;
   check(bad.native_error!=OK && !refused.resource.handle_serial && !refused.rid,"typed_bare_texture_rid_refused");
  }
  require(invoke_rt(bridge,15,&q,sizeof(q),bindings,binding_count*sizeof(bindings[0]),&typed_sets[phase][set_index],sizeof(replay_resource)).native_error,"typed_set_create");
  auto &created=typed_sets[phase][set_index];check(created.state==1 && created.kind==3 && created.rid && created.object_serial && created.resource.handle_serial && !memcmp(&created.source,&q.source,sizeof(q.source)),"typed_original_phase_set");
  require(invoke_rt(bridge,15,&q,sizeof(q),bindings,binding_count*sizeof(bindings[0]),&replay_resource,sizeof(replay_resource)).native_error,"typed_set_replay");check(!memcmp(&created,&replay_resource,sizeof(created)),"set_exact_replay");sets[phase][set_index]=created.rid;
 }}
 request.capture_set=sets[0][0];request.capture_set1=sets[0][1];request.body_set=sets[1][0];request.body_set1=sets[1][1];
 std::fprintf(stderr,"TYPED_ADMITTED original_children=6 phase_sets=4 image_bytes=64 actual_row_segments=2 busy_suffix_returns=%u admission_is_gpu_evidence=0 binding_negatives=3 payload_digest_negative=1\n",image_busy_returns);
