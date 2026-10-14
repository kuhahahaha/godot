#!/usr/bin/env python3
"""Check explicit Godot FR PLAN files; 0=structural pass, 1=findings, 2=tool error.

Not a semantic/test-coverage validator. No recursive basename resolution and no
silent missing-source skips. Table/fence handling adapts ANGE fr_plan_check C13
(source SHA256 9733a24ef39e9a242e769e07ffdedeedf4b24bc73697e3e4f1c8f63ad1a56b68).
"""
import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote


REQUIRED = ('0. Decisions', 'Sources', 'Implementation', 'Tests', 'Example Impact',
            'Build and acceptance', 'File-change checklist', 'Review', 'Self-review')
LINK = re.compile(r'\[[^\]\n]+\]\((<[^>\n]+>|[^)\n]+)\)')


def defenced(text):
    result, fence = [], None
    for line in text.splitlines():
        match = re.match(r'^\s*(`{3,}|~{3,})', line)
        if match:
            token = match[1]
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence) and not line[match.end():].strip():
                fence = None
            result.append('')
        else:
            result.append('' if fence else line)
    return result


def local_targets(path, lines):
    """Explicit inline and reference-style links, outside fenced/inline code.

    This is a deliberately limited documentation checker, not a GFM parser.
    Sources use standard link destinations (angle brackets for spaces).
    """
    definitions, clean = {}, []
    for line in lines:
        line = re.sub(r'(`+).*?\1', '', line)
        match = re.match(r'^\s{0,3}\[([^]]+)\]:\s*(<[^>]+>|\S+)', line)
        if match:
            definitions[match[1].casefold()] = match[2]
            clean.append('')
        else:
            clean.append(line)
    for number, line in enumerate(clean, 1):
        destinations = [match[1] for match in LINK.finditer(line)]
        for match in re.finditer(r'\[([^]]+)\]\[([^]]*)\]', line):
            key = (match[2] or match[1]).casefold()
            if key in definitions:
                destinations.append(definitions[key])
        for destination in destinations:
            token = re.match(r'(<[^>]+>|\S+)', destination)
            raw = token[1].strip('<>') if token else ''
            if not raw or re.match(r'^[a-zA-Z][\w+.-]*://', raw) or raw.startswith('#'):
                continue
            raw = unquote(raw.split('#', 1)[0])
            location = re.fullmatch(r'(.*):(\d+)', raw)
            target = Path(location[1] if location else raw)
            target = (target if target.is_absolute() else path.parent / target).resolve()
            yield number, target, int(location[2]) if location else None


def check(path):
    lines = defenced(path.read_text(encoding='utf-8'))
    text = '\n'.join(lines)
    issues = []

    def fail(line, message):
        issues.append(f'{path}:{line}: {message}')

    if not path.name.endswith('_PLAN.md'):
        fail(1, 'expected *_PLAN.md')
    for heading in REQUIRED:
        if not re.search(r'^##\s+' + re.escape(heading) + r'(?:\s|$)', text, re.M):
            fail(1, 'missing section: ' + heading)
    if 'Success check' not in text:
        fail(1, 'missing phase Success check')
    pair = path.with_name(path.name.replace('_PLAN.md', '.md')).resolve()
    targets = set()
    for number, target, target_line in local_targets(path, lines):
        targets.add(target)
        if not target.exists():
            fail(number, 'missing target: ' + str(target))
        elif target_line is not None:
            if not target.is_file():
                fail(number, 'line target is not a file: ' + str(target))
            else:
                count = len(target.read_text(encoding='utf-8').splitlines())
                if not 1 <= target_line <= count:
                    fail(number, 'line out of range: ' + str(target) + ':' + str(target_line))
    if not pair.is_file():
        fail(1, 'missing paired FR: ' + str(pair))
    if pair not in targets:
        fail(1, 'paired FR must be linked')
    if pair.is_file():
        backlinks = {target for _, target, _ in local_targets(pair, defenced(pair.read_text(encoding='utf-8')))}
        if path not in backlinks:
            fail(1, 'paired FR must link back to PLAN')

    width, matrix, seen, test_rows, in_tests = None, False, set(), 0, False
    for number, line in enumerate(lines, 1):
        if re.match(r'^##\s+', line):
            in_tests = bool(re.match(r'^##\s+Tests(?:\s|$)', line))
        if not line.strip().startswith('|'):
            width, matrix = None, False
            continue
        cells = [c.strip() for c in re.split(r'(?<!\\)\|', line.strip().strip('|'))]
        if width is None:
            width = len(cells)
            matrix = in_tests and cells[0] == 'ID'
            if matrix and cells not in [
                ['ID', 'Trigger', 'Expected', 'Test / command', 'RED'],
                ['ID', 'Trigger', 'Expected', 'Test', 'command', 'RED'],
            ]:
                fail(number, 'test matrix requires Trigger/Expected/Test/command/RED columns')
            continue
        if all(re.fullmatch(r'[:\- ]+', cell) for cell in cells):
            continue
        if len(cells) > width:
            fail(number, 'table row wider than header; escape literal pipes')
        if matrix:
            if len(cells) != width or any(not c for c in cells):
                fail(number, 'test matrix row has missing/extra cells')
            case_id = cells[0]
            if not re.fullmatch(r'T\d+', case_id):
                fail(number, 'test matrix ID must be T followed by digits')
            if case_id in seen:
                fail(number, 'duplicate test ID: ' + case_id)
            seen.add(case_id)
            test_rows += 1
    if test_rows == 0:
        fail(1, 'no test matrix rows')
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('plans', nargs='+', type=Path)
    args = parser.parse_args()
    issues = []
    try:
        for path in args.plans:
            issues.extend(check(path.resolve()))
    except (OSError, UnicodeError) as error:
        print('tool error: ' + str(error), file=sys.stderr)
        return 2
    for issue in issues:
        print(issue)
    print(f'{len(args.plans)} PLAN(s), {len(issues)} finding(s); structure only, no execution proof')
    return 1 if issues else 0


if __name__ == '__main__':
    sys.exit(main())
