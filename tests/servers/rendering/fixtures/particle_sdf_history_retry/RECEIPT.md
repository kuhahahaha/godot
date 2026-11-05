# SDF trail history survives rejected native particle frames

SDF screen updates overwrite colliders across retained trail history. On an upload failure, reversing the history shift restored only the first and last full frames; middle colliders kept the rejected screen transform. Preserve only their collider records before mutation and restore them before the existing inverse shift and trail resampling.

Actual main-Metal RED failed 11 targeted history/parameter checks, with all reads and physical returns still complete. The same fixture and oracle then passed all 362 checks after the minimal production fix: original 292 plus 70 explicit additions, original six plus two SDF asynchronous readbacks, 30 allocation identities and 26 native resource returns. Both runs closed without exit leaks; exact deliberately injected errors remain in the raw logs. The final combined engine host passed 2,082 cases / 521,421 assertions.

This is native retry/lifecycle evidence. It does not qualify SDF collision numerics, an ANGE whole producer, full Sprite output or whole-output release.
