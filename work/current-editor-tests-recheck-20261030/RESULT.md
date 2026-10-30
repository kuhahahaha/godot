# Current editor selector recheck

The current tests-enabled editor SHA `37743639ba01d06aa9b43c5c21582a8d4683ecfcd0ec3ab5b9cbf5e4600bb7e6` was rerun against every selector used by the native long-goal ledger. All 15 selectors returned rc0: K03/P2 25/386, QualificationNative Metal 2/51, QualificationNative Vulkan 65/251, MetalCompletion 6/386, ParticlesReadback 2/5, E05 18/417, QualificationRt 6/458, TypedABI 12/356, SceneReady 2/52, Qualification 102/1896, device_exit 133/2619, QualificationNative 67/302, K03 44/1153, R02 9/115, and RenderingDevice 12/214.

The executable was produced by the module-on texture-import regression build (`f4b839b4a0`; build rc0). This package only refreshes current editor selector evidence; it does not claim the exact Metal pair, owner transport, operation/completion, readback, Ready, or G1.
