# SceneOnly profile routing

The internal particles_is_external_scene_only(RID) query reads the original immutable owner.config.profile==SCENE_ONLY. Mobile and Clustered use it in both geometry collection and instance-binding phases, preserving dependency/dirty completion. Instance192/Indexed144 are not classified from is_external or zero surfaces. Public render-profile admission and unresolved surface/read grants are unchanged.

Independent source review found no source blocker. Centralized actual macOS arm64 editor build exit0; component test [SceneOnlyRouting] selected1case/15assertions exit0, included with IncomingReadback in14cases/2096assertions exit0. Broad original combination206cases/87716assertions exit0; three expected preexisting pipeline-factory refusal diagnostics.

Initial component test crashed before any assertions because ShaderRD requires an existing RenderingDevice singleton; the test now creates RenderingDevice before component storage and destroys it afterward. No production workaround was added.

Editor SHA25679dfbd35beebec50cd6559aa5ae6c9a4bd4195fcd1516bc05580a9f2deb4ee92, source build before commit at version stamp60c5098c4. Evidence: work/readback-v2-publication-native-20261107/{INCOMING_R5,COMBINED_R5}.xml and work/k06-owner-interface-20261107/SCENE_ONLY_REVIEW.md. This CPU component test proves the profile projection; actual Scene cull plus geometry-renderer integration remains separately owned and is not claimed.
