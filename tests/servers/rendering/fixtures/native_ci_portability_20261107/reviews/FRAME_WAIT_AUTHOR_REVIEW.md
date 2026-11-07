# Frame-wait CI fixture repair

Status: source READY; syntax-only GREEN; runtime GREEN pending parent-controlled build/test.

Real RED: run 42371357955, TSan job 128400182259 and ASan job 128400182293. Preserved FIRST_FATAL excerpts in this directory. The TSan signal-unsafe reports are consequences of the bounds-fatal signal handler, not the original cause.

Root cause is an incomplete test lifecycle. First execute_chained_cmds() advances qualification_issue_slot from 0 to 1. The fake one-command pool has zero chain semaphores. record_next_graph() previously reset graph/primary state directly and bypassed RD::_begin_frame(), leaving issue_slot at 1. Second execute_chained_cmds() therefore treated the new graph as a resumed chain and read semaphores[0] before issuing anything. Production _begin_frame() resets the issue cursor and closed/busy epoch fields, while _stall_for_frame() deliberately does not open a new recording epoch.

Fix: enter actual RD::_begin_frame() before adding the second segment; stop faking graph/primary begin in record_next_graph(). A test-local ReuseDriver accepts only buffer_free(BufferID(61)), because real frame begin now disposes the existing synthetic retained buffer from attach(). Other tests retain their fail-fast base driver behavior. No production source, assertion removal, test skip, or dummy semaphore insertion.

The original same-buffer/same-slot/different-graph, serial 23/24, completed_size=4, one callback, and error=OK assertions remain. Removing the production cursor reset would still make this scenario fail; overwriting earlier graph issue facts would still fail the serial assertions.

Verification: git diff --check for the test file passed. SYNTAX_COMMAND.json contains the full clang++ -fsyntax-only invocation and exit 0; SYNTAX_OUTPUT.txt is empty. No full build, GPU run, git staging, commit, or push was performed. Existing binaries were not presented as current-source evidence.

After parent builds current sources with tests enabled, run:

```sh
bin/godot.macos.editor.arm64 --headless --test --test-case='*FrameWait*'
```

Then rerun the affected Linux ASan/TSan unit-test lanes; syntax verification does not certify runtime or sanitizer success.
