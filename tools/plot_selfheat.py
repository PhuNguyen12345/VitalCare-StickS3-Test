"""Vẽ biểu đồ thí nghiệm A1: thiết bị nằm trên bàn có tự làm nóng cảm biến không.

Đầu vào:
  --logs     các file CSV segment đã sync (ble_logs/seg_*.csv), có thể dùng glob
  --live     CSV từ `ble_client.py live --csv` (xác định lúc bắt đầu live)
  --from/--to  khoảng thời gian cần vẽ (HH:MM, giờ địa phương, cùng ngày với --live)
  --unplug   lúc rút USB (HH:MM[:SS])
  --tz       múi giờ phút (mặc định +420)
Đầu ra (thư mục --out):
  a1_selfheat.png   Nhiệt độ cảm biến theo thời gian, chia giai đoạn sạc / không live / live
  summary.md        Bảng mức nhiệt trung bình từng giai đoạn

Chạy:
  .venv/bin/python tools/plot_selfheat.py --logs "ble_logs/seg_0006[45]*.csv" \
      --live docs/experiments/2026-11-14_selfheat/live_on.csv \
      --from 21:55 --unplug 23:00 --out docs/experiments/2026-11-14_selfheat/charts
"""

import argparse
import csv
import glob
from datetime import datetime, timedelta, timezone
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.dates as mdates  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402

# ======================================================
# Màu (cùng hệ với plot_temperature.py)
# ======================================================

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SERIES_SENSOR = "#2a78d6"

# Giai đoạn: thang xám có thứ tự, kèm chữ trên dải
PHASE_STYLE = {
    "charging": ("#e4e3df", "Đang sạc"),
    "idle": ("#f3f2ee", "Rút USB\nkhông live"),
    "live": ("#c9c8c2", "Bật\nBLE live"),
}

plt.rcParams.update({
    "font.family": ["Arial", "Helvetica", "DejaVu Sans"],
    "font.size": 10,
    "axes.facecolor": SURFACE,
    "figure.facecolor": SURFACE,
    "axes.edgecolor": GRID,
    "axes.labelcolor": TEXT_SECONDARY,
    "xtick.color": TEXT_SECONDARY,
    "ytick.color": TEXT_SECONDARY,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "axes.spines.top": False,
    "axes.spines.right": False,
})


# ======================================================
# Đọc dữ liệu
# ======================================================

def read_segments(pattern, tz):
    rows = {}

    for path in sorted(glob.glob(pattern)):
        with open(path, encoding="utf-8") as f:
            for line in f:
                fields = line.strip().split(",")

                if not fields[0].isdigit() or len(fields) < 2:
                    continue

                t100 = int(fields[1])

                if t100 == 0:
                    continue

                moment = datetime.fromtimestamp(int(fields[0]), tz).replace(tzinfo=None)
                rows[moment] = t100 / 100.0

    return sorted(rows.items())


def live_start(path):
    with open(path, encoding="utf-8") as f:
        first = next(csv.DictReader(f))

    return datetime.fromisoformat(first["host_time"]).replace(tzinfo=None)


def at(day, text):
    parts = [int(p) for p in text.split(":")] + [0]
    return day.replace(hour=parts[0], minute=parts[1], second=parts[2], microsecond=0)


def mean_between(rows, start, end):
    values = [v for t, v in rows if start <= t < end]
    return sum(values) / len(values) if values else float("nan")


# ======================================================
# Vẽ
# ======================================================

def plot(rows, phases, levels, path):
    fig, ax = plt.subplots(figsize=(9, 4.6))

    low = min(v for _, v in rows) - 0.4
    high = max(v for _, v in rows) + 0.5

    for name, start, end in phases:
        color, label = PHASE_STYLE[name]
        ax.axvspan(start, end, color=color, linewidth=0, zorder=0)
        ax.text(
            start + (end - start) / 2,
            high - 0.12,
            label,
            ha="center",
            va="top",
            fontsize=9,
            color=TEXT_SECONDARY,
        )

    times = [t for t, _ in rows]
    values = [v for _, v in rows]
    ax.step(times, values, where="post", color=SERIES_SENSOR, linewidth=2)

    for label, start, end, value in levels:
        ax.hlines(value, start, end, colors=TEXT, linewidths=1, linestyles="--")
        ax.text(
            end,
            value + 0.06,
            f"{label}: {value:.2f} °C",
            ha="right",
            va="bottom",
            fontsize=9,
            color=TEXT,
        )

    ax.set_ylim(low, high)
    ax.set_ylabel("Nhiệt độ cảm biến MAX30205 (°C)")
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))

    fig.suptitle(
        "A1: thiết bị nằm trên bàn, bật BLE live làm cảm biến nóng thêm",
        x=0.01,
        ha="left",
        fontsize=12,
        color=TEXT,
        fontweight="bold",
    )
    ax.set_title(
        "Bản ghi trung bình 10 giây từ LittleFS (đã sync), không đeo trên tay",
        loc="left",
        fontsize=9,
        color=TEXT_SECONDARY,
    )
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


# ======================================================
# Chương trình chính
# ======================================================

def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--logs", required=True)
    parser.add_argument("--live", required=True)
    parser.add_argument("--from", dest="start", required=True)
    parser.add_argument("--to", dest="end")
    parser.add_argument("--unplug", required=True)
    parser.add_argument("--tz", type=int, default=420)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    tz = timezone(timedelta(minutes=args.tz))
    live_at = live_start(args.live)
    day = live_at.replace(hour=0, minute=0, second=0, microsecond=0)
    start = at(day, args.start)
    unplug = at(day, args.unplug)

    rows = [(t, v) for t, v in read_segments(args.logs, tz) if t >= start]
    end = at(day, args.end) if args.end else rows[-1][0]
    rows = [(t, v) for t, v in rows if t <= end]

    phases = [
        ("charging", start, unplug),
        ("idle", unplug, live_at),
        ("live", live_at, end),
    ]

    # Mức so sánh: 3 phút cuối của mỗi giai đoạn (đã gần cân bằng nhất)
    window = timedelta(minutes=3)
    levels = []

    for label, phase_end in (("Sạc", unplug), ("Không live", live_at), ("Live", end)):
        levels.append((label, phase_end - window, phase_end, mean_between(rows, phase_end - window, phase_end)))

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    plot(rows, phases, levels, out / "a1_selfheat.png")

    idle_c = levels[1][3]
    live_c = levels[2][3]
    lines = [
        "# A1: tự làm nóng cảm biến (thiết bị nằm trên bàn)",
        "",
        f"- Bắt đầu live: {live_at:%H:%M:%S}; rút USB: {unplug:%H:%M}",
        "",
        "| Giai đoạn (3 phút cuối) | Nhiệt độ cảm biến |",
        "|---|---|",
    ]
    lines += [f"| {label} | {value:.2f} °C |" for label, _, _, value in levels]
    lines += [
        "",
        f"**Live làm cảm biến nóng thêm: {live_c - idle_c:+.2f} °C** "
        "(mức không live vẫn đang nguội nên đây là giá trị tối thiểu).",
        "",
    ]
    (out / "summary.md").write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
