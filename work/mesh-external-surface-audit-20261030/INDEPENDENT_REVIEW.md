# Independent review

- Reviewed `ExternalSurfaceDescriptor`, `mesh_add_external_surface`,
  `validate_external_indexed_surface_buffers`, `_mesh_surface_generate_external_version`,
  and `_mesh_surface_clear` with line-level source evidence in `SOURCE_AUDIT.md`.
- Admission and partial publication are fail-closed. Borrowed buffer ownership is
  preserved through teardown; generated vertex arrays are retired on surface clear.
- The only uncovered edge is production reachability: no caller supplies a real
  external descriptor, owner, generation, or packet. Existing tests pass but do not
  establish a native or GPU owner.
- No source change was made; changing ownership or adding a caller would fabricate
  authority. Focused suites and current pair evidence are included.
