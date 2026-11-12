# Cross-platform test compilation repair

Commit `e5de6a06dc49af6807fd974a128d390413fd5213` repairs diagnostics observed in workflow 42366982660 at `acd6310a0b84ff80ea90cec106f4a0e9ec7a855c`. The original static job passed; Android/Clang builds rejected aggregate omissions for synchronization members, Linux minimal and TSan also rejected RecordSource omissions, and MSVC rejected two local variables hiding a member.

The repair supplies explicit default initializers for the existing mutex, condition variable and semaphore members and renames the shadowing locals. No compiler diagnostic is suppressed. Touching RecordSource also required whole-file clang-format 21.1.7; independent review reconstructed the three-line semantic repair and verified formatter output exactly. The Sprite two-check addition is excluded from this CI commit.

The old two translation units were extracted from the actual failed revision and both failed local strict syntax checks with the corresponding diagnostics. Both repaired units passed with `-Werror=missing-field-initializers`, `-Werror=shadow`, and `-Werror=shadow-uncaptured-local`. Exact commands and outputs are archived here.

The combined CI-repair plus Sprite-row source subsequently built on macOS (exit 0, 12.02 seconds), with focused CPU 18/4,604 and combined CPU 217/90,846 all passing. Those are combined-source results, not an isolated CI-only host. Remote successor results remain separate; these local checks do not certify MSVC, Android, TSan or the full remote matrix.
