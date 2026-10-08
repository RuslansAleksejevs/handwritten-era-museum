"""Verify the recovered-page map, not official grader acceptance."""
import json
from pathlib import Path

if not __debug__:
    raise SystemExit("assert-based checks need Python without -O or PYTHONOPTIMIZE")

ROOT = Path(__file__).resolve().parents[1]
BELTS = ROOT / "projects/cpp-belts"


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read(path):
    return json.loads(path.read_text(), object_pairs_hook=unique)


def main():
    sources = read(BELTS / "exercises-index.json")["entries"]
    index = {entry["source_path"]: entry for entry in sources}
    assert len(index) == len(sources) == 189
    seen = set()
    statuses = {"LOCAL_CONTRACT_TESTED", "IMPLEMENTED_TESTED", "ADAPTED_TESTED"}
    for belt in ["white", "yellow", "red", "brown", "black"]:
        coverage = read(BELTS / "course" / belt / "coverage.json")
        for entry in coverage["entries"]:
            source = entry["source_path"]
            assert source in index and source not in seen, source
            seen.add(source)
            assert entry["status"] in statuses, (source, entry["status"])
            assert entry["source_url"] == index[source]["source_url"], source
            paths = entry.get("implementation", entry.get("code", []))
            assert paths and entry["tests"], source
            for name in paths + entry["tests"]:
                path = (ROOT / name.split("::", 1)[0]).resolve()
                assert path.is_relative_to(ROOT) and path.is_file(), (source, name)
            assert index[source]["modern_solution"] == entry["status"], source
    assert seen == set(index), set(index)-seen
    print(f"Course coverage: {len(seen)} recovered pages, five belts, implementation/test paths PASS")


if __name__ == "__main__":
    main()
