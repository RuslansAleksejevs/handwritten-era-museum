"""Render identical fixed prompts with both models and audit continuation overlap."""
import argparse
from difflib import SequenceMatcher
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import wave
import numpy as np
import torch
import mido
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from corpus import prepare, read_json, PROMPT, decode, digest
from model import MelodyGRU, Markov, continue_melody

BPM = 104
GENERATION_TICKS = 160
TEMPERATURE = .85
SAMPLE_SEEDS = [11, 29, 47]


def longest_match(events, training, transpose=False):
    """Longest contiguous match, including duration; prompt must be removed first.

    For transposed matches, anchor each possible starting pair and extend at one
    constant pitch offset. Rest positions and durations must also match exactly.
    """
    best = {'events': 0, 'piece': None, 'generated_start': None, 'training_start': None, 'shift': None}
    for p in training:
        source = p['events']
        for i, (a, duration) in enumerate(events):
            for j, (b, d) in enumerate(source):
                if duration != d or bool(a) != bool(b):
                    continue
                shift = (a-b) if a and b else None
                if not transpose and shift not in (None, 0):
                    continue
                n = 0
                current_shift = shift
                while i+n < len(events) and j+n < len(source):
                    x, dx = events[i+n]
                    y, dy = source[j+n]
                    if dx != dy or bool(x) != bool(y):
                        break
                    if x and y:
                        if current_shift is None:
                            current_shift = x-y
                        if x-y != current_shift or (not transpose and x != y):
                            break
                    n += 1
                if n > best['events']:
                    best = dict(events=n, piece=p['id'], generated_start=i,
                                training_start=j, shift=current_shift or 0)
    return best


