# Ordinary viewport camera capture correction

Production fix is applied in the working tree. `production.patch` fixes only the
unregistered viewport branch. A normal viewport returns an absent token without
an error or ordinal increment. Registered invalid keys still error; UINT64_MAX
still errors before incrementing.

The complete function does not exist at HEAD 76c3b1010f. It belongs to the existing
uncommitted C01/E15 migration. This commit freezes the minimal patch and evidence,
not that unrelated migration. Integrator must apply the patch to its owning
baseline (identity.json records exact source hashes). It is already applied here.

Build (exit 0):
```
scons platform=macos target=editor dev_build=yes tests=yes bin/godot.macos.editor.dev.arm64 -j2
```

Visible regression, A fixture unchanged (exit 0; zero ERROR messages):
```
./bin/godot.macos.editor.dev.arm64 --display-driver macos --rendering-method mobile --rendering-driver vulkan --audio-driver Dummy --path /Users/macbookair/Documents/Work/ANGE/work/particle-integration-baseline-20261025 --script native_particle.gd
```
Observed Vulkan Apple M3 Pro and moved/visible/finished/freed=true. `finished`
is the node notification, not a GPU fence; instance free is not proof of GPU
physical memory retirement. The fixture writes its existing burst.png output.

Existing registration regression (exit 0, 1 case, 38 assertions):
```
./bin/godot.macos.editor.dev.arm64 --test --test-case='[B4][E15] viewport camera source requires explicit C01 registration'
```
Its three foreign-token errors are intentional negative cases. They are absent
from the visible scene log. Build remains based on the mixed working tree.
