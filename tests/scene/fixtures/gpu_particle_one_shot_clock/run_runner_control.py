#!/usr/bin/env python3
"""Check host receipts reject incomplete workloads and accept complete clean markers."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    controls = [
        ('vacuous', {'cases': 0, 'checks': 0, 'frames': 0}, False),
        ('truncated', {'cases': 14, 'checks': 2746, 'frames': 99}, False),
        ('wrong-type', {'cases': 14.0, 'checks': 2774, 'frames': 100}, False),
        ('complete', {'cases': 14, 'checks': 2774, 'frames': 100}, True),
    ]
    passed = True
    with tempfile.TemporaryDirectory(prefix='gpu-clock-runner-control-') as temporary:
        directory = Path(temporary)
        for name, counts, expected_pass in controls:
            fake = directory / ('fake-' + name)
            marker = 'GPU_ONESHOT_ENGINE=' + json.dumps(dict(failures=[], **counts))
            fake.write_text('#!/usr/bin/env python3\nprint(' + repr(marker) + ')\n')
            fake.chmod(0o755)
            receipt_path = directory / (name + '.json')
            command = [sys.executable, str(HERE / 'run_host.py'), '--godot', str(fake), '--output', str(receipt_path)]
            run = subprocess.run(command, capture_output=True, text=True, timeout=30)
            receipt = json.loads(receipt_path.read_text())
            ok = (run.returncode == (0 if expected_pass else 1) and receipt['engine_exit'] == 0 and receipt['passed'] == expected_pass)
            if not expected_pass:
                ok &= any(error.startswith('Expected ') for error in receipt['errors'])
            passed &= ok
            print(json.dumps({'control': name, 'command': command, 'runner_exit': run.returncode, 'engine_exit': receipt['engine_exit'], 'errors': receipt['errors'], 'control_passed': ok}))
    print('runner controls: ' + ('PASS' if passed else 'FAIL'))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
