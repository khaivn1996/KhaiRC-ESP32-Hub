"""In mục lục hoặc các mục được chọn của docs/PROJECT_SPEC.md (tiết kiệm ngữ cảnh).

Dùng:
  python tools/spec.py              mục lục kèm dung lượng từng mục
  python tools/spec.py 0 8 9        theo số mục (mục cấp 2 gồm cả các mục con)
  python tools/spec.py 5.5 6.2      theo số mục con
  python tools/spec.py OTA          theo từ khóa trong tiêu đề (không phân biệt hoa/thường)

Chỉ đọc file, không ghi gì. Cần Python 3.7+.
"""
import pathlib
import re
import sys

SPEC = pathlib.Path(__file__).resolve().parents[1] / "docs" / "PROJECT_SPEC.md"
HEADING = re.compile(r"^(#{2,3})\s+(?:(\d+(?:\.\d+)?)\.?\s+)?(.*)$")


def load():
    lines = SPEC.read_text(encoding="utf-8").split("\n")
    heads = []  # (chỉ số dòng, cấp, số mục hoặc "", tiêu đề)
    in_fence = False
    for i, line in enumerate(lines):
        if line.startswith("```"):
            in_fence = not in_fence
        match = None if in_fence else HEADING.match(line)
        if match:
            heads.append((i, len(match.group(1)), match.group(2) or "", match.group(3)))
    return lines, heads


def end_of(heads, n, total):
    level = heads[n][1]
    for j in range(n + 1, len(heads)):
        if heads[j][1] <= level:
            return heads[j][0]
    return total


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if not SPEC.exists():
        print(f"Không thấy {SPEC}", file=sys.stderr)
        return 1
    lines, heads = load()
    total = len(lines)
    if not argv:
        for n, (i, level, key, title) in enumerate(heads):
            size = sum(len(x.encode("utf-8")) + 1 for x in lines[i:end_of(heads, n, total)])
            print(f"{'  ' * (level - 2)}{key or '-':<5} {size:>6} B  {title}")
        print("\nĐọc mục: python tools/spec.py <số mục|từ khóa> ...")
        return 0
    chosen, missing = set(), []
    for query in argv:
        found = [n for n, h in enumerate(heads)
                 if (h[2] == query if re.fullmatch(r"\d+(?:\.\d+)?", query)
                     else query.lower() in h[3].lower())]
        if not found:
            missing.append(query)
        for n in found:
            chosen.update(range(heads[n][0], end_of(heads, n, total)))
    previous = None
    for i in sorted(chosen):
        if previous is not None and i != previous + 1:
            print()
        print(lines[i])
        previous = i
    if missing:
        print(f"\nKhông thấy mục: {', '.join(missing)} (chạy không đối số để xem mục lục)", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
