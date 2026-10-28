# Author review

Default and HOT_RELOAD macOS profiles now share the existing per-thread ConstructInfo map; no ConstructInfo fields, source permissions or native issuer change. Guarded claimant is preserved across deinit acknowledgement, so native OS image retention cannot silently permit in-image reinitialization. Real extra dlopen retains an image only in the negative test DSO; production adds no loader service or retention handle. Real fresh unload/reload remains green. Explicit current-source omission test retains the original core fail-closed holder gate.

Official SDK checks and original scripted controls pass. Source-linked native controls do not prove complete matching primary archives/DSOs. Previous full Debug artifact predates the second refinement and must not be re-labelled current. Independent review remains pending.
