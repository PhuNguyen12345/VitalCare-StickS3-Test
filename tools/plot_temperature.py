"""Vẽ biểu đồ thí nghiệm làm ấm: bằng chứng cho cách tính nhiệt độ cơ thể.

Đầu vào:
  --live   CSV từ `ble_client.py live --csv` (mỗi giây một dòng)
  --oral   CSV số đo nhiệt kế đo miệng: time,oral_c,note (time dạng HH:MM hoặc HH:MM:SS)
Đầu ra (thư mục --out):
  1_warmup_curve.png      Nhiệt độ da từ lúc đeo + đường cong làm ấm khớp dữ liệu
  2_body_temperature.png  Số thiết bị hiển thị, cách cộng thẳng, nhiệt kế; dải trạng thái màn hình
  3_offset.png            Độ lệch miệng - da theo thời gian
  4_error.png             Sai số so với nhiệt kế tại từng lần đo
  summary.md              Bảng số liệu (dạng bảng thay cho biểu đồ, dùng cho báo cáo)

Chạy: .venv/bin/python tools/plot_temperature.py --live <live.csv> --oral <oral.csv> --out <thư mục>
"""

import argparse
import csv
import math
from datetime import datetime, timedelta
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# ======================================================
# Màu (bảng màu chuẩn dataviz, 3 màu đầu: đã kiểm tra mù màu cho mọi cặp)
# ======================================================

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SERIES_DEVICE = "#2a78d6"   # số thiết bị hiển thị / nhiệt độ da đo được
SERIES_NAIVE = "#eb6834"    # cách cộng thẳng / mô hình
SERIES_ORAL = "#1baf7a"     # nhiệt kế đo miệng

# Dải trạng thái: thang xám có thứ tự, kèm chữ trên dải (không dùng màu trạng thái)
STATE_STYLE = {
    "measuring": ("#d9d8d3", "Measuring"),
    "settling": ("#b4b3ad", "Settling (số tạm thời)"),
    "stable": ("#7d7c76", "Stable"),
    "not_worn": ("#efeeea", "Not worn"),
    "unavailable": ("#efeeea", "Lỗi cảm biến"),
}

DEFAULT_OFFSET_C = 2.2
FIRMWARE_TAU_MIN = 6.0

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
    "grid.linestyle": "-",
    "axes.spines.top": False,
    "axes.spines.right": False,
    "legend.frameon": False,
})


# ======================================================
# Đọc dữ liệu
# ======================================================

def read_live(path):
    rows = []

    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows.append({
                "time": datetime.fromisoformat(row["host_time"]).replace(tzinfo=None),
                "skin": float(row["skin_c"]) if row["skin_c"] else None,
                "body": float(row["body_c"]) if row["body_c"] else None,
                "state": row["state"],
            })

    rows.sort(key=lambda r: r["time"])
    return rows


def read_oral(path, day):
    readings = []

    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if not row.get("time") or not row.get("oral_c"):
                continue

            parts = [int(p) for p in row["time"].strip().split(":")]
            moment = datetime.combine(day, datetime.min.time()) + timedelta(
                hours=parts[0], minutes=parts[1], seconds=parts[2] if len(parts) > 2 else 0
            )
            readings.append({"time": moment, "oral": float(row["oral_c"]), "note": (row.get("note") or "").strip()})

    return readings


