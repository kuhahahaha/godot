This is a private receiver contract stub for owner integration. It does not spawn
processes, decode child payloads, issue verdicts, or create a fake native witness.
The E34 supervisor remains the sole process owner. Production integration requires
adding seal/EOF framing and parent-generated expectations at the owner seam.
