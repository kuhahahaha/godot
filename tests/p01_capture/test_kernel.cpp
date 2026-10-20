#include "modules/p01_probe/p01_kernels.h"
#include <cassert>
int main(){P01KernelInput in{42};assert(p01_kernel_identity(in)==42);}
