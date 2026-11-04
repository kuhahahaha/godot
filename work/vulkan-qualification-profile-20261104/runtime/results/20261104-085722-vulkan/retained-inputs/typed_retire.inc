 uint64_t retired_events[6]{};uint32_t typed_acks=0;
 auto finish_retire=[&](const GDExtensionQualificationResourceBackingV1 &resource){
  GDExtensionQualificationResourceRetireV1 q{abi,resource.resource,resource.rid,resource.object_serial,700+typed_acks};
  GDExtensionQualificationResourceRetireReportV1 physical{},replay{};
  require(invoke_rt(bridge,16,&q,sizeof(q),nullptr,0,&physical,sizeof(physical)).native_error,"typed_retire");
  require(invoke_rt(bridge,16,&q,sizeof(q),nullptr,0,&replay,sizeof(replay)).native_error,"typed_retire_replay");
  check(replay.resource.handle_serial==resource.resource.handle_serial && replay.retire_operation==q.retire_operation,"typed_retire_original_replay");
  if(physical.state==0)check(typed_release(bridge,resource.resource,physical.revision).native_error==ERR_BUSY,"typed_pending_ack_refused");
  for(uint32_t pass=0;pass<5000 && physical.state==0;++pass){require(typed_observe(bridge,resource.resource,&budget,&physical).native_error,"typed_retire_observe");if(!physical.state)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(physical.state==1 && physical.return_status==1 && !physical.reason && !physical.native_error && !physical.native_code && physical.rid==resource.rid && physical.object_serial==resource.object_serial && physical.resource.registry_instance==resource.resource.registry_instance && physical.resource.handle_serial==resource.resource.handle_serial && physical.frame_epoch && physical.event_seq && physical.dependency_serial && physical.kind==resource.kind,"typed_actual_physical_return");
  check(typed_release(bridge,resource.resource,physical.revision-1).native_error==ERR_BUSY,"typed_stale_revision_ack_refused");
  for(uint32_t n=0;n<typed_acks;++n)check(retired_events[n]!=physical.event_seq,"typed_distinct_physical_free_events");retired_events[typed_acks++]=physical.event_seq;
  require(typed_release(bridge,resource.resource,physical.revision).native_error,"typed_physical_ack");
  check(typed_release(bridge,resource.resource,physical.revision).native_error!=OK,"typed_ack_once");
  std::fprintf(stderr,"TYPED_PHYSICAL kind=%u object=%llu frame_epoch=%llu event_seq=%llu dependency=%llu revision=%llu exact_ack=1\n",resource.kind,(unsigned long long)resource.object_serial,(unsigned long long)physical.frame_epoch,(unsigned long long)physical.event_seq,(unsigned long long)physical.dependency_serial,(unsigned long long)physical.revision);
 };
 for(const auto &phase:typed_sets)for(const auto &set:phase)finish_retire(set);
 finish_retire(texture);finish_retire(sampler);check(typed_acks==6,"all_typed_physical_acknowledgements");
