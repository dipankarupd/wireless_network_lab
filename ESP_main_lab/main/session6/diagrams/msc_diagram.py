"""Generate the Session 6 message sequence diagram (ideal path) as SVG.

    python3 msc_diagram.py .
    swift svg2png.swift msc_ideal_path.svg msc_ideal_path.png 1800 <height printed by this script>
"""
import sys
from html import escape

FONT = "Helvetica Neue, Helvetica, Arial, sans-serif"
INK = "#202124"
GREY = "#5f6368"
I_COL, R_COL = "#1a73e8", "#188038"

W = 1800
X_I, X_R = 560, 1240          # lifelines
NOTE_W = 430
LINE_H = 25


class Svg:
    def __init__(self):
        self.parts = []

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=18, anchor="start", weight="normal", fill=INK, italic=False):
        style = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{size}" '
                 f'font-weight="{weight}" text-anchor="{anchor}" fill="{fill}"{style}>{escape(s)}</text>')

    def save(self, path, h):
        with open(path, "w") as f:
            f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{h}" viewBox="0 0 {W} {h}">\n'
                    f'<rect width="{W}" height="{h}" fill="white"/>\n')
            f.write("\n".join(self.parts))
            f.write("\n</svg>\n")


def text_w(s, size):
    return len(s) * size * 0.56


def note(svg, y, side, lines, color):
    """Local-computation box next to a lifeline. Returns the y below the box."""
    h = 16 + LINE_H * len(lines)
    if side == "I":
        x0 = X_I - 22 - NOTE_W
    else:
        x0 = X_R + 22
    for ln in lines:
        assert text_w(ln, 17) < NOTE_W - 24, f"note line too long: {ln!r}"
    svg.add(f'<rect x="{x0}" y="{y}" width="{NOTE_W}" height="{h}" rx="8" fill="{color}" fill-opacity="0.08" '
            f'stroke="{color}" stroke-width="1.5"/>')
    for k, ln in enumerate(lines):
        svg.text(x0 + 14, y + 10 + LINE_H * (k + 0.75), ln, size=17)
    # tick from the box to the lifeline
    lx = X_I if side == "I" else X_R
    bx = x0 + NOTE_W if side == "I" else x0
    svg.add(f'<line x1="{bx}" y1="{y + h / 2}" x2="{lx}" y2="{y + h / 2}" stroke="{color}" stroke-width="1.5" '
            f'stroke-dasharray="4 4"/>')
    return y + h


def message(svg, y, frm, label, sub=None):
    """Arrow between the lifelines with a label above it. Returns the y below it."""
    x1, x2 = (X_I, X_R) if frm == "I" else (X_R, X_I)
    color = I_COL if frm == "I" else R_COL
    y_arrow = y + 38
    svg.text((X_I + X_R) / 2, y + 26, label, size=19, anchor="middle", weight="bold", fill=color)
    svg.add(f'<line x1="{x1}" y1="{y_arrow}" x2="{x2 - (14 if x2 > x1 else -14)}" y2="{y_arrow}" '
            f'stroke="{color}" stroke-width="3"/>')
    d = 1 if x2 > x1 else -1
    svg.add(f'<polygon points="{x2},{y_arrow} {x2 - d * 16},{y_arrow - 7} {x2 - d * 16},{y_arrow + 7}" fill="{color}"/>')
    if sub:
        svg.text((X_I + X_R) / 2, y_arrow + 24, sub, size=15, anchor="middle", fill=GREY, italic=True)
        return y_arrow + 40
    return y_arrow + 22


def band(svg, y, text, size=18):
    svg.add(f'<rect x="{X_I - 140}" y="{y}" width="{X_R - X_I + 280}" height="40" rx="6" fill="#f1f3f4" '
            f'stroke="#dadce0"/>')
    svg.text((X_I + X_R) / 2, y + 26, text, size=size, anchor="middle", fill=INK)
    return y + 40


