#include <cstdint>
#include <cstdio>
struct Base {
virtual uint32_t qualification_native_call_bound(uint32_t p_slots) const { return 0; }
virtual uint32_t qualification_native_call_bound_for_profile(uint32_t p_main_slots, uint32_t p_worker_slots, uint32_t p_frames) const {
	 if (!p_frames || p_frames > p_main_slots || p_worker_slots > UINT32_MAX - p_main_slots) {
		 return 0;
	 }
	 return qualification_native_call_bound(p_main_slots + p_worker_slots);
 }
};
struct Vulkan:Base {

};
int main(){Vulkan d; int bad=0;
{auto actual=d.qualification_native_call_bound(1);if(actual!=1u){std::printf("one: expected=1 actual=%u\n",actual);++bad;}}
{auto actual=d.qualification_native_call_bound(4096);if(actual!=4096u){std::printf("policy-limit: expected=4096 actual=%u\n",actual);++bad;}}
{auto actual=d.qualification_native_call_bound(4097);if(actual!=0u){std::printf("over-policy: expected=0 actual=%u\n",actual);++bad;}}
{auto actual=d.qualification_native_call_bound_for_profile(145,1595,145);if(actual!=1740u){std::printf("r12-profile: expected=1740 actual=%u\n",actual);++bad;}}
{auto actual=d.qualification_native_call_bound_for_profile(1,4095,1);if(actual!=4096u){std::printf("profile-limit: expected=4096 actual=%u\n",actual);++bad;}}
{auto actual=d.qualification_native_call_bound_for_profile(UINT32_MAX,1,1);if(actual!=0u){std::printf("overflow: expected=0 actual=%u\n",actual);++bad;}}
return bad?1:0;}
