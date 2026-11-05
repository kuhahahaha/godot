# Explicit Metal precise functions

Metal MathModeSafe and MathFloatingPointFunctionsPrecise are separate options. The SDK defaults functions to Fast, and an actual original v2 three-variant run observed Safe0/functions0 before this change. On the existing available SDK/OS safe path, explicitly choose Precise and audit the actual getter. Preserve the older fastMathEnabled fallback and default-policy behavior.

The actual function selection now enters the existing private LE library-key encoding before lookup. The existing key test varies this new dimension and uses an independently encoded new golden. RED observer/header/test build fails that single dimension; GREEN distinguishes it. Existing stage-options assertions also require safe=Precise and default fast=Fast on the available API.

Actual main-Metal GREEN recompiles all three original integer-sqrt v2 static wrappers and creates every pipeline, with Safe0/functions1 at all three runtime compile boundaries. The same host passes 2082 engine cases / 521424 assertions. See DELIVERY.json for exact raw paths, options, keys and source/host identities.

This verifies the concrete option and cache-key correction. It does not qualify full Sprite output mathematics, old-OS equivalence, offline binaries or a final output-mapping unit. Historical Safe-only observations remain unchanged.
