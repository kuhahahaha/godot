# Scene117 CI housekeeping receipt

Base: `71079130d7fc0fad0d9d6cdc0d32027cf41ebbe2`.

The actual remote workflow https://github.com/kuhahahaha/godot/actions/runs/42365142038 failed in the original gitignore gate before formatting or cross-platform builds. It found 168 previously tracked ignored evidence files (166 logs, two generated JSON files), all predating the current Scene117 batch.

The repair adds only exact rooted exceptions for those existing tracked files. Their 168 byte counts and SHA256 values are preserved in PRESERVED_EVIDENCE.json; arbitrary new logs remain ignored. The original `misc/scripts/gitignore_check.sh` passed in a fresh `git archive HEAD` export with the repaired ignore file, and `git ls-files -ci --exclude-standard` is empty.

The original full pre-commit hooks additionally found executable index modes for three shebang runners, four missing C++ copyright headers, whole-file formatting, and one documentation spelling. These were corrected. Four Python runners retain the same standard-library imports and executable AST (PYTHON_FORMAT_EQUIVALENCE.json). C++ changes are formatting, include ordering and copyright only; the separately developed Request wrong-thread test is excluded from this housekeeping commit.

PRECOMMIT_GREEN_EXIT.json records the actual command and exit 0 (3.39 seconds), after the initial failing run preserved in PRECOMMIT_FIRST_OUTPUT.txt. Applicable shebang, clang-format 21.1.7, Ruff 0.15.8, mypy 1.19.1, codespell, codeowner, include, copyright, guard and file-format hooks passed; nonmatching hooks were skipped. The checked set includes the Scene117 batch and the pending Request test. No cross-platform build or remote green is claimed by this local gate result.

Previously recorded GPU runs retain their original source/runner pins. This receipt does not relabel those runs as executions of the formatted scripts or updated headers. No new GPU run is required or claimed for these equivalent formatting changes.
