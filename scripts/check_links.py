"""Check repository-local Markdown and HTML links without contacting the network."""
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r'\]\(([^\s)]+)(?:\s+"[^"]*")?\)|(?:href|src)="([^"]+)"')
SKIP = {".git", ".venv", "build", "__pycache__"}


def main():
    count, errors = 0, []
    for path in ROOT.rglob("*.md"):
        if any(part in SKIP for part in path.relative_to(ROOT).parts):
            continue
        # Ignore fenced examples; this is a local path check, not a full Markdown parser.
        body = re.sub(r"```.*?```", "", path.read_text(), flags=re.S)
        for match in LINK.finditer(body):
            target = match.group(1) or match.group(2)
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            destination = (path.parent / unquote(parsed.path)).resolve()
            if not destination.is_relative_to(ROOT) or not destination.exists():
                errors.append(f"{path.relative_to(ROOT)}: {target}")
            count += 1
    if errors:
        raise SystemExit("Broken local links:\n" + "\n".join(errors))
    print(f"PASS: {count} local Markdown/image links (paths only; anchors and external URLs not checked)")


if __name__ == "__main__":
    main()
