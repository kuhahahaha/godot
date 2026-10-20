#include "modules/p01_probe/p01_capture.h"
#include <cassert>
int main(){P01Capture c;c.begin(7);P01CaptureEvent q{0,7,1,2,3,9,0,4,1};assert(c.append(q));P01CaptureEvent bad=q;bad.kind=2;bad.queue_sequence=8;assert(!c.append(bad));assert(c.is_poisoned());bad.queue_sequence=9;assert(c.append(bad));assert(!c.seal());assert(c.dispose_physical());P01Capture d;d.begin(8);q.epoch=8;assert(d.append(q));q.kind=2;assert(d.append(q));assert(d.seal());assert(d.dispose_physical());}
