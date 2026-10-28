# Independent review — R21

The change adds index checks for all three sampler repeat modes before any Vulkan sampler creation. The RED run demonstrates the previous backend accepted the sentinel value; Debug, UBSan, qualification, K03/P2 and release evidence are green with no sanitizer diagnostics. No owner authority, fallback, lifecycle association or G1 behavior was introduced.
