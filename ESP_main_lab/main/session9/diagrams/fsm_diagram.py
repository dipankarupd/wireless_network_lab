"""Generate the Session 9 MP-SAE combined A/B state diagram as SVG.

    python3 fsm_diagram.py .
    swift svg2png.swift fsm_sae.svg fsm_sae.png 1900 <height this script prints>
"""
import math
import sys
from html import escape

FONT = "Helvetica Neue, Helvetica, Arial, sans-serif"
INK = "#202124"
COL = {"normal": "#3c4043", "accept": "#188038", "drop": "#e37400", "timeout": "#7b1fa2", "error": "#c5221f"}
A_COL, B_COL = "#1a73e8", "#188038"
W = 1900


class Svg:
    def __init__(self, h):
        self.h = h
        self.parts = [f'<rect width="{W}" height="{h}" fill="white"/>']

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=18, anchor="start", weight="normal", fill=INK, italic=False):
        style = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{size}" '
                 f'font-weight="{weight}" text-anchor="{anchor}" fill="{fill}"{style}>{escape(s)}</text>')

    def save(self, path):
        with open(path, "w") as f:
            f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{self.h}" '
                    f'viewBox="0 0 {W} {self.h}">\n' + "\n".join(self.parts) + "\n</svg>\n")


def text_w(s, size):
    return len(s) * size * 0.56


def unit(dx, dy):
    d = math.hypot(dx, dy)
    return dx / d, dy / d


def head(svg, tip, frm, color, size=15):
    ux, uy = unit(tip[0] - frm[0], tip[1] - frm[1])
    bx, by = tip[0] - ux * size, tip[1] - uy * size
    px, py = -uy * size * 0.45, ux * size * 0.45
    svg.add(f'<polygon points="{tip[0]:.1f},{tip[1]:.1f} {bx+px:.1f},{by+py:.1f} {bx-px:.1f},{by-py:.1f}" fill="{color}"/>')


def circle(svg, c, r, name, fill, stroke, final=False, sub=None):
    svg.add(f'<circle cx="{c[0]}" cy="{c[1]}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="3"/>')
    if final:
        svg.add(f'<circle cx="{c[0]}" cy="{c[1]}" r="{r-8}" fill="none" stroke="{stroke}" stroke-width="1.8"/>')
    svg.text(c[0], c[1] + (0 if sub else 6), name, size=19, anchor="middle", weight="bold")
    if sub:
        svg.text(c[0], c[1] + 23, sub, size=13, anchor="middle", fill="#5f6368", italic=True)


def vline(svg, c1, r1, c2, r2, color, label=None, num=None, dx=12):
    ux, uy = unit(c2[0]-c1[0], c2[1]-c1[1])
    p1 = (c1[0]+ux*r1, c1[1]+uy*r1); p2 = (c2[0]-ux*r2, c2[1]-uy*r2)
    svg.add(f'<line x1="{p1[0]:.1f}" y1="{p1[1]:.1f}" x2="{p2[0]:.1f}" y2="{p2[1]:.1f}" stroke="{color}" stroke-width="2.6"/>')
    head(svg, p2, p1, color)
    if label:
        my = (p1[1]+p2[1])/2
        if num is not None:
            svg.add(f'<circle cx="{c1[0]+dx:.1f}" cy="{my:.1f}" r="12" fill="{color}"/>')
            svg.text(c1[0]+dx, my+4, str(num), size=13, anchor="middle", weight="bold", fill="white")
            svg.text(c1[0]+dx+20, my+5, label, size=15, fill=color, weight="bold")
        else:
            svg.text(c1[0]+dx, my+5, label, size=15, fill=color, weight="bold")


def cross(svg, p1, p2, color, label, num, lx, ly, anchor="middle"):
    """Horizontal-ish arrow between the two columns (a sent message)."""
    svg.add(f'<line x1="{p1[0]:.1f}" y1="{p1[1]:.1f}" x2="{p2[0]:.1f}" y2="{p2[1]:.1f}" '
            f'stroke="{color}" stroke-width="2.6" stroke-dasharray="1,0"/>')
    head(svg, p2, p1, color)
    badge = 13
    tw = text_w(label, 16)
    total = 2*badge+8+tw
    x0 = lx - total/2 if anchor == "middle" else lx
    svg.add(f'<rect x="{x0-6:.1f}" y="{ly-19:.1f}" width="{total+12:.1f}" height="34" rx="7" '
            f'fill="white" fill-opacity="0.95" stroke="{color}" stroke-width="1.2"/>')
    svg.add(f'<circle cx="{x0+badge:.1f}" cy="{ly-2:.1f}" r="{badge}" fill="{color}"/>')
    svg.text(x0+badge, ly+3, str(num), size=14, anchor="middle", weight="bold", fill="white")
    svg.text(x0+2*badge+8, ly+3, label, size=16, weight="bold", fill=color)


