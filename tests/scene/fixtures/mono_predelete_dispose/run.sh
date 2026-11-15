#!/usr/bin/env bash
set -euo pipefail

fixture_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$fixture_dir/../../../.." && pwd)"
godot_bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"

dotnet build "$fixture_dir/MonoPredeleteDispose.csproj" --configuration Debug -p:BuildProjectReferences=false
mkdir -p "$fixture_dir/.godot/mono"
"$godot_bin" --headless --path "$fixture_dir" --quit-after 120 2>&1 | tee "$fixture_dir/.godot/mono/predelete-dispose.log"
GODOT_CHECK_CI_LOG_ALL_ERRORS=1 python3 "$repo_dir/misc/scripts/check_ci_log.py" "$fixture_dir/.godot/mono/predelete-dispose.log"
python3 - "$fixture_dir/.godot/mono/predelete-dispose.log" <<'CHECK_RESULT'
from pathlib import Path
import sys
assert "MONO_PREDELETE_DISPOSE_OK" in Path(sys.argv[1]).read_text(), "C# runtime fixture did not finish"
CHECK_RESULT
