# B4 create-config parser syntax verification

The six new `[ExternalOutputConfig]` doctest cases have **not run**. No new engine build was started. These checks validate translation-unit syntax only; they do not establish runtime assertions, pipeline/material eligibility, E09 source authority, real native draw/completion or GPU acceptance. `tests/SCsub` discovers the new test translation unit recursively; it supplies `TEST_FORCE_LINK(test_external_output_config)`.

`verification.json` records exact working directory, commands, exit codes, source hashes and raw-log hashes. The debug/tests, release-production and double/tests syntax checks all returned 0. The double check proves compilation only, not double-profile numeric qualification. Empty raw logs are retained.

`diff-check.log` records `git diff --no-index --check` separately for each owned new source/test file. Each returns 1 because the file differs from `/dev/null`; all three outputs are empty, so no whitespace diagnostics were found.

The parser is CPU syntax/projection only. Native storage still must resolve actual RID ownership, exact flattened surface/material count, qualified pipeline/material support and original source/native-use authority.
