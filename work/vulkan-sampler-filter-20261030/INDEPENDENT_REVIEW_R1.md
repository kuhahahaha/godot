# Independent review — R20

The change validates all three sampler filter fields before Vulkan sampler creation and avoids reading malformed enum values as typed enums by copying their storage bytes. The RED run demonstrates the previous silent fallback, the intermediate UBSan run captures the invalid-enum hazard, and the final Debug/UBSan/release evidence is green with no sanitizer diagnostics. No fallback, lifecycle authority, or G1 behavior was introduced.
