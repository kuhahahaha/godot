#include "modules/p01_probe/p01_capture.h"
#include <cassert>
int main(){P01Capture c;c.begin(1);P01CaptureEvent q{0,1,1,2,3,4,0,1,1};assert(c.append(q));q.kind=2;assert(c.append(q));assert(c.seal());uint8_t b=0;assert(c.write_transport(&b,1,1));P01Capture d;d.begin(2);assert(!d.write_transport(&b,1,0));assert(d.has_transport_failure());}
