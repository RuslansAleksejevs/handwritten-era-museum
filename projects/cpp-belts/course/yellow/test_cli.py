"""Black-box contracts and independent state/date/parser oracles."""
import datetime as dt
import itertools
import random
import subprocess

CASES={
 'matrix':('2 2 1 2 3 4\n2 2 -1 3 7 0\n','2 2\n0 5\n10 4\n'),
 'temperature':('4 -8 -6 -4 -2\n','2\n2 3\n'),
 'blocks':('2 100\n10000 10000 10000\n10000 10000 10000\n','200000000000000\n'),
 'buses':('5\nNEW_BUS Z 2 x y\nNEW_BUS A 1 y\nBUSES_FOR_STOP y\nSTOPS_FOR_BUS Z\nALL_BUSES\n','Z A\nStop x: no interchange\nStop y: A\nBus A: y\nBus Z: x y\n'),
 'permutations':('2\n','2 1\n1 2\n'),
 'budget':('4\nEarn 2000-02-28 2000-03-01 30\nComputeIncome 2000-02-29 2000-02-29\nEarn 2000-02-29 2000-02-29 5\nComputeIncome 2000-01-01 2000-12-31\n','10\n35\n'),
 'budget-prefix':('3\n1700-01-01 10\n2000-02-29 20\n2099-12-31 30\n3\n1700-01-01 2099-12-31\n1700-01-01 1999-12-31\n2000-02-29 2000-02-29\n','60\n10\n20\n'),
 'arithmetic':('8\n3\n* 3\n- 6\n/ 1\n','(((8) * 3) - 6) / 1\n'),
 'arithmetic-minimal':('8\n3\n* 3\n- 6\n/ 1\n','(8 * 3 - 6) / 1\n'),
 'figures':('ADD RECT 2 3\nADD TRIANGLE 3 4 5\nADD CIRCLE 5\nPRINT\n','RECT 10.000 6.000\nTRIANGLE 12.000 6.000\nCIRCLE 31.400 78.500\n'),
 'database':('Add 2000-1-1 z\nAdd 2000-1-1 a\nAdd 2000-1-1 z\nLast 1999-1-1\nPrint\nFind event == "a"\nDel event != "a"\nLast 2000-1-1\n','No entries\n2000-01-01 z\n2000-01-01 a\n2000-01-01 a\nFound 1 entries\nRemoved 1 entries\n2000-01-01 a\n'),
}

def run(binary,command,data):
 p=subprocess.run([str(binary),command],input=data,text=True,capture_output=True,timeout=15)
 if p.returncode:raise AssertionError((command,p.returncode,p.stderr))
 return p.stdout

def check(binary):
 for command,(data,expected) in CASES.items():
  assert run(binary,command,data)==expected,command
 # The maximum block total exceeds signed int64 and still fits uint64.
 assert run(binary,'blocks','100000 100\n'+'10000 10000 10000\n'*100000)=='10000000000000000000\n'
 expected=''.join(' '.join(map(str,p))+'\n' for p in sorted(itertools.permutations(range(1,7)),reverse=True))
 assert run(binary,'permutations','6\n')==expected
 rng=random.Random(9381)
 origin=dt.date(1700,1,1)
 end=dt.date(2099,12,31)
 span=(end-origin).days+1
 earnings=[(rng.randrange(span),rng.randrange(1,1000001)) for _ in range(500)]
 queries=[tuple(sorted((rng.randrange(span),rng.randrange(span)))) for _ in range(200)]
 text=str(len(earnings))+'\n'+''.join(f'{origin+dt.timedelta(days=d)} {v}\n' for d,v in earnings)+str(len(queries))+'\n'+''.join(f'{origin+dt.timedelta(days=a)} {origin+dt.timedelta(days=b)}\n' for a,b in queries)
 expected=[sum(v for d,v in earnings if a<=d<=b) for a,b in queries]
 assert list(map(int,run(binary,'budget-prefix',text).split()))==expected
 # Range-budget oracle is a simple independently updated day array.
 days=[0.]*400
 commands=[];expected=[];start=dt.date(2000,1,1)
 for _ in range(200):
  a,b=sorted((rng.randrange(len(days)),rng.randrange(len(days))))
  dates=f'{start+dt.timedelta(days=a)} {start+dt.timedelta(days=b)}'
  if rng.randrange(2):
   value=rng.randrange(1,1000);commands.append(f'Earn {dates} {value}')
   for i in range(a,b+1):days[i]+=value/(b-a+1)
  else:commands.append(f'ComputeIncome {dates}');expected.append(sum(days[a:b+1]))
 actual=list(map(float,run(binary,'budget',str(len(commands))+'\n'+'\n'.join(commands)).split()))
 assert all(abs(a-b)<1e-8 for a,b in zip(actual,expected)) and len(actual)==len(expected)
 # Independent event store: chronological keys with insertion-ordered lists.
 store={};commands=[];output=[]
 def fmt(d,e):return f'{d} {e}'
 for _ in range(700):
  date=f'2000-01-{rng.randrange(1,7):02}';event=rng.choice(['z','a','team meeting','AND (x)'])
  op=rng.choice(['Add','Find','Del','Last','Print'])
  if op=='Add':
   commands.append(f'Add {date} {event}');values=store.setdefault(date,[])
   if event not in values:values.append(event)
  elif op=='Last':
   commands.append(f'Last {date}');eligible=[d for d in store if d<=date]
   output.append(fmt(max(eligible),store[max(eligible)][-1]) if eligible else 'No entries')
  elif op=='Print':
   commands.append('Print');output.extend(fmt(d,e) for d in sorted(store) for e in store[d])
  else:
   condition=f'date <= {date} AND event != "{event}"'
   commands.append(f'{op} {condition}')
   selected=[(d,e) for d in sorted(store) for e in store[d] if d<=date and e!=event]
   if op=='Find':output.extend(fmt(d,e) for d,e in selected);output.append(f'Found {len(selected)} entries')
   else:
    for d,e in selected:store[d].remove(e)
    store={d:events for d,events in store.items() if events}
    output.append(f'Removed {len(selected)} entries')
 assert run(binary,'database','\n'.join(commands))==''.join(x+'\n' for x in output)
 # Parentheses change truth tables; events can contain reserved words/spaces.
 data='Add 2000-1-1 x\nAdd 2000-1-1 y\nAdd 2001-1-1 y\nFind event == "x" OR event == "y" AND date > 2000-1-1\nFind (event == "x" OR event == "y") AND date > 2000-1-1\nDel\nFind\n'
 expected='2000-01-01 x\n2001-01-01 y\nFound 2 entries\n2001-01-01 y\nFound 1 entries\nRemoved 3 entries\nFound 0 entries\n'
 assert run(binary,'database',data)==expected
 print(f'PASS: {len(CASES)} yellow CLI contracts, 700-command event oracle, {len(queries)+len(actual)} date queries, uint64 boundary and 720 permutations')
