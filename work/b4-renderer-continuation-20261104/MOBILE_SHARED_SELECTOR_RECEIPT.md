# Mobile shared native surface selector receipt

Behavior-preserving extraction: actual draw and future pre-list preparation use the same native shader/material/cull/specialization/pass tuple selector. Existing set_as_used timing, shadow override isolation, indexed backing, vertex format and ubershader fallback remain in original draw flow.

Default Sol implementation and independent scoped Sol review clear. Focused MobileNativePassSelection cases ran within NATIVE_CONTINUATION_NEIGHBORS.log: combined 9 cases / 2932 assertions pass, exit 0. New test uses independent literal specialization words. Coherent final build exit 0; format/diff checks clean. Final binary SHA256 3cbc9e04a659128008bd22d4ea63e814212c87fda48c6f987b404b4d89293aba.

No native pre-list caller or GPU/visible Sprite qualification claimed. Full Task3 integration still requires Task2 tuple bank and original capture/adoption.