def loop(svg, c, r, angle, color, num, label, size=90, spread=24, ly_off=0, dist=1.9):
    a1, a2 = angle-spread, angle+spread
    p1 = (c[0]+r*math.cos(math.radians(a1)), c[1]-r*math.sin(math.radians(a1)))
    p2 = (c[0]+r*math.cos(math.radians(a2)), c[1]-r*math.sin(math.radians(a2)))
    d1 = (math.cos(math.radians(a1-8)), -math.sin(math.radians(a1-8)))
    d2 = (math.cos(math.radians(a2+8)), -math.sin(math.radians(a2+8)))
    c1 = (p1[0]+d1[0]*size, p1[1]+d1[1]*size); c2 = (p2[0]+d2[0]*size, p2[1]+d2[1]*size)
    svg.add(f'<path d="M{p1[0]:.1f},{p1[1]:.1f} C{c1[0]:.1f},{c1[1]:.1f} {c2[0]:.1f},{c2[1]:.1f} {p2[0]:.1f},{p2[1]:.1f}" '
            f'fill="none" stroke="{color}" stroke-width="2.6"/>')
    head(svg, p2, c2, color)
    # Badge sits at the loop's outermost point; the label TEXT must extend
    # further outward (away from the node), never back across it -- so the
    # anchor and text-x flip depending on which side the loop points to.
    dirx = math.cos(math.radians(angle))
    lp = (c[0]+dirx*r*dist, c[1]-math.sin(math.radians(angle))*r*dist+ly_off)
    svg.add(f'<circle cx="{lp[0]:.1f}" cy="{lp[1]:.1f}" r="12" fill="{color}"/>')
    svg.text(lp[0], lp[1]+4, str(num), size=12, anchor="middle", weight="bold", fill="white")
    if dirx > 0.3:
        svg.text(lp[0]+18, lp[1]+5, label, size=14, fill=color, anchor="start")
    elif dirx < -0.3:
        svg.text(lp[0]-18, lp[1]+5, label, size=14, fill=color, anchor="end")
    else:
        svg.text(lp[0], lp[1]+30, label, size=14, fill=color, anchor="middle")


def table(svg, x, y, cols, header, rows, colors, row_h=42, size=15.5):
    total = sum(cols)
    svg.add(f'<rect x="{x}" y="{y}" width="{total}" height="{row_h}" fill="#f1f3f4"/>')
    cx = x
    for wcol, h in zip(cols, header):
        svg.text(cx+10, y+row_h/2+6, h, size=size+1, weight="bold")
        cx += wcol
    for i, row in enumerate(rows):
        ry = y+row_h*(i+1)
        if i % 2 == 1:
            svg.add(f'<rect x="{x}" y="{ry}" width="{total}" height="{row_h}" fill="#fafafa"/>')
        cx = x
        for j, (wcol, cell) in enumerate(zip(cols, row)):
            if j == 0:
                svg.add(f'<circle cx="{cx+22}" cy="{ry+row_h/2}" r="12" fill="{colors[i]}"/>')
                svg.text(cx+22, ry+row_h/2+5, cell, size=13, anchor="middle", weight="bold", fill="white")
            else:
                assert text_w(cell, size) < wcol-12, f"too wide: {cell!r} ({text_w(cell,size):.0f}>{wcol-12})"
                svg.text(cx+10, ry+row_h/2+6, cell, size=size)
            cx += wcol
    bottom = y+row_h*(len(rows)+1)
    svg.add(f'<rect x="{x}" y="{y}" width="{total}" height="{bottom-y}" fill="none" stroke="#dadce0" stroke-width="1.5"/>')
    cx = x
    for wcol in cols[:-1]:
        cx += wcol
        svg.add(f'<line x1="{cx}" y1="{y}" x2="{cx}" y2="{bottom}" stroke="#dadce0" stroke-width="1"/>')
    return bottom


