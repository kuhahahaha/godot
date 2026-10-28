# Independent review — linker flag parser repair

- `-S` and `-x` are handled as literal closed flags; no operand is consumed
  and no input is fabricated.
- The parser still rejects unsupported search modes, indirect inputs and
  unclassified operands fail-closed.
- The red tests demonstrate that each newly accepted flag was previously
  rejected for the exact intended reason.
- The release target now builds and starts `--version`/`--help`; this is build
  and startup evidence only.

Disposition: no P1/P2 finding. This work is independent of the E20 graph slice
and does not alter operation, completion, readback, Ready or G1 authority.
