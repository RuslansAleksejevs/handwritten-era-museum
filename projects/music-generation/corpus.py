"""Pinned Bach edition, soprano extraction, and conservative family-level splits.

Downloaded scores and parsed events stay in the caller's cache, outside Git.
All preprocessing decisions are fixed before model fitting. No MIDI heuristics.
"""
from collections import Counter
from difflib import SequenceMatcher
import hashlib
import html
import io
import json
from pathlib import Path
import random
import re
import tarfile
import unicodedata
import urllib.request

REVISION = "67ef0b59bf49d0b562e8dfc9b870f6f9d5287822"
ARCHIVE_SHA256 = "7b7f9cb7b349ed3111c82a73e899dd4080950037cbb143c3d7698234d4db4c5b"
URL = f"https://codeload.github.com/craigsapp/bach-370-chorales/tar.gz/{REVISION}"
PREPROCESS_VERSION = 1
MIN_PITCH, MAX_PITCH = 48, 88
PITCHES = MAX_PITCH - MIN_PITCH + 2  # 0 is a rest
DURATIONS = 32                    # integer sixteenth-note ticks, 1..32
PROMPT = 8


def read_json(path):
    def unique(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def download(cache):
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / f"bach-{REVISION}.tar.gz"
    if not archive.exists():
        with urllib.request.urlopen(URL, timeout=60) as response:
            data = response.read(8_000_001)
        if len(data) > 8_000_000:
            raise ValueError("unexpectedly large archive")
        if hashlib.sha256(data).hexdigest() != ARCHIVE_SHA256:
            raise ValueError("download checksum mismatch")
        archive.write_bytes(data)
    data = archive.read_bytes()
    if hashlib.sha256(data).hexdigest() != ARCHIVE_SHA256:
        raise ValueError("cached archive checksum mismatch")
    directory = cache / "scores"
    directory.mkdir(exist_ok=True)
    # Write only expected regular files; never extract arbitrary archive paths.
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        members = [m for m in tar.getmembers() if m.isfile() and
                   re.fullmatch(r"[^/]+/kern/chor\d{3}\.krn", m.name)]
        if len(members) != 370:
            raise ValueError("expected 370 scores")
        for member in members:
            (directory / Path(member.name).name).write_bytes(tar.extractfile(member).read())
    return directory


def ticks(value):
    exact = float(value) * 4
    rounded = round(exact)
    if abs(exact - rounded) > 1e-7:
        raise ValueError(f"off-grid rhythm: {value}")
    return rounded


def stream_events(part):
    """Merge ties, preserve rests/gaps, and reject overlaps or unsupported rhythm."""
    events, cursor = [], 0
    for item in part.stripTies().flatten().notesAndRests:
        start, duration = ticks(item.offset), ticks(item.quarterLength)
        if duration <= 0:
            raise ValueError("grace note or nonpositive duration")
        if start < cursor:
            raise ValueError("overlapping soprano events")
        if start > cursor:
            events.append([0, start - cursor])
        if item.isRest:
            pitch = 0
        elif item.isNote:
            pitch = int(item.pitch.midi)
        else:
            raise ValueError("chord in soprano")
        events.append([pitch, duration])
        cursor = start + duration
    if any(d > DURATIONS for _, d in events):
        raise ValueError("duration outside fixed 1..32 tick vocabulary")
    return events


def parse_score(path):
    from music21 import converter, key, meter
    text = path.read_text()
    soprano = next(line.split("\t").index("*Isoprn") for line in text.splitlines()
                   if "*Isoprn" in line.split("\t"))
    score = converter.parse(path, forceSource=True)
    part = next(p for p in score.parts if p.id == f"spine_{soprano}")
    keys = list(part.recurse().getElementsByClass(key.Key))
    meters = list(part.recurse().getElementsByClass(meter.TimeSignature))
    if not keys or not meters or len({k.name for k in keys}) != 1:
        raise ValueError("missing or changing key")
    if len({m.ratioString for m in meters}) != 1:
        raise ValueError("changing meter")
    tonality = keys[0]
    if tonality.mode not in ("major", "minor"):
        raise ValueError("unsupported mode")
    mode = int(tonality.mode == "minor")
    shift = ((9 if mode else 0) - tonality.tonic.pitchClass + 6) % 12 - 6
    events = stream_events(part)
    normalized = [[p + shift if p else 0, d] for p, d in events]
    if any(p and not MIN_PITCH <= p <= MAX_PITCH for p, _ in normalized):
        raise ValueError("pitch outside fixed vocabulary")
    if len(events) < PROMPT + 16:
        raise ValueError("too short")
    bar_ticks = ticks(meters[0].barDuration.quarterLength)
    if not 1 <= bar_ticks <= 32:
        raise ValueError("unsupported meter")
    first_measure = next(iter(part.getElementsByClass("Measure")), None)
    pickup = (bar_ticks - ticks(first_measure.highestTime)) % bar_ticks if first_measure else 0
    title = html.unescape(re.search(r"^!!!OTL@@DE: (.+)$", text, re.MULTILINE).group(1))
    bwv_match = re.search(r"^!!!SCT: (.+)$", text, re.MULTILINE)
    bwv = bwv_match.group(1) if bwv_match else None
    return {"id": path.stem, "title": title, "bwv": bwv,
            "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "key": tonality.name, "mode": mode, "shift": shift,
            "meter": meters[0].ratioString, "bar_ticks": bar_ticks,
            "pickup_ticks": pickup, "events": normalized}


def title_key(title):
    text = unicodedata.normalize("NFKD", title.casefold()).replace("ß", "ss")
    return "".join(c for c in text if c.isalpha() and not unicodedata.combining(c))


def intervals(piece):
    pitches = [p for p, _ in piece["events"] if p]
    return tuple(b - a for a, b in zip(pitches, pitches[1:]))


def assign_splits(pieces, seed=1729):
    """Union titles, BWV identifiers, or very similar transposition-free melodies.

    Heuristic family grouping, not a claim to a complete musicological catalogue.
    All matching edges are recorded so the separation can be inspected.
    """
    parent = list(range(len(pieces)))
    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    fingerprints = [intervals(p) for p in pieces]
    edges = []
    for i, a in enumerate(pieces):
        for j in range(i):
            b = pieces[j]
            same = title_key(a["title"]) == title_key(b["title"])
            same_bwv = a["bwv"] is not None and a["bwv"] == b["bwv"]
            matcher = SequenceMatcher(None, fingerprints[i], fingerprints[j], autojunk=False)
            similar = (matcher.quick_ratio() >= .72 and matcher.ratio() >= .72)
            opening = (len(fingerprints[i]) >= 16 and len(fingerprints[j]) >= 16 and
                       fingerprints[i][:16] == fingerprints[j][:16])
            if same or same_bwv or similar or opening:
                parent[root(i)] = root(j)
                edges.append([a["id"], b["id"], "title" if same else "BWV" if same_bwv else
                              "opening" if opening else "interval similarity"])
    families = {}
    for i, p in enumerate(pieces):
        families.setdefault(root(i), []).append(p)
    groups = sorted(families.values(), key=lambda g: min(p["id"] for p in g))
    random.Random(seed).shuffle(groups)
    n = len(groups)
    for i, group in enumerate(groups):
        split = "train" if i < int(.75*n) else "validation" if i < int(.875*n) else "test"
        family = min(p["id"] for p in group)
        for p in group:
            p.update(family=family, split=split)
    return edges


def prepare(cache):
    directory = download(cache)
    parsed = Path(cache) / f"parsed-v{PREPROCESS_VERSION}.json"
    if parsed.exists():
        data = read_json(parsed)
        if data["revision"] == REVISION and data["pieces_sha256"] == digest(data["pieces"]):
            return data
        raise ValueError("parsed cache mismatch; remove it and prepare again")
    pieces, excluded = [], []
    for path in sorted(directory.glob("chor*.krn")):
        try:
            pieces.append(parse_score(path))
        except (ValueError, StopIteration) as error:
            excluded.append({"id": path.stem, "reason": str(error)})
    if len(pieces) < 300:
        raise ValueError(f"only {len(pieces)} scores parsed; inspect importer")
    edges = assign_splits(pieces)
    data = {"revision": REVISION, "archive_sha256": ARCHIVE_SHA256,
            "preprocess_version": PREPROCESS_VERSION, "split_seed": 1729,
            "pieces": pieces, "excluded": excluded, "family_edges": edges,
            "pieces_sha256": digest(pieces)}
    parsed.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n")
    return data


def encode(events, shift=0):
    result = []
    for pitch, duration in events:
        pitch = pitch + shift if pitch else 0
        if pitch and not MIN_PITCH <= pitch <= MAX_PITCH:
            raise ValueError("pitch outside vocabulary")
        if not 1 <= duration <= DURATIONS:
            raise ValueError("duration outside vocabulary")
        result.append([pitch - MIN_PITCH + 1 if pitch else 0, duration - 1])
    return result


def decode(tokens):
    return [[p + MIN_PITCH - 1 if p else 0, d + 1] for p, d in tokens]


def manifest(data):
    pieces = data["pieces"]
    return {k: v for k, v in data.items() if k != "pieces"} | {
        "counts": dict(Counter(p["split"] for p in pieces)),
        "family_counts": {s: len({p["family"] for p in pieces if p["split"] == s})
                          for s in ("train", "validation", "test")},
        "pieces": [{k: v for k, v in p.items() if k != "events"} |
                   {"events": len(p["events"]), "events_sha256": digest(p["events"])} for p in pieces]}
