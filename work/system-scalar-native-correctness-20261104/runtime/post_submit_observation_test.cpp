#include "post_submit_observation.h"
#include <cassert>
#include <iostream>

struct Fact {
 uint64_t graph_instance=0, graph_frame=0, native_serial=0, completion_serial=0;
 uint32_t graph_kind=1, graph_index=0, graph_offset=0;
 uint32_t semantic_kind=0, command_slot=UINT32_MAX, backend_queue=0, issue=0, completion=0, error=0;
 int64_t native_code=0;
};
struct Report {uint32_t state=4, possible_count=0;int32_t native_error=0;int64_t native_code=0;};
uint32_t checks=0;
void check(bool value){++checks;assert(value);}
Fact native(uint32_t slot=3,uint32_t issue=1,uint32_t completion=2){Fact f;f.semantic_kind=5;f.command_slot=slot;f.native_serial=21;f.completion_serial=21;f.backend_queue=1;f.issue=issue;f.completion=completion;return f;}
Fact body(const Fact &n){Fact b=n;b.semantic_kind=2;b.graph_instance=1;return b;}
bool selected(const Report &r,const std::vector<Fact>&facts){Fact b,n;uint32_t index=UINT32_MAX;return SystemScalarPostSubmit::issued_body(r,facts,b,n,index);}
bool observe(const Report&r,const std::vector<Fact>&facts){std::vector<Fact>pending;return SystemScalarPostSubmit::observe_pending(r,facts,pending);}
int main(){
 Report report;Fact n=native();Fact b=body(n);
 check(selected(report,{b,n}));
 n.completion=1;check(selected(report,{body(n),n}));
 n=native();b=body(n);
 for(uint32_t field=0;field<4;field++){
  Fact wrong=n;
  if(field==0)wrong.command_slot++;
  if(field==1)wrong.native_serial++;
  if(field==2)wrong.completion_serial++;
  if(field==3)wrong.backend_queue++;
  check(!selected(report,{b,wrong}));
 }
 Fact hidden=native(UINT32_MAX);hidden.semantic_kind=4;
 check(!selected(report,{body(hidden),hidden}));
 Fact unsealed=b;unsealed.command_slot=UINT32_MAX;check(!selected(report,{unsealed,n}));
 check(!selected(report,{b}));
 Fact graph=n;graph.semantic_kind=3;check(!selected(report,{b,graph}));
 check(!selected(report,{b,b,n}));
 Fact impossible=n;impossible.semantic_kind=4;check(!selected(report,{body(impossible),impossible}));
 Fact no_graph=b;no_graph.graph_instance=0;check(!selected(report,{no_graph,n}));
 for(uint32_t bad_state:{5u,6u,7u}){report.state=bad_state;check(!selected(report,{b,n}));}
 report={};report.possible_count=1;
 Fact pending_native=native(UINT32_MAX,2,1);pending_native.semantic_kind=4;pending_native.native_serial=11;pending_native.completion_serial=11;pending_native.backend_queue=2;
 Fact pending_body=body(pending_native);
 std::vector<Fact>pending;
 for(uint32_t state:{2u,3u,4u}){report.state=state;check(SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{pending_body,pending_native},pending));check(!selected(report,{pending_body,pending_native}));}
 check(pending.size()==1&&pending[0].native_serial==11);
 for(uint32_t state:{5u,6u,7u,8u}){report.state=state;check(!observe(report,{pending_body,pending_native}));}
 report.state=2;report.possible_count=2;std::vector<Fact>transaction;
 check(!SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{pending_body,pending_native},transaction));check(transaction.empty());
 report.possible_count=1;
 Fact unknown=pending_native;unknown.completion=4;check(!observe(report,{pending_body,unknown}));
 for(uint32_t field=0;field<3;field++){
  Fact nameless=pending_native;
  if(field==0)nameless.native_serial=0;
  if(field==1)nameless.completion_serial=0;
  if(field==2)nameless.backend_queue=0;
  check(!observe(report,{pending_body,nameless}));
 }
 Fact failed=pending_native;failed.error=1;check(!observe(report,{pending_body,failed}));
 failed=pending_native;failed.native_code=4;check(observe(report,{pending_body,failed})); // raw diagnostic is not an Error
 check(!observe(report,{pending_body,pending_native,pending_native}));
 Fact refined=pending_native;refined.issue=1;refined.completion=2;report.possible_count=0;report.state=4;
 check(SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{body(refined),refined},pending));
 // A completed hidden-copy source still cannot qualify the unsealed Body.
 check(!selected(report,{body(refined),refined}));
 for(uint32_t field=0;field<4;field++){
  auto tracked=pending;Fact replaced=refined;
  if(field==0)replaced.completion_serial++;
  if(field==1)replaced.backend_queue++;
  if(field==2)replaced.command_slot=3;
  if(field==3)replaced.semantic_kind=5;
  check(!SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{body(replaced),replaced},tracked));
 }
 auto tracked=pending;check(!SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{b,n},tracked));
 Fact unproven=refined;unproven.completion=1;tracked=pending;
 check(!SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{body(unproven),unproven},tracked));
 // The same old hidden source remains held while a separate real Body source
 // is appended. Only the Body's actual command slot/native tuple qualifies.
 check(SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{b,n,refined},pending));
 check(selected(report,{b,n,refined}));
 report.state=8;check(selected(report,{b,n,refined}));
 report.native_error=1;check(!observe(report,{b,n,refined}));check(!selected(report,{b,n,refined}));
 report.native_error=0;report.native_code=1;check(!observe(report,{b,n,refined}));check(!selected(report,{b,n,refined}));
 report={};n=native();b=body(n);
 std::vector<Fact> final_facts{b,n,refined};
 check(SystemScalarPostSubmit::original_sources_returned(final_facts,pending,b,n));
 Fact later=native(6);later.native_serial=99;later.completion_serial=99;final_facts.push_back(later);
 check(SystemScalarPostSubmit::original_sources_returned(final_facts,pending,b,n));
 for(uint32_t field=0;field<9;field++){
  auto replaced=final_facts;
  if(field==0)replaced[0].graph_instance++;
  if(field==1)replaced[0].graph_frame++;
  if(field==2)replaced[0].graph_kind++;
  if(field==3)replaced[0].graph_index++;
  if(field==4)replaced[0].graph_offset++;
  if(field==5)replaced[0].command_slot++;
  if(field==6)replaced[0].native_serial++;
  if(field==7)replaced[0].completion_serial++;
  if(field==8)replaced[0].backend_queue++;
  check(!SystemScalarPostSubmit::original_sources_returned(replaced,pending,b,n));
 }
 for(uint32_t field=0;field<5;field++){
  auto replaced=final_facts;
  if(field==0)replaced[2].native_serial++;
  if(field==1)replaced[2].completion_serial++;
  if(field==2)replaced[2].backend_queue++;
  if(field==3)replaced[2].command_slot=3;
  if(field==4)replaced[2].semantic_kind=5;
  check(!SystemScalarPostSubmit::original_sources_returned(replaced,pending,b,n));
 }
 auto missing=final_facts;missing.erase(missing.begin()+2);check(!SystemScalarPostSubmit::original_sources_returned(missing,pending,b,n));
 missing=final_facts;missing.erase(missing.begin()+1);check(!SystemScalarPostSubmit::original_sources_returned(missing,pending,b,n));
 auto duplicate=final_facts;duplicate.push_back(n);check(!SystemScalarPostSubmit::original_sources_returned(duplicate,pending,b,n));
 duplicate=final_facts;duplicate.push_back(b);check(!SystemScalarPostSubmit::original_sources_returned(duplicate,pending,b,n));
 auto incomplete=final_facts;incomplete[1].completion=1;check(!SystemScalarPostSubmit::original_sources_returned(incomplete,pending,b,n));
 // Metal reports raw MTLCommandBufferStatus::Completed (4). Keep it intact.
 report={};n=native();n.native_code=4;b=body(n);refined.native_code=4;
 auto metal_pending=pending;
 check(SystemScalarPostSubmit::observe_pending(report,std::vector<Fact>{b,n,refined},metal_pending));
 check(n.native_code==4&&b.native_code==4&&refined.native_code==4);
 check(selected(report,{b,n,refined}));
 check(SystemScalarPostSubmit::original_sources_returned(std::vector<Fact>{b,n,refined},metal_pending,b,n));
 for(uint32_t completion:{3u,4u}){
  Fact bad=n;bad.completion=completion;
  check(!observe(report,{body(bad),bad,refined}));
  check(!selected(report,{body(bad),bad,refined}));
  check(!SystemScalarPostSubmit::original_sources_returned(std::vector<Fact>{body(bad),bad,refined},metal_pending,b,n));
 }
 Fact actual_error=n;actual_error.error=1;
 check(!observe(report,{body(actual_error),actual_error,refined}));
 check(!selected(report,{body(actual_error),actual_error,refined}));
 check(!SystemScalarPostSubmit::original_sources_returned(std::vector<Fact>{body(actual_error),actual_error,refined},metal_pending,b,n));
 for(uint32_t state:{5u,6u,7u}){report.state=state;check(!observe(report,{b,n,refined}));check(!selected(report,{b,n,refined}));}
 report.state=4;report.native_code=4;check(!observe(report,{b,n,refined}));check(!selected(report,{b,n,refined}));
 std::cout<<checks<<" post-submit original-source/Body tuple cases PASS (CPU only)\n";
}
