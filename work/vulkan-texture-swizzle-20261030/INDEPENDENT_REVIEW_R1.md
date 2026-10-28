# Independent review — R19

The change adds four bounds checks at each Vulkan texture-view construction boundary and a real native regression that supplies each invalid channel independently. The RED run demonstrates the old code accepted invalid swizzle values; Debug, actual UBSan, focused Vulkan, K03/P2, and release checks are green after the guard. No fallback, owner authority, lifecycle association, or G1 behavior was introduced.
