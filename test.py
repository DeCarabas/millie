#!/usr/local/bin/python3
import locale

from collections import namedtuple
from pathlib import Path
from subprocess import run, PIPE
from time import perf_counter


def read_test_spec(path):
    spec = {}
    with open(path) as file:
        for line in file:
            if (not line) or line[0] != '#':
                break
            spec_part = line[1:].strip().split(':')
            if len(spec_part) > 1:
                spec[spec_part[0].strip()] = spec_part[1].strip()
    return spec


test_result = namedtuple(
    'test_result',
    ['result', 'path', 'elapsed', 'details', 'stderr', 'stdout'],
)


def parse_bool(val):
    val = val.lower()
    return val in ('true', 1, 'yes', 'y')


def run_test(path):
    spec = read_test_spec(path)

    if (
        ('Disabled' in spec and parse_bool(spec['Disabled'])) or
        ('Enabled' in spec and not parse_bool(spec['Enabled']))
    ):
        return test_result(
            result='skip',
            path=path,
            elapsed=0.0,
            details=None,
            stderr=None,
            stdout=None,
        )

    start = perf_counter()
    cp = run(
        ['./millie', path],
        stdout=PIPE,
        stderr=PIPE,
        encoding=locale.getpreferredencoding()
    )
    elapsed = perf_counter() - start

    result = 'ok'
    details = None

    expect_failure = 'ExpectFailure' in spec
    if cp.returncode and not expect_failure:
        result = 'fail'
        details = 'millie returned exit code {}'.format(cp.returncode)
    else:
        actual_type = cp.stdout.strip()
        if 'ExpectedType' in spec:
            if actual_type != spec['ExpectedType']:
                result = 'fail'
                details = 'Expected type "{}" got "{}"'.format(
                    spec['ExpectedType'],
                    actual_type
                )
        if 'ExpectedError' in spec:
            if not spec['ExpectedError'] in cp.stderr:
                result = 'fail'
                details = "Expected error '{}' to be reported".format(
                    spec['ExpectedError'],
                )

    return test_result(result, path, elapsed, details, cp.stderr, cp.stdout)


locale.setlocale(locale.LC_ALL, '')
for path in Path('./tests').glob('**/*.millie'):
    result, path, elapsed, details, stderr, stdout = run_test(path)
    print('[{0:<4}] {1} ({2:0.3}ms)'.format(result, path, elapsed * 1000))
    if result == 'fail':
        print('  ' + details)
        print('  stdout:')
        print('    ' + '\n    '.join(stdout.split('\n')))
        print('  stderr:')
        print('    ' + '\n    '.join(stderr.split('\n')))