def build(path):
    H = 1500
    s = Svg(H)
    s.text(40, 50, "MP-SAE (Protocol B) — combined state diagram, A and B", size=30, weight="bold")
    s.text(40, 82, "Despite the name “Simultaneous”, Figure 5's wire flow is a strict lockstep: A sends c1, B replies c2, A replies c3, B replies c4.",
           size=17, fill="#5f6368")

    XA, XB = 420, 1480
    s.add(f'<rect x="{XA-190}" y="110" width="380" height="60" rx="10" fill="{A_COL}" fill-opacity="0.1" stroke="{A_COL}" stroke-width="2.4"/>')
    s.text(XA, 148, "A (initiator): sends c1, c3", size=21, anchor="middle", weight="bold", fill=A_COL)
    s.add(f'<rect x="{XB-190}" y="110" width="380" height="60" rx="10" fill="{B_COL}" fill-opacity="0.1" stroke="{B_COL}" stroke-width="2.4"/>')
    s.text(XB, 148, "B (responder): sends c2, c4", size=21, anchor="middle", weight="bold", fill=B_COL)

    yA_idle, yA_c2, yA_c4, yA_done, yA_err = 260, 460, 700, 920, 1080
    yB_idle, yB_c3, yB_done, yB_err = 300, 560, 820, 1080
    rA, rB = 62, 62

    cA_idle=(XA,yA_idle); cA_c2=(XA,yA_c2); cA_c4=(XA,yA_c4); cA_done=(XA-90,yA_done); cA_err=(XA+90,yA_err)
    cB_idle=(XB,yB_idle); cB_c3=(XB,yB_c3); cB_done=(XB-90,yB_done); cB_err=(XB+90,yB_err)

    # vertical spine, A
    vline(s, cA_idle, rA, cA_c2, rA, COL["normal"], "1  start: make commit, send C1", num=1, dx=22)
    vline(s, cA_c2, rA, cA_c4, rA, COL["accept"], "3  recv C2: compute K_a,v_a, send C3", num=3, dx=22)
    vline(s, cA_c4, rA, cA_done, rA, COL["accept"], None)
    s.text(XA+20, (yA_c4+yA_done)/2+5, "6  recv C4, v matches", size=15, fill=COL["accept"], weight="bold")
    s.add(f'<circle cx="{XA+20}" cy="{(yA_c4+yA_done)/2-10}" r="12" fill="{COL["accept"]}"/>')
    s.text(XA+20, (yA_c4+yA_done)/2-6, "6", size=13, anchor="middle", weight="bold", fill="white")
    vline(s, cA_c4, rA, cA_err, rA, COL["error"], None)
    s.text(XA+140, (yA_c4+yA_err)/2+5, "7  v mismatch,", size=14, fill=COL["error"], weight="bold")
    s.text(XA+140, (yA_c4+yA_err)/2+23, "or timeout exhausted", size=14, fill=COL["error"])
    s.add(f'<circle cx="{XA+118}" cy="{(yA_c4+yA_err)/2-6}" r="12" fill="{COL["error"]}"/>')
    s.text(XA+118, (yA_c4+yA_err)/2-2, "7", size=13, anchor="middle", weight="bold", fill="white")

    loop(s, cA_c2, rA, 180, COL["timeout"], 2, "timeout: resend C1 (<=8)", size=110, dist=2.1)
    loop(s, cA_c4, rA, 205, COL["timeout"], 4, "timeout: resend C3 (<=8)", size=90, ly_off=-16, dist=1.9)
    loop(s, cA_c4, rA, 155, COL["drop"], 5, "dup C2: resend C3", size=140, ly_off=16, dist=2.3)

    # vertical spine, B
    vline(s, cB_idle, rB, cB_c3, rB, COL["accept"], "1  recv C1: make commit, send C2", num=1, dx=22)
    vline(s, cB_c3, rB, cB_done, rB, COL["accept"], None)
    s.text(XB+20, (yB_c3+yB_done)/2+5, "4  recv C3, v matches: send C4", size=15, fill=COL["accept"], weight="bold")
    s.add(f'<circle cx="{XB+20}" cy="{(yB_c3+yB_done)/2-10}" r="12" fill="{COL["accept"]}"/>')
    s.text(XB+20, (yB_c3+yB_done)/2-6, "4", size=13, anchor="middle", weight="bold", fill="white")
    vline(s, cB_c3, rB, cB_err, rB, COL["error"], None)
    s.text(XB+140, (yB_c3+yB_err)/2+5, "5  v mismatch,", size=14, fill=COL["error"], weight="bold")
    s.text(XB+140, (yB_c3+yB_err)/2+23, "or timeout exhausted", size=14, fill=COL["error"])
    s.add(f'<circle cx="{XB+118}" cy="{(yB_c3+yB_err)/2-6}" r="12" fill="{COL["error"]}"/>')
    s.text(XB+118, (yB_c3+yB_err)/2-2, "5", size=13, anchor="middle", weight="bold", fill="white")

    loop(s, cB_c3, rB, 0, COL["timeout"], 2, "timeout: resend C2 (<=8)", size=110, dist=2.1)
    loop(s, cB_done, 58, 270, COL["drop"], 3, "dup C3: resend C4", size=90, dist=2.2)
    loop(s, cB_idle, rB, 0, COL["drop"], 0, "non-C1 frame: ignore", size=100, dist=2.1)

    # cross arrows (the actual wire messages)
    cross(s, (XA+rA, yA_idle+4), (XB-rB, yB_idle+4), COL["normal"], "C1 = (s_a, t_a)", "c1", (XA+XB)/2, (yA_idle+yB_idle)/2-8)
    cross(s, (XB-rB, yB_c3-4), (XA+rA, yA_c2-4), COL["accept"], "C2 = (s_b, t_b)", "c2", (XA+XB)/2, (yA_c2+yB_c3)/2+6)
    cross(s, (XA+rA, yA_c4-4), (XB-rB, yB_c3+rB+30), COL["accept"], "C3 = v_a", "c3", (XA+XB)/2, yA_c4-40)
    cross(s, (XB-rB, yB_done+4), (XA+rA, yA_done+4), COL["accept"], "C4 = v_b", "c4", (XA+XB)/2, (yA_done+yB_done)/2+6)

    circle(s, cA_idle, rA, "A_IDLE", "#f1f3f4", "#5f6368", sub="not started")
    circle(s, cA_c2, rA, "A_WAIT_C2", "#e8f0fe", A_COL, sub="sent c1")
    circle(s, cA_c4, rA, "A_WAIT_C4", "#e8f0fe", A_COL, sub="sent c3")
    circle(s, cA_done, 58, "A_DONE", "#e6f4ea", COL["accept"], final=True)
    circle(s, cA_err, 58, "A_ERROR", "#fce8e6", COL["error"], final=True)

    circle(s, cB_idle, rB, "B_IDLE", "#f1f3f4", "#5f6368", sub="no session")
    circle(s, cB_c3, rB, "B_WAIT_C3", "#e6f4ea", B_COL, sub="sent c2")
    circle(s, cB_done, 58, "B_DONE", "#e6f4ea", COL["accept"], final=True)
    circle(s, cB_err, 58, "B_ERROR", "#fce8e6", COL["error"], final=True)

    y = 1180
    cols = [50, 360, 1430]
    rows = [
        ("1", "both: start/recv C1", "make_commit(): r,m random mod n=p-1; s=(r+m) mod n; t=g^-m mod p; send; start timer W=2s"),
        ("2", "both: timeout", "resend the identical last frame; after 8 resends -> ERROR (bounded retries, no deadlock)"),
        ("3", "A: recv C2", "store (s_b,t_b); K_a=(g^s_b * t_b)^r_a mod p; v_a=prf(KDF(K_a), s_a,t_a,s_b,t_b); send C3"),
        ("4", "B: recv C3", "K_b=(g^s_a * t_a)^r_b mod p; v_b=prf(KDF(K_b), s_a,t_a,s_b,t_b); if v_b==v_a(received): send C4, DONE"),
        ("5", "dup / invalid", "a repeated c1/c2/c3 (peer's reply was lost) just resends our own last reply; malformed s,t or wrong sender: dropped, never aborts"),
        ("6", "6 / 7", "A checks v_b(received) == its own v_a; match -> A_DONE, mismatch -> A_ERROR (wrong password or forged C4)"),
    ]
    colors = [COL["normal"], COL["timeout"], COL["accept"], COL["accept"], COL["drop"], COL["error"]]
    bottom = table(s, 40, y, cols, ["#", "Event", "Action"], rows, colors)

    s.text(40, bottom+36, "K_a == K_b always (not just when r_a+m_a < p): exponents are combined mod the group ORDER n=p-1, not mod the field size p — see sae_mp.c / README.",
           size=15.5)
    s.text(40, bottom+60, "KDF(K) = Ascon-hash(K)[:16], used as the 16-byte prf() key. v is 16 bytes (the prf() output), reused as both “what I send” and “what I expect from the peer”.",
           size=15.5)

    s.h = bottom + 90
    s.save(path)
    return s.h


if __name__ == "__main__":
    out = sys.argv[1]
    print("height", build(f"{out}/fsm_sae.svg"))
