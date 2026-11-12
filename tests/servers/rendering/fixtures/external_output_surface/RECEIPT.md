# External output RenderingServer surface

Bind the six original methods and return their storage result directly inside the render thread. Wrong-thread calls return ERR_UNAVAILABLE before storage access, without queued work or a cross-thread wait. Existing admission/source/Host requirements remain enforced by storage.

Actual ClassDB probe: six missing methods before the change, none after. Three engine cases / 116 assertions verify exact bindings, RT forwarding, unsupported-backend refusal without RID allocation, and a real worker's immediate refusal with a subsequent queue drain. RenderingServer XML is byte-identical to the actual doctool output. The combined source host also passed 2,082 cases / 521,421 assertions. This interface delivery does not supply the remaining production issuer or renderer joins.