def build(path):
    s = Svg()
    s.text(40, 52, "P2P protocol — message sequence diagram of the ideal path", size=32, weight="bold")
    s.text(40, 86, "Every message arrives, every check passes. Boxes = local computations; arrows = frames on the air. "
           "Example values: T = 5.", size=19, fill=GREY)

    # participant headers
    top = 120
    for x, name, mac, col in ((X_I, "Initiator I", "a_I = 48:f6:ee:c7:1f:18", I_COL),
                              (X_R, "Responder R", "a_R = 48:f6:ee:c7:20:f0", R_COL)):
        s.add(f'<rect x="{x - 170}" y="{top}" width="340" height="70" rx="10" fill="{col}" fill-opacity="0.12" '
              f'stroke="{col}" stroke-width="2.5"/>')
        s.text(x, top + 30, name, size=24, anchor="middle", weight="bold")
        s.text(x, top + 56, mac, size=17, anchor="middle", fill=GREY)

    y = top + 90
    y = band(s, y, "(0) pre-shared by both:  n0 = 0x3A7F12C4,  T,  a_I,  a_R,  Ascon key K (16 bytes)") + 24

    # (1) I starts
    y = note(s, y, "I", ["START event (board boots)",
                         "k ← 0,  n ← n0",
                         "nonce ← 16 random bytes (TRNG)",
                         "body ← Ascon-encrypt([n]) + tag",
                         "start timer W = 2 s;  → I_WAIT_PONG"], I_COL) + 8
    y = message(s, y, "I", "(1)  PING  ⟨a_I, nonce⟩ [ n0 ]", "n0 = 3a 7f 12 c4") + 10

    # (2) R answers the first PING
    y = note(s, y, "R", ["verify Ascon tag  ✓",
                         "ã_I = a_I  ✓    ñ = n0  ✓  (new session)",
                         "k ← 1,  e ← ñ + 2   (next PING expected)",
                         "reply with ñ + 1;  start timer W",
                         "→ R_IN_SESSION"], R_COL) + 8
    y = message(s, y, "R", "(2)  PONG  ⟨a_R, nonce⟩ [ n0+1 ]", "3a 7f 12 c5") + 10

    # (3) I checks and continues
    y = note(s, y, "I", ["stop timer;  verify Ascon tag  ✓",
                         "ã_R = a_R  ✓    ñ = n + 1  ✓",
                         "k ← k + 1 = 1;   k < T → continue",
                         "n ← ñ + 1;  send PING;  restart timer W"], I_COL) + 8
    y = message(s, y, "I", "(3)  PING  ⟨a_I, nonce⟩ [ n0+2 ]", "3a 7f 12 c6") + 10

    y = note(s, y, "R", ["verify tag  ✓   ã_I = a_I  ✓   ñ = e  ✓",
                         "k ← 2,  e ← ñ + 2",
                         "reply with ñ + 1;  restart timer W"], R_COL) + 8
    y = message(s, y, "R", "PONG  ⟨a_R, nonce⟩ [ n0+3 ]", "3a 7f 12 c7") + 16

    y = band(s, y, "(4)  …  the exchange (2)–(3) repeats: I sends n0 + 2(k−1), R answers n0 + 2k − 1  …") + 16

    y = message(s, y, "I", "PING  ⟨a_I, nonce⟩ [ n0 + 2T − 2 ]", "last round, T = 5:  3a 7f 12 cc") + 8
    y = note(s, y, "R", ["verify  ✓    ñ = e  ✓",
                         "k ← T,  e ← ñ + 2"], R_COL) + 8
    y = message(s, y, "R", "PONG  ⟨a_R, nonce⟩ [ n0 + 2T − 1 ]", "3a 7f 12 cd") + 10

    y = note(s, y, "I", ["stop timer;  verify  ✓    ñ = n + 1  ✓",
                         "k ← k + 1 = T   → finished",
                         "send END with ñ + 1"], I_COL) + 8
    y = message(s, y, "I", "(4)  END  ⟨a_I, nonce⟩ [ n0 + 2T ]", "3a 7f 12 ce") + 10

    y_end = y
    note(s, y, "I", ["→ I_DONE", "print SUMMARY: SUCCESS, T / T rounds"], I_COL)
    y = note(s, y, "R", ["verify  ✓    ñ = e  ✓", "stop timer;  → R_IDLE",
                         "print SUMMARY: SUCCESS, T / T rounds"], R_COL) + 20

    # lifelines (drawn last but behind is fine: thin dashed)
    for x, col in ((X_I, I_COL), (X_R, R_COL)):
        s.parts.insert(1, f'<line x1="{x}" y1="{top + 70}" x2="{x}" y2="{y}" stroke="{col}" stroke-width="2" '
                          f'stroke-dasharray="7 6"/>')

    # legend
    y += 20
    s.add(f'<line x1="40" y1="{y}" x2="{W - 40}" y2="{y}" stroke="#dadce0" stroke-width="1.5"/>')
    y += 36
    legend = [
        "⟨ … ⟩  associated data: sent readable, but covered by the Ascon tag.     [ … ]  encrypted.   "
        "Every frame also carries magic 0x6A, the message type and the 16-byte tag.",
        "x̃ (tilde) = the value as received, not yet trusted (lab text convention).     "
        "k = completed rounds,  n = counter in I's last PING,  e = counter R expects next.",
        "Counter rule: I's messages carry n0 + even offsets, R's carry odd offsets; every message is the previous one + 1.  "
        "Without encryption (P2P_USE_AEAD 0) the same flow runs with a zero nonce, plain counter and no tag.",
    ]
    for ln in legend:
        s.text(40, y, ln, size=16, fill=INK)
        y += 28
    h = int(y + 10)
    s.save(path, h)
    return h


if __name__ == "__main__":
    out = sys.argv[1]
    print("height", build(f"{out}/msc_ideal_path.svg"))
