# Post-integration runtime probe — 2026-10-30

Current editor source commit: `84c9b2ee7d`; editor SHA `fb8693bd5c159a724793b17f23581b1fdc2e1ba9875c0195834883ec79cc736a`; stable DSO SHA `0e1750c7ef509096201c849116d28ae385bd429ee914e4adebbd8960f0284041`.

The real non-headless Metal probe returned rc 2. First error: `host.storage_owner_transport/E05/UNAVAILABLE`. Extension marker: `True`; world marker: `False`; unload marker: `False`.

The source integration does not bypass the E05 owner seam; no submit, completion, readback, Ready, or G1 authority is claimed.