def find_wear_window(rows, off_time=None, drop_c=0.3):
    """Lúc đeo: lần đầu có nhiệt độ da và không còn not_worn.
    Lúc tháo: giờ --off nếu có; nếu không, lúc da (trung bình 30 giây) giảm quá drop_c so với đỉnh.
    Sau khi tháo, da còn trên 30 °C một lúc nên firmware chưa báo not_worn ngay."""
    start = next((i for i, r in enumerate(rows) if r["skin"] is not None and r["state"] not in ("not_worn", "unavailable")), None)

    if start is None:
        raise SystemExit("Không tìm thấy đoạn đeo trong dữ liệu live")

    if off_time is not None:
        end = next((i for i in range(start, len(rows)) if rows[i]["time"] >= off_time), len(rows))
        return start, end

    peak = None
    peak_index = start

    for i in range(start, len(rows)):
        if rows[i]["state"] == "not_worn":
            return start, peak_index + 1

        window = [r["skin"] for r in rows[max(start, i - 29):i + 1] if r["skin"] is not None]
        if not window:
            continue

        smooth = sum(window) / len(window)

        if peak is None or smooth > peak:
            peak = smooth
            peak_index = i
        elif peak - smooth > drop_c:
            return start, peak_index + 1

    return start, len(rows)


# ======================================================
# Tính toán
# ======================================================

def rolling_mean(times, values, window_s=60):
    """Trung bình 60 giây gần nhất, giống firmware (dùng cho cách cộng thẳng)."""
    out = []
    first = 0

    for i, t in enumerate(times):
        while times[first] < t - window_s:
            first += 1
        window = [v for v in values[first:i + 1] if v is not None]
        out.append(sum(window) / len(window) if window else None)

    return out


def fit_warmup(minutes, skin):
    """Khớp T(t) = T_cb - A·e^(-t/τ). Dò τ, mỗi τ giải bình phương tối thiểu tuyến tính cho T_cb và A."""
    best = None

    for step in range(20, 2001):
        tau = step / 100
        xs = [math.exp(-m / tau) for m in minutes]
        n = len(xs)
        mean_x = sum(xs) / n
        mean_y = sum(skin) / n
        sxx = sum((x - mean_x) ** 2 for x in xs)

        if sxx == 0:
            continue

        slope = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, skin)) / sxx
        intercept = mean_y - slope * mean_x
        sse = sum((intercept + slope * x - y) ** 2 for x, y in zip(xs, skin))

        if best is None or sse < best["sse"]:
            best = {"tau": tau, "plateau": intercept, "amplitude": -slope, "sse": sse, "rmse": math.sqrt(sse / n)}

    return best


def value_at(rows_time, values, moment):
    """Giá trị gần thời điểm moment nhất (trong vòng 5 giây)."""
    best = None

    for t, v in zip(rows_time, values):
        if v is None:
            continue
        gap = abs((t - moment).total_seconds())
        if gap <= 5 and (best is None or gap < best[0]):
            best = (gap, v)

    return None if best is None else best[1]


# ======================================================
# Vẽ
# ======================================================

def finish(fig, ax, title, subtitle, path):
    ax.set_title(title, loc="left", fontsize=13, color=TEXT, fontweight="bold", pad=22)
    ax.text(0, 1.02, subtitle, transform=ax.transAxes, fontsize=9.5, color=TEXT_SECONDARY, va="bottom")
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


def end_label(ax, x, y, text, color):
    ax.plot([x], [y], "o", ms=7, color=color, mec=SURFACE, mew=2, zorder=5)
    ax.annotate(text, (x, y), xytext=(8, 0), textcoords="offset points", va="center", fontsize=9, color=TEXT)


