# Godot native producer caller gap — 2026-10-27

A source census confirms that the landed Godot native consumer path has no production producer caller for `MeshStorage::mesh_add_external_surface`: the symbol appears only in its declaration/definition, with no caller under `servers/` or `tests/` beyond the test declaration.

`RenderDataRD::set_camera_source()` is written by `RendererSceneRenderRD::render_scene()` from an existing camera tuple; external packet admission is consumed by Forward+ and Mobile renderer paths. The source census finds no authenticated U08/E09 producer or same-request association that can populate an external surface/operation packet.

This is a precise external owner boundary. It does not justify adding a synthetic producer or promoting consumer guards to operation/completion/readback/Ready/GPU authority.
