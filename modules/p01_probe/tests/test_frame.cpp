#include "modules/p01_probe/p01_frame.h"
#include <cassert>
int main(){P01CaptureEvent a{1,1,1,2,3,4,5,6,7,0,4,0};assert(p01_validate_frame_event(a,nullptr));P01CaptureEvent b=a;b.sequence=1;assert(!p01_validate_frame_event(b,&a));b.queue_sequence=9;assert(p01_validate_frame_event(b,nullptr));b.queue_sequence=0;assert(!p01_validate_frame_event(b,nullptr));}
