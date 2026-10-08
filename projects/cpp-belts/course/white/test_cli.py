"""Independent I/O checks for every White Belt command and a database oracle."""
import random
import subprocess
import tempfile
from pathlib import Path

# IDs are referenced by coverage.json; inputs and expectations are newly authored.
CASES = {
    'sum': ('-99999 100000\n', '1\n'),
    'min-string': ('zero alpha alphabet\n', 'alpha\n'),
    'equation': ('0 2 -7\n', '3.5\n'),
    'division': ('9 0\n', 'Impossible\n'),
    'price': ('150 100 120 10 20\n', '120\n'),
    'even': ('3 9\n', '4 6 8\n'),
    'second-f': ('f x f\n', '4\n'),
    'gcd': ('84 126\n', '42\n'),
    'binary': ('42\n', '101010\n'),
    'temperature': ('4\n2 4 6 8\n', '2\n2 3\n'),
    'queue': ('10\nCOME 3\nWORRY 1\nWORRY 1\nWORRY_COUNT\nQUIET 1\nWORRY 2\nCOME -1\nWORRY_COUNT\nCOME -2\nWORRY_COUNT\n', '1\n0\n0\n'),
    'months': ('9\nADD 31 end\nADD 28 keep\nNEXT\nDUMP 28\nNEXT\nDUMP 31\nDUMP 28\nNEXT\nDUMP 30\n', '2 keep end\n0\n2 keep end\n0\n'),
    'anagrams': ('3\naab aba\naab abb\nx x\n', 'YES\nNO\nYES\n'),
    'capitals': ('9\nDUMP\nABOUT A\nCHANGE_CAPITAL A X\nCHANGE_CAPITAL A X\nCHANGE_CAPITAL A Y\nRENAME A B\nRENAME B B\nABOUT B\nDUMP\n', "There are no countries in the world\nCountry A doesn't exist\nIntroduce new country A with capital X\nCountry A hasn't changed its capital\nCountry A has changed its capital from X to Y\nCountry A with capital Y has been renamed to B\nIncorrect rename, skip\nCountry B has capital Y\nB/Y\n"),
    'buses': ('7\nALL_BUSES\nNEW_BUS Z 2 x y\nNEW_BUS A 2 y z\nBUSES_FOR_STOP y\nSTOPS_FOR_BUS Z\nSTOPS_FOR_BUS missing\nALL_BUSES\n', 'No buses\nZ A\nStop x: no interchange\nStop y: A\nNo bus\nBus A: y z\nBus Z: x y\n'),
    'routes': ('3\n2 a b\n2 b a\n2 a b\n', 'New bus 1\nNew bus 2\nAlready exists for 1\n'),
    'route-sets': ('3\n2 a b\n2 b a\n3 a a b\n', 'New bus 1\nAlready exists for 1\nAlready exists for 1\n'),
    'unique': ('4\nx\ny\nx\nz\n', '3\n'),
    'synonyms': ('7\nADD a b\nADD b c\nADD a b\nCOUNT b\nCHECK a c\nCHECK b a\nCOUNT z\n', '2\nNO\nYES\n0\n'),
    'abs-sort': ('5 -8 3 -1 0 5\n', '0 -1 3 5 -8\n'),
    'case-sort': ('4 zoo B apple AA\n', 'AA apple B zoo\n'),
    'copy': (' A\n\nB\n', ' A\n\nB\n'),
    'copy-file': (' A\n\nB\n', ' A\n\nB\n'),
    'precision': ('1.23456\n-2.4\n0\n', '1.235\n-2.400\n0.000\n'),
    'table': ('2 2\n1,-2\n30,4\n', '         1         -2\n        30          4'),
    'students': ('1\nA B 2 3 2000\n4\nname 1\ndate 1\nname 0\nunknown 1\n', 'A B\n2.3.2000\nbad request\nbad request\n'),
    'calculator': ('1/3 + 1/6\n', '1/2\n'),
    'database': ('Add 2-1-1 z\nAdd 2-1-1 a\nAdd 2-1-1 a\nPrint\nDel 2-1-1 a\nFind 2-1-1\nDel 2-1-1\n', '0002-01-01 a\n0002-01-01 z\nDeleted successfully\nz\nDeleted 1 events\n'),
}

def run(binary, command, source):
    with tempfile.TemporaryDirectory(prefix='white-cli-') as directory:
        if command in ('copy', 'copy-file', 'precision', 'table'):
            (Path(directory) / 'input.txt').write_text(source)
        p = subprocess.run([str(binary), command], input=source, text=True, capture_output=True, cwd=directory, timeout=10)
        if p.returncode:
            raise AssertionError((command, p.returncode, p.stderr))
        return (Path(directory) / 'output.txt').read_text() if command == 'copy-file' else p.stdout


def check(binary):
    for command, (source, expected) in CASES.items():
        assert run(binary, command, source) == expected, command
    for source, expected in [('1/0 + 2/3', 'Invalid argument\n'), ('1/2 / 0/3', 'Division by zero\n'), ('1/2 * 3/4', '3/8\n')]:
        assert run(binary, 'calculator', source) == expected
    for source, expected in [('Add 1-13-0 x\nPrint\n', 'Month value is invalid: 13\n'), ('Find 1--1-2\n','Month value is invalid: -1\n'), ('Find 1-2-0\n','Day value is invalid: 0\n'), ('Find 1-2-3x\n', 'Wrong date format: 1-2-3x\n'), ('Nope\nPrint\n','Unknown command: Nope\n')]:
        assert run(binary, 'database', source) == expected
    # Stateful oracle uses a flat set and sorted projections, independent of the C++ map layout.
    rng = random.Random(1701)
    saved = set()
    commands, expected = [], []
    for _ in range(500):
        date = (rng.randrange(4), rng.randrange(1, 13), rng.randrange(1, 32))
        date_text = '-'.join(map(str, date))
        event = rng.choice(['a', 'b', 'cc', 'zz'])
        op = rng.choice(['Add', 'DelOne', 'DelAll', 'Find', 'Print'])
        if op == 'Add':
            saved.add((date, event)); commands.append(f'Add {date_text} {event}')
        elif op == 'DelOne':
            commands.append(f'Del {date_text} {event}')
            expected.append('Deleted successfully' if (date, event) in saved else 'Event not found')
            saved.discard((date, event))
        elif op == 'DelAll':
            commands.append(f'Del {date_text}')
            matches = {pair for pair in saved if pair[0] == date}
            expected.append(f'Deleted {len(matches)} events'); saved -= matches
        elif op == 'Find':
            commands.append(f'Find {date_text}'); expected.extend(e for d, e in sorted(saved) if d == date)
        else:
            commands.append('Print'); expected.extend(f'{d[0]:04}-{d[1]:02}-{d[2]:02} {e}' for d, e in sorted(saved))
    assert run(binary, 'database', '\n'.join(commands)) == ''.join(line+'\n' for line in expected)
    # All discriminant branches, including the statement's erroneous 'fire' example.
    for a,b,c,roots in [(1,0,-4,[-2,2]),(1,2,1,[-1]),(1,0,1,[]),(0,0,3,[]),(1,0,0,[0])]:
        actual=sorted(map(float,run(binary,'equation',f'{a} {b} {c}').split()))
        assert actual==roots
    print(f'PASS: {len(CASES)} white CLI contracts, date/calculator errors, 500-command database oracle')
