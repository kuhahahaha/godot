# Independent review — Sprite/ExternalOutput adapter gap

Read-only review traced all current external packet/resource seams and their callers.

1. The packet seam is `RenderDataRD::PacketIdentity`; it only authenticates camera tuple and material generation/capability.
2. The only typed external geometry seam is `MeshStorage::ExternalSurfaceDescriptor`, with strict buffer RID/generation/range checks, but no public production caller and no texture/sampler adapter.
3. External texture storage is a separate proxy path and cannot authorize a draw packet's material/sampler resources.
4. Existing tests validate fail-closed external surface cache generation and packet admission, but do not create GPU authority or prove visible output.
5. Because the missing adapter requires an upstream owner/schema contract, a local implementation would be unsafe. This package therefore records an exact gap rather than adding a guessed bridge.