def plot_warmup(minutes, skin, fit, path, after_minutes=None, after_skin=None):
    fig, ax = plt.subplots(figsize=(9, 4.8))

    ax.plot(minutes, skin, color=SERIES_DEVICE, lw=2, label="Nhiệt độ da đo được (MAX30205)")

    model = [fit["plateau"] - fit["amplitude"] * math.exp(-m / fit["tau"]) for m in minutes]
    ax.plot(minutes, model, color=SERIES_NAIVE, lw=2, label=f"Mô hình làm ấm, τ = {fit['tau']:.1f} phút")

    if after_minutes:
        ax.plot(after_minutes, after_skin, color=SERIES_DEVICE, lw=2, alpha=0.45)
        ax.annotate("Tháo ra", (after_minutes[0], after_skin[0]), xytext=(6, 10), textcoords="offset points",
                    fontsize=9, color=TEXT_SECONDARY)

    ax.axhline(fit["plateau"], color=TEXT_SECONDARY, lw=1)
    ax.annotate(f"Cân bằng ≈ {fit['plateau']:.2f} °C", (minutes[0], fit["plateau"]), xytext=(4, 6),
                textcoords="offset points", ha="left", fontsize=9, color=TEXT_SECONDARY)

    # Thời điểm đạt 95% quãng tăng: 3τ
    t95 = 3 * fit["tau"]
    if t95 <= minutes[-1]:
        ax.axvline(t95, color=GRID, lw=1.5)
        ax.annotate(f"95% quãng tăng sau ≈ {t95:.0f} phút", (t95, 0.04), xycoords=("data", "axes fraction"),
                    xytext=(6, 0), textcoords="offset points", fontsize=9, color=TEXT_SECONDARY)

    ax.set_xlabel("Phút kể từ lúc đeo")
    ax.set_ylabel("Nhiệt độ da (°C)")
    ax.legend(loc="center right")
    finish(fig, ax, "Da dưới cảm biến ấm lên theo quy luật đoán trước được",
           f"Đường cong khớp mô hình T(t) = T_cb − A·e^(−t/τ), sai số khớp {fit['rmse']:.2f} °C. "
           "Từ độ dốc ở phút đầu có thể dự đoán giá trị cân bằng.", path)


def plot_body(minutes, body, naive, states, oral_points, path):
    fig, (ax, band) = plt.subplots(2, 1, figsize=(9, 5.6), sharex=True,
                                   gridspec_kw={"height_ratios": [6, 1], "hspace": 0.08})

    ax.plot(minutes, naive, color=SERIES_NAIVE, lw=2, label=f"Cộng thẳng: trung bình da 1 phút + {DEFAULT_OFFSET_C} °C")
    # Thiết bị cập nhật số 10 giây một lần: vẽ dạng bậc thang
    ax.plot(minutes, body, color=SERIES_DEVICE, lw=2, drawstyle="steps-post", label="Thiết bị hiển thị (có dự đoán)")

    if oral_points:
        xs = [p["minute"] for p in oral_points]
        ys = [p["oral"] for p in oral_points]
        ax.plot(xs, ys, "o", ms=8, color=SERIES_ORAL, mec=SURFACE, mew=2, zorder=6, label="Nhiệt kế đo miệng")
        for x, y in zip(xs, ys):
            ax.annotate(f"{y:.1f}", (x, y), xytext=(0, 9), textcoords="offset points", ha="center", fontsize=8.5, color=TEXT)

    ax.set_ylabel("Nhiệt độ cơ thể (°C)")
    ax.legend(loc="lower right")

    # Dải trạng thái màn hình
    segments = []
    for m, s in zip(minutes, states):
        if segments and segments[-1][2] == s:
            segments[-1][1] = m
        else:
            segments.append([m, m, s])

    for start, end, state in segments:
        color, label = STATE_STYLE.get(state, ("#efeeea", state))
        band.axvspan(start, max(end, start + 0.05), color=color, lw=0)
        if end - start >= 1.5:
            band.text((start + end) / 2, 0.5, label, ha="center", va="center", fontsize=8.5,
                      color=SURFACE if state == "stable" else TEXT)

    band.set_yticks([])
    band.grid(False)
    band.set_ylabel("Màn hình", rotation=0, ha="right", va="center")
    band.set_xlabel("Phút kể từ lúc đeo")

    finish(fig, ax, "Nhiệt độ cơ thể: thiết bị, cách cộng thẳng và nhiệt kế đo miệng",
           "Cộng thẳng = trung bình da 1 phút + độ lệch; thiết bị bù thêm phần da chưa ấm bằng độ dốc nhiệt độ da.",
           path)


