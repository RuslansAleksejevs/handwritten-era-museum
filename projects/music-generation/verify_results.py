"""Check committed experiment accounting and MIDI timelines without downloading data."""
from pathlib import Path
import hashlib
import math
import shutil
import subprocess
import mido
from corpus import read_json, digest

if not __debug__:
    raise SystemExit('assert-based checks need Python without -O or PYTHONOPTIMIZE')


def verify_media(stem, variant):
    """Check the render's file fingerprints, not a claim of musical equivalence."""
    media = variant['media']
    if media['events_sha256'] != digest(variant['events_original_key']):
        raise ValueError('events differ from render manifest')
    if media['midi_sha256'] != hashlib.sha256(stem.with_suffix('.mid').read_bytes()).hexdigest():
        raise ValueError('MIDI differs from render manifest')
    mp3 = stem.with_suffix('.mp3')
    if media['mp3'] is None:
        if mp3.exists():
            raise ValueError('stale MP3: render manifest records no MP3')
        return False
    if media['mp3']['file'] != mp3.name or media['mp3']['sha256'] != hashlib.sha256(mp3.read_bytes()).hexdigest():
        raise ValueError('MP3 differs from render manifest')
    return True


def verify(root):
    split = read_json(root/'split.json')
    metrics = read_json(root/'metrics.json')
    demos = read_json(root/'demos.json')
    pieces = {p['id']: p for p in split['pieces']}
    assert len(pieces) + len(split['excluded']) == 370

    for a,b,_ in split['family_edges']:
        assert pieces[a]['family'] == pieces[b]['family']
        assert pieces[a]['split'] == pieces[b]['split']
    for field in ['family', 'events_sha256']:
        owners = {}
        for p in pieces.values():
            owners.setdefault(p[field],set()).add(p['split'])
        assert all(len(s)==1 for s in owners.values()), f'{field} crosses split'
    assert metrics['corpus_sha256'] == split['pieces_sha256']
    expected = {p['id']:p for p in pieces.values() if p['split']=='test'}
    runs = metrics['runs']
    assert metrics['selected_seed'] == min(runs,key=lambda r:r['validation_nll'])['seed']
    reports = [metrics['markov_test']] + [r['test'] for r in runs]
    for report in reports:
        assert {p['id'] for p in report['pieces']} == set(expected)
        for p in report['pieces']:
            assert p['n'] == expected[p['id']]['events'] - 8
            assert p['family'] == expected[p['id']]['family']
        assert report['events'] == sum(p['n'] for p in report['pieces'])
        recomputed = sum(p['n']*p['nll'] for p in report['pieces'])/report['events']
        assert math.isclose(report['nll'],recomputed,abs_tol=1e-10)
    audio_count, decoded = 0, 0
    for demo in demos['demos']:
        assert demo['piece'] in expected
        prompts=[]
        for variant in demo['variants']:
            stem = root/f"{demo['piece']}-{variant['model']}"
            has_mp3 = verify_media(stem, variant)
            audio_count += has_mp3
            events=variant['events_original_key']
            prompts.append(events[:variant['prompt_events']])
            cursor, expected_onsets = 0, []
            for pitch,duration in events:
                if pitch: expected_onsets.append((cursor,pitch))
                cursor += duration*120
            midi = mido.MidiFile(stem.with_suffix('.mid'))
            clock, actual_onsets = 0, []
            for message in midi.tracks[0]:
                clock += message.time
                if message.type=='note_on' and message.velocity:
                    actual_onsets.append((clock,message.note))
            assert actual_onsets == expected_onsets
            assert clock == cursor
            assert math.isclose(midi.length+.75,variant['seconds'],abs_tol=.0001)
            assert 20 <= variant['seconds'] <= 40
            if has_mp3 and shutil.which('ffmpeg'):
                subprocess.run(['ffmpeg','-v','error','-i',str(stem.with_suffix('.mp3')),
                                '-f','null','-'],check=True)
                decoded += 1
        assert prompts[0] == prompts[1] == prompts[2]
    print(f"PASS: {len(pieces)} scores, separated families, {reports[0]['events']} test events, "
          f"{len(demos['demos'])*3} matched MIDI timelines; "
          f"{audio_count} MP3 fingerprints verified, {decoded} decoded. Audio content is not independently matched to notes.")


if __name__=='__main__':
    verify(Path(__file__).parent/'results')
