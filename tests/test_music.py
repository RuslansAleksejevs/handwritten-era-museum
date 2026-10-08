"""Offline synthetic fixtures: musical timing, separation, and causal scoring."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import hashlib
import subprocess
import wave
import numpy as np
import torch
import mido
from music21 import stream, note, tie
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'projects/music-generation'))
from corpus import encode, decode, stream_events, assign_splits, read_json, digest, PITCHES, DURATIONS
from model import MelodyGRU, Markov, batch, event_losses, continue_melody
from render import save_midi, save_wav, render_audio, longest_match
from verify_results import verify_media


def piece(events=None, **extra):
    return dict(id='fixture', title='Synthetic fixture', bwv=None, mode=0, family='fixture',
                bar_ticks=16, pickup_ticks=12,
                events=events or [[60+i%5, 2+i%3] for i in range(24)]) | extra


class MusicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(2)

    def test_event_roundtrip_and_out_of_range(self):
        events = [[60, 4], [0, 2], [88, 32], [48, 1]]
        self.assertEqual(decode(encode(events)), events)
        for invalid in [[[47,4]], [[60,0]], [[60,33]]]:
            with self.assertRaises(ValueError): encode(invalid)

    def test_audio_without_encoder_removes_old_mp3(self):
        with tempfile.TemporaryDirectory() as d:
            wav, mp3 = Path(d)/'fixture.wav', Path(d)/'fixture.mp3'
            mp3.write_bytes(b'old recording')
            with patch('render.shutil.which', return_value=None):
                _, media = render_audio([[60,4],[0,4],[62,4]], wav, mp3)
            self.assertTrue(wav.exists())
            self.assertFalse(mp3.exists())
            self.assertIsNone(media['mp3'])

    def test_failed_encoder_cannot_retain_previous_mp3(self):
        with tempfile.TemporaryDirectory() as d:
            wav, mp3 = Path(d)/'fixture.wav', Path(d)/'fixture.mp3'
            mp3.write_bytes(b'old recording')
            def fail(command, **kwargs):
                Path(command[-1]).write_bytes(b'partial recording')
                raise subprocess.CalledProcessError(1, command)
            with patch('render.shutil.which', return_value='ffmpeg'), patch('render.subprocess.run', side_effect=fail):
                with self.assertRaises(subprocess.CalledProcessError):
                    render_audio([[60,4]], wav, mp3)
            self.assertFalse(mp3.exists())
            self.assertFalse(mp3.with_suffix('.pending.mp3').exists())

    def test_render_manifest_rejects_changed_events_and_stale_audio(self):
        with tempfile.TemporaryDirectory() as d:
            stem = Path(d)/'fixture'
            events = [[60,4],[62,4]]
            save_midi(events, stem.with_suffix('.mid'))
            variant = {'events_original_key': events, 'media': {
                'events_sha256': digest(events), 'midi_sha256': hashlib.sha256(stem.with_suffix('.mid').read_bytes()).hexdigest(),
                'mp3': None}}
            self.assertFalse(verify_media(stem, variant))
            stem.with_suffix('.mp3').write_bytes(b'old audio')
            with self.assertRaisesRegex(ValueError, 'stale MP3'): verify_media(stem, variant)
            variant['media']['mp3'] = {'file': 'fixture.mp3', 'sha256': hashlib.sha256(b'new audio').hexdigest()}
            with self.assertRaisesRegex(ValueError, 'MP3 differs'): verify_media(stem, variant)
            variant['events_original_key'] = [[64,4],[62,4]]
            with self.assertRaisesRegex(ValueError, 'events differ'): verify_media(stem, variant)

    def test_ties_repeated_notes_rests_and_gaps(self):
        part = stream.Part()
        a, b = note.Note('C4', quarterLength=1), note.Note('C4', quarterLength=.5)
        a.tie, b.tie = tie.Tie('start'), tie.Tie('stop')
        part.insert(0,a); part.insert(1,b)
        part.insert(1.5, note.Note('C4', quarterLength=.5))
        part.insert(2, note.Rest(quarterLength=.5))
        part.insert(3, note.Note('D4', quarterLength=1))
        self.assertEqual(stream_events(part), [[60,6],[60,2],[0,2],[0,2],[62,4]])

    def test_overlap_and_off_grid_fail_loudly(self):
        part=stream.Part()
        part.insert(0, note.Note('C4', quarterLength=1))
        part.insert(.5, note.Note('E4', quarterLength=1))
        with self.assertRaisesRegex(ValueError, 'overlapping'): stream_events(part)
        offgrid = stream.Part([note.Note('C4', quarterLength=1/3)])
        with self.assertRaisesRegex(ValueError, 'off-grid'): stream_events(offgrid)

    def test_family_grouping_precedes_splits(self):
        base = piece()
        transposed = piece([[p+5,d] for p,d in base['events']], id='variant', title='Different title')
        relative = piece([[70,4]]*30, id='title-relative', title='Synthetic-fixture')
        items = [base, transposed, relative]
        self.assertTrue(assign_splits(items))
        self.assertEqual(len({p['family'] for p in items}), 1)
        self.assertEqual(len({p['split'] for p in items}), 1)

    def test_duplicate_json_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'data.json'; p.write_text('{"seed":1,"seed":2}')
            with self.assertRaisesRegex(ValueError,'duplicate'): read_json(p)

    def test_padding_and_warmup_do_not_count_as_targets(self):
        a, b = piece(), piece([[60,4]]*12)
        packed = batch([a,b])
        self.assertEqual(packed[-1].sum().item(), len(a['events'])+len(b['events'])-16)
        self.assertFalse(packed[-1][:,:7].any())
        model = MelodyGRU(hidden=12,layers=1,dropout=0).eval()
        together = event_losses(model,packed)[0]
        separate = torch.cat([event_losses(model,batch([p]))[0] for p in [a,b]])
        torch.testing.assert_close(together,separate)

    def test_next_onset_phase_includes_pickup(self):
        self.assertEqual(batch([piece([[60,4]]*12)])[2][0,:5].tolist(), [0,4,8,12,0])

    def test_prefix_logits_are_causal_and_incremental_state_matches(self):
        torch.manual_seed(8)
        model = MelodyGRU(hidden=12,layers=2,dropout=0).eval()
        x,y,phase,mode,meter,mask = batch([piece()])
        full, _, _ = model(x,phase,mode,meter)
        changed = x.clone(); changed[:,10:,0] = 0
        perturbed, _, _ = model(changed,phase,mode,meter)
        torch.testing.assert_close(full[:,:10],perturbed[:,:10])
        h, outputs = None, []
        for i in range(x.shape[1]):
            logits, _, h = model(x[:,i:i+1],phase[:,i:i+1],mode,meter,h)
            outputs.append(logits)
        torch.testing.assert_close(full,torch.cat(outputs,dim=1))

    def test_joint_distribution_and_gradients(self):
        model = MelodyGRU(hidden=12,layers=1,dropout=0)
        packed = batch([piece()])
        logits, states, _ = model(packed[0],packed[2],packed[3],packed[4])
        pitches = torch.arange(PITCHES).reshape(1,-1)
        duration = model.duration_logits(states[:,-1:].expand(1,PITCHES,12),pitches)
        joint = logits[:,-1].softmax(-1).unsqueeze(-1)*duration.softmax(-1)
        self.assertAlmostEqual(float(joint.detach().sum()),1.,places=6)
        event_losses(model,packed)[0].mean().backward()
        for parameter in model.parameters():
            self.assertIsNotNone(parameter.grad)
            self.assertTrue(torch.isfinite(parameter.grad).all())

    def test_markov_counts_probability_and_refit(self):
        p = piece([[60,4]]*20)
        model = Markov(2,5).fit([p])
        token = encode([[60,4]])[0]; token = token[0]*DURATIONS+token[1]
        self.assertEqual(model.counts[(0,())][token],12)
        for mode in (0,1):
            probs = model.probabilities([999,1000],mode)
            self.assertTrue(np.all(probs>0))
            self.assertAlmostEqual(float(probs.sum()),1.)
        self.assertLess(model.score(p).mean(),.1)
        model.fit([piece([[62,4]]*20)])
        self.assertEqual(model.counts[(0,())][token],0)

    def test_both_samplers_reproducible_and_preserve_seed(self):
        p = piece()
        for model in [Markov().fit([p]),MelodyGRU(hidden=12,layers=1,dropout=0)]:
            a = continue_melody(model,p,11,32)
            self.assertEqual(a,continue_melody(model,p,11,32))
            self.assertEqual(a[:8],encode(p['events'][:8]))
            ticks = sum(d+1 for _,d in a[8:])
            self.assertGreaterEqual(ticks,32)
            self.assertLess(ticks,32+DURATIONS)
        with self.assertRaises(ValueError): continue_melody(model,p,11,32,0)

    def test_midi_timing_rests_and_boundary(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'fixture.mid'
            save_midi([[60,4],[0,2],[62,6]],p,prompt_events=2,bpm=120)
            midi = mido.MidiFile(p)
            absolute, onsets, marker = 0, [], None
            for message in midi.tracks[0]:
                absolute += message.time
                if message.type=='note_on': onsets.append(absolute)
                if message.type=='marker': marker=absolute
            self.assertEqual(onsets,[0,720]); self.assertEqual(marker,720)
            self.assertEqual(absolute,1440); self.assertAlmostEqual(midi.length,1.5)

    def test_audio_silence_length_and_no_clipping(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'fixture.wav'
            seconds = save_wav([[0,4],[60,4]],p,bpm=120,sample_rate=8000)
            with wave.open(str(p)) as w:
                samples = np.frombuffer(w.readframes(w.getnframes()),dtype='<i2')
                self.assertAlmostEqual(len(samples)/w.getframerate(),seconds)
            self.assertTrue(np.all(samples[:3900]==0))
            self.assertGreater(abs(samples).max(),1000); self.assertLess(abs(samples).max(),32767)

    def test_copy_detection_requires_contiguous_rhythm_and_constant_shift(self):
        training=[piece([[60,4],[62,2],[0,2],[64,8],[65,4]])]
        generated=[[67,4],[69,2],[0,2],[71,8],[72,4]]
        self.assertEqual(longest_match(generated,training)['events'],1)
        self.assertEqual(longest_match(generated,training,True)['events'],5)
        generated[3][1]=4
        self.assertEqual(longest_match(generated,training,True)['events'],3)