def plot_offset(oral_points, path):
    fig, ax = plt.subplots(figsize=(9, 4.2))

    xs = [p["minute"] for p in oral_points if p["skin_avg"] is not None]
    ys = [p["oral"] - p["skin_avg"] for p in oral_points if p["skin_avg"] is not None]

    ax.plot(xs, ys, color=SERIES_ORAL, lw=2, marker="o", ms=8, mec=SURFACE, mew=2)
    for x, y in zip(xs, ys):
        ax.annotate(f"{y:.2f}", (x, y), xytext=(0, 9), textcoords="offset points", ha="center", fontsize=8.5, color=TEXT)

    ax.axhline(DEFAULT_OFFSET_C, color=TEXT_SECONDARY, lw=1)
    ax.annotate(f"Độ lệch mặc định {DEFAULT_OFFSET_C} °C", (xs[0] if xs else 0, DEFAULT_OFFSET_C), xytext=(0, 6),
                textcoords="offset points", ha="left", fontsize=9, color=TEXT_SECONDARY)

    ax.set_xlabel("Phút kể từ lúc đeo")
    ax.set_ylabel("Miệng − da (°C)")
    finish(fig, ax, "Độ lệch giữa thân nhiệt và da hội tụ khi da đã ấm",
           "Chỉ khi đã cân bằng nhiệt, độ lệch mới ổn định; đó là lý do chỉ cho hiệu chuẩn ở trạng thái Stable.", path)


def plot_error(oral_points, path):
    points = [p for p in oral_points if p["body"] is not None and p["naive"] is not None]
    fig, ax = plt.subplots(figsize=(9, 4.2))

    labels = [f"{p['minute']:.0f}'" for p in points]
    x = list(range(len(points)))
    width = 0.36
    device_err = [p["body"] - p["oral"] for p in points]
    naive_err = [p["naive"] - p["oral"] for p in points]

    ax.bar([i - width / 2 - 0.02 for i in x], naive_err, width, color=SERIES_NAIVE, label="Cộng thẳng")
    ax.bar([i + width / 2 + 0.02 for i in x], device_err, width, color=SERIES_DEVICE, label="Thiết bị (có dự đoán)")

    def signed(value):
        return "0.0" if abs(value) < 0.05 else f"{value:+.1f}"

    for i, (n, d) in enumerate(zip(naive_err, device_err)):
        ax.annotate(signed(n), (i - width / 2 - 0.02, n), xytext=(0, 3 if n >= 0 else -11), textcoords="offset points",
                    ha="center", fontsize=8, color=TEXT)
        ax.annotate(signed(d), (i + width / 2 + 0.02, d), xytext=(0, 3 if d >= 0 else -11), textcoords="offset points",
                    ha="center", fontsize=8, color=TEXT)

    ax.axhline(0, color=TEXT_SECONDARY, lw=1)
    ax.set_xticks(x, labels)
    ax.set_xlabel("Thời điểm đo (phút kể từ lúc đeo)")
    ax.set_ylabel("Lệch so với nhiệt kế (°C)")
    ax.legend(loc="lower right")
    finish(fig, ax, "Sai số so với nhiệt kế đo miệng",
           "Dương: thiết bị cao hơn nhiệt kế. Âm: thấp hơn.", path)


# ======================================================
# Chương trình chính
# ======================================================

