# Sprite preparation rows receipt

Commit `15d8884529` adds finite metadata preparation rows for the existing external output owner. Rows are placed by the same metadata planner, charged for every slot×surface binding, bound only after the original accepted acquire has reserved the slot, and checked against generation, lease serial, and exact row address. Duplicate binding and out-of-range access reject. Unopened rows roll back on an acquire failure; OBSERVE clears settled rows before the surface directory is destroyed.

The final macOS build exited 0 in 12.02 seconds. The focused CPU run executed 18 cases and 4,604 assertions; the combined run executed 217 cases and 90,846 assertions; both had zero failures. These runs validate metadata layout, acquire identity, duplicate/out-of-range rejection, cancellation/OBSERVE reset, and existing neighboring lifecycle tests.

This slice does not claim a renderer pass opener, pipeline/material/geometry creation, guard/seal publication, view issuer, checked draw, visible pixels, or zero-follow-up-draw closure. The unavailable public gate and ABI remain unchanged.