def save_midi(events, path, prompt_events=PROMPT, bpm=BPM):
    midi = mido.MidiFile(ticks_per_beat=480)
    track = mido.MidiTrack()
    midi.tracks.append(track)
    track.append(mido.MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    track.append(mido.Message('program_change', program=4, time=0))
    delay = 0
    for i, (pitch, duration) in enumerate(events):
        if i == prompt_events:
            track.append(mido.MetaMessage('marker', text='Generated continuation', time=delay))
            delay = 0
        ticks = duration*120
        if not pitch:
            delay += ticks
            continue
        track.append(mido.Message('note_on', note=pitch, velocity=76, time=delay))
        sounding = round(ticks*.93)
        track.append(mido.Message('note_off', note=pitch, velocity=0, time=sounding))
        delay = ticks-sounding
    track.append(mido.MetaMessage('end_of_track', time=delay))
    midi.save(path)


def save_wav(events, path, bpm=BPM, sample_rate=24000):
    seconds_per_tick = 60/bpm/4
    total = sum(d for _, d in events)*seconds_per_tick
    samples = np.zeros(round((total+.75)*sample_rate))
    cursor = 0.
    for pitch, duration in events:
        seconds = duration*seconds_per_tick
        if pitch:
            t = np.arange(round((seconds+.32)*sample_rate))/sample_rate
            frequency = 440*2**((pitch-69)/12)
            note = np.zeros_like(t)
            for harmonic, amplitude, decay in [(1, 1, .7), (2, .3, .32), (3, .1, .2), (4, .055, .13)]:
                if frequency*harmonic < sample_rate*.45:
                    note += amplitude*np.sin(2*np.pi*frequency*harmonic*t)*np.exp(-t/decay)
            envelope = np.minimum(t/.008, 1)*np.exp(-np.maximum(t-seconds*.93, 0)/.065)
            start = round(cursor*sample_rate)
            samples[start:start+len(note)] += note*envelope*.35
        cursor += seconds
    # A small fixed room reflection, applied identically to all outputs.
    dry = samples.copy()
    for delay, gain in [(.047, .08), (.083, .06), (.139, .035)]:
        n = round(delay*sample_rate)
        samples[n:] += gain*dry[:-n]
    peak = float(np.max(np.abs(samples)))
    if peak > .85:
        samples *= .85/peak
    with wave.open(str(path), 'wb') as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes((samples*32767).astype('<i2').tobytes())
    return total+.75


def render_audio(events, wav_path, mp3_path):
    """Never leave a previous MP3 beside newly rendered events, even on failure."""
    mp3_path.unlink(missing_ok=True)
    seconds = save_wav(events, wav_path)
    encoder = shutil.which('ffmpeg')
    mp3 = None
    if encoder:
        pending = mp3_path.with_suffix('.pending.mp3')
        try:
            subprocess.run([encoder, '-v', 'error', '-y', '-i', str(wav_path),
                            '-codec:a', 'libmp3lame', '-b:a', '96k', '-map_metadata', '-1',
                            str(pending)], check=True)
            pending.replace(mp3_path)
        finally:
            pending.unlink(missing_ok=True)
        mp3 = {'file': mp3_path.name, 'sha256': hashlib.sha256(mp3_path.read_bytes()).hexdigest()}
    return seconds, {'wav_sha256': hashlib.sha256(wav_path.read_bytes()).hexdigest(), 'mp3': mp3}


def comparison_plot(exhibits, path):
    fig, axes = plt.subplots(len(exhibits), 1, figsize=(12, 7), sharex=True, layout='constrained')
    fig.patch.set_facecolor('#f5f1e8')
    for ax, (label, events) in zip(axes, exhibits):
        ax.set_facecolor('#f5f1e8')
        cursor = 0
        boundary = sum(d for _, d in events[:PROMPT])/4
        pitches = [p for _, notes in exhibits for p, d in notes if p]
        low, high = min(pitches)-2, max(pitches)+2
        for i, (pitch, duration) in enumerate(events):
            color = '#b5a17c' if i < PROMPT else '#243c45'
            if pitch:
                ax.add_patch(Rectangle((cursor/4, pitch-.32), duration/4*.93, .64, color=color))
            else:
                ax.plot([cursor/4, (cursor+duration)/4], [low+1]*2, color='#a64d35', lw=2)
            cursor += duration
        ax.axvline(boundary, color='#a64d35', linestyle='--', linewidth=1)
        ax.set_ylim(low, high)
        ax.set_xlim(0, max(sum(d for _,d in n)/4 for _,n in exhibits))
        ax.set_ylabel('MIDI pitch')
        ax.set_title(label, loc='left', color='#243c45', fontsize=11)
        ax.grid(axis='y', alpha=.12)
        ax.spines[['top', 'right']].set_visible(False)
    axes[-1].set_xlabel('Quarter-note beats · shared prompt in gold · continuation in blue · rests in rust')
    fig.savefig(path, dpi=160, metadata={'Software': 'Matplotlib'})
    plt.close(fig)


def training_plot(report, path):
    fig, ax = plt.subplots(figsize=(8, 3.6), layout='constrained')
    fig.patch.set_facecolor('#f5f1e8')
    ax.set_facecolor('#f5f1e8')
    for run, color in zip(report['runs'], ['#243c45', '#a64d35', '#8b7c51']):
        history = run['history']
        ax.plot([x['epoch'] for x in history], [x['validation_nll'] for x in history],
                label=f"GRU seed {run['seed']}", color=color)
        ax.scatter(run['best_epoch'], run['validation_nll'], color=color, s=25)
    ax.axhline(report['selected_baseline']['validation_nll'], color='#777', ls='--', label='Selected Markov')
    ax.set(xlabel='Training epoch', ylabel='Validation NLL / event', ylim=(1.7, 4.3),
           title='Choose the checkpoint on validation melodies')
    ax.spines[['top', 'right']].set_visible(False)
    ax.legend(frameon=False, fontsize=8)
    fig.savefig(path, dpi=160)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=Path(tempfile.gettempdir())/'museum-music')
    parser.add_argument('--output', type=Path, default=Path(__file__).parent/'results')
    parser.add_argument('--wav-dir', type=Path, help='uncompressed audio location; defaults to cache/audio')
    args = parser.parse_args()
    if not shutil.which('ffmpeg'):
        print('FFmpeg unavailable: rendering WAV/MIDI/plots; old matching MP3 files will be removed.',
              file=sys.stderr)
    torch.set_num_threads(2)
    data = prepare(args.cache)
    metrics = read_json(args.output/'metrics.json')
    if data['pieces_sha256'] != metrics['corpus_sha256']:
        raise ValueError('metrics and corpus do not match')
    checkpoint = args.cache/f"gru-{metrics['selected_seed']}.pt"
    run = next(r for r in metrics['runs'] if r['seed'] == metrics['selected_seed'])
    if hashlib.sha256(checkpoint.read_bytes()).hexdigest() != run['checkpoint_sha256']:
        raise ValueError('checkpoint checksum mismatch')
    saved = torch.load(checkpoint, map_location='cpu', weights_only=True)
    model = MelodyGRU(**saved['config'])
    model.load_state_dict(saved['state_dict'])
    training = [p for p in data['pieces'] if p['split'] == 'train']
    config = metrics['selected_baseline']
    markov = Markov(config['order'], config['strength']).fit(training)
    # Fixed before listening: first three test pieces in the source numbering.
    prompts = sorted([p for p in data['pieces'] if p['split']=='test'], key=lambda p:p['id'])[:3]
    audio_dir = args.wav_dir or args.cache/'audio'
    audio_dir.mkdir(parents=True, exist_ok=True)
    demos = []
    for piece, seed in zip(prompts, SAMPLE_SEEDS):
        variants, plot_rows = [], []
        for label, generator in [('gru', model), ('markov', markov), ('reference', None)]:
            if generator is None:
                events = piece['events'][:PROMPT]
                for event in piece['events'][PROMPT:]:
                    events = events + [event]
                    if sum(d for _,d in events[PROMPT:]) >= GENERATION_TICKS:
                        break
            else:
                events = decode(continue_melody(generator, piece, seed, GENERATION_TICKS, TEMPERATURE))
            # Play each prompt in its original key. Audits use normalized events.
            audible = [[p-piece['shift'] if p else 0, d] for p,d in events]
            stem = f"{piece['id']}-{label}"
            midi_path = args.output/f'{stem}.mid'
            save_midi(audible, midi_path, prompt_events=PROMPT if generator else -1)
            seconds, audio = render_audio(audible, audio_dir/f'{stem}.wav', args.output/f'{stem}.mp3')
            media = {'events_sha256': digest(audible),
                     'midi_sha256': hashlib.sha256(midi_path.read_bytes()).hexdigest(), **audio}
            audit = {'exact': longest_match(events[PROMPT:], training),
                     'transposed': longest_match(events[PROMPT:], training, transpose=True)}
            continuation = events[PROMPT:]
            variants.append({'model': label, 'seconds': seconds, 'events_original_key': audible,
                             'prompt_events': PROMPT, 'continuation_events': len(continuation),
                             'rest_events': sum(p==0 for p,d in continuation),
                             'distinct_pitches': len({p for p,d in continuation if p}),
                             'overlap_with_training': audit, 'media': media})
            plot_rows.append(({'gru':'GRU', 'markov':'Markov', 'reference':'Bach reference'}[label], audible))
        comparison_plot(plot_rows, args.output/f"{piece['id']}-comparison.png")
        demos.append({'piece': piece['id'], 'title': piece['title'], 'key': piece['key'],
                      'family': piece['family'], 'sampling_seed': seed, 'variants': variants})
    report = {'prompt_rule': 'first three test pieces in source numbering; no listening selection',
              'temperature': TEMPERATURE, 'generation_ticks': GENERATION_TICKS, 'bpm': BPM,
              'ticks_per_quarter': 4, 'selected_training_seed': metrics['selected_seed'],
              'audits': 'contiguous pitch+duration matches, generated suffix only; not a plagiarism detector',
              'demos': demos}
    (args.output/'demos.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    training_plot(metrics, args.output/'training.png')
    print(json.dumps([{'piece': d['piece'], 'audio': [{k:v for k,v in s.items() if k!='events_original_key'}
                                                     for s in d['variants']]} for d in demos], indent=2))


if __name__ == '__main__':
    main()