def main():
    parser = argparse.ArgumentParser(description="Vẽ biểu đồ thí nghiệm nhiệt độ")
    parser.add_argument("--live", required=True)
    parser.add_argument("--oral", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--off", help="Giờ tháo thiết bị HH:MM[:SS] (mặc định: tự nhận khi da giảm rõ)")
    args = parser.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    rows = read_live(args.live)
    off_time = None

    if args.off:
        parts = [int(p) for p in args.off.split(":")]
        off_time = datetime.combine(rows[0]["time"].date(), datetime.min.time()) + timedelta(
            hours=parts[0], minutes=parts[1], seconds=parts[2] if len(parts) > 2 else 0
        )

    start, end = find_wear_window(rows, off_time)
    t0 = rows[start]["time"]
    worn = rows[start:end]

    minutes = [(r["time"] - t0).total_seconds() / 60 for r in worn]
    times = [r["time"] for r in worn]
    skin = [r["skin"] for r in worn]
    skin_avg = rolling_mean([(t - t0).total_seconds() for t in times], skin)
    naive = [None if v is None else v + DEFAULT_OFFSET_C for v in skin_avg]
    body = [r["body"] for r in worn]
    states = [r["state"] for r in worn]

    fit_points = [(m, s) for m, s in zip(minutes, skin) if s is not None]
    fit = fit_warmup([m for m, _ in fit_points], [s for _, s in fit_points])

    after = rows[end:end + 300]
    plot_warmup(
        [m for m, _ in fit_points],
        [s for _, s in fit_points],
        fit,
        out / "1_warmup_curve.png",
        [(r["time"] - t0).total_seconds() / 60 for r in after if r["skin"] is not None],
        [r["skin"] for r in after if r["skin"] is not None],
    )

    oral_points = []
    for reading in read_oral(args.oral, t0.date()):
        minute = (reading["time"] - t0).total_seconds() / 60
        if minute < 0 or reading["time"] > times[-1]:
            continue
        oral_points.append({
            "minute": minute,
            "oral": reading["oral"],
            "note": reading["note"],
            "body": value_at(times, body, reading["time"]),
            "naive": value_at(times, naive, reading["time"]),
            "skin_avg": value_at(times, skin_avg, reading["time"]),
        })

    plot_body(minutes, body, naive, states, oral_points, out / "2_body_temperature.png")

    if oral_points:
        plot_offset(oral_points, out / "3_offset.png")
        plot_error(oral_points, out / "4_error.png")

    # Thời điểm chuyển trạng thái
    first_estimate = next((m for m, b in zip(minutes, body) if b is not None), None)
    first_stable = next((m for m, s in zip(minutes, states) if s == "stable"), None)

    lines = [
        "# Kết quả thí nghiệm làm ấm",
        "",
        f"- Bắt đầu đeo: {t0:%Y-%m-%d %H:%M:%S}; thời gian đeo: {minutes[-1]:.1f} phút",
        f"- Mô hình làm ấm: τ = {fit['tau']:.2f} phút, da cân bằng ≈ {fit['plateau']:.2f} °C, "
        f"biên độ {fit['amplitude']:.2f} °C, sai số khớp {fit['rmse']:.3f} °C (firmware đang dùng τ = {FIRMWARE_TAU_MIN} phút)",
        f"- Có số đầu tiên sau: {first_estimate:.1f} phút" if first_estimate is not None else "- Chưa có số trong lúc đeo",
        f"- Chuyển sang Stable sau: {first_stable:.1f} phút" if first_stable is not None else "- Chưa đạt Stable",
        "",
        "| Phút | Nhiệt kế (°C) | Thiết bị (°C) | Lệch thiết bị | Cộng thẳng (°C) | Lệch cộng thẳng | Miệng − da TB 1 phút | Ghi chú |",
        "|---|---|---|---|---|---|---|---|",
    ]

    def fmt(value, sign=False):
        if value is None:
            return "—"
        return f"{value:+.2f}" if sign else f"{value:.2f}"

    for p in oral_points:
        device_err = None if p["body"] is None else p["body"] - p["oral"]
        naive_err = None if p["naive"] is None else p["naive"] - p["oral"]
        offset = None if p["skin_avg"] is None else p["oral"] - p["skin_avg"]
        lines.append(
            f"| {p['minute']:.1f} | {p['oral']:.1f} | {fmt(p['body'])} | {fmt(device_err, True)} | {fmt(p['naive'])} | "
            f"{fmt(naive_err, True)} | {fmt(offset)} | {p['note']} |"
        )

    (out / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
