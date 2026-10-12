"""Generate the Session 6 initiator/responder FSM diagrams as SVG (rendered to PNG separately)."""
import math
import sys
from html import escape

FONT = "Helvetica Neue, Helvetica, Arial, sans-serif"
INK = "#202124"
COL = {"normal": "#3c4043", "accept": "#188038", "drop": "#e37400", "timeout": "#7b1fa2", "error": "#c5221f"}


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.parts = w, h, []

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=18, anchor="start", weight="normal", fill=INK, italic=False):
        style = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{size}" '
                 f'font-weight="{weight}" text-anchor="{anchor}" fill="{fill}"{style}>{escape(s)}</text>')

    def save(self, path):
        with open(path, "w") as f:
            f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
                    f'viewBox="0 0 {self.w} {self.h}">\n<rect width="{self.w}" height="{self.h}" fill="white"/>\n')
            f.write("\n".join(self.parts))
            f.write("\n</svg>\n")


def text_w(s, size):
    return len(s) * size * 0.56


def unit(dx, dy):
    d = math.hypot(dx, dy)
    return dx / d, dy / d


def dirv(angle_deg):  # math angle, y axis up -> svg coordinates
    a = math.radians(angle_deg)
    return math.cos(a), -math.sin(a)


def head(svg, tip, frm, color, size=16):
    ux, uy = unit(tip[0] - frm[0], tip[1] - frm[1])
    bx, by = tip[0] - ux * size, tip[1] - uy * size
    px, py = -uy * size * 0.45, ux * size * 0.45
    svg.add(f'<polygon points="{tip[0]:.1f},{tip[1]:.1f} {bx + px:.1f},{by + py:.1f} {bx - px:.1f},{by - py:.1f}" fill="{color}"/>')


def state(svg, c, r, name, fill, stroke, final=False, sub=None):
    svg.add(f'<circle cx="{c[0]}" cy="{c[1]}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="3.5"/>')
    if final:
        svg.add(f'<circle cx="{c[0]}" cy="{c[1]}" r="{r - 9}" fill="none" stroke="{stroke}" stroke-width="2"/>')
    svg.text(c[0], c[1] + (0 if sub else 8), name, size=23, anchor="middle", weight="bold")
    if sub:
        svg.text(c[0], c[1] + 28, sub, size=16, anchor="middle", fill="#5f6368", italic=True)


def on_circle(c, r, angle):
    dx, dy = dirv(angle)
    return c[0] + r * dx, c[1] + r * dy


def line(svg, c1, r1, c2, r2, color):
    ux, uy = unit(c2[0] - c1[0], c2[1] - c1[1])
    p1 = (c1[0] + ux * r1, c1[1] + uy * r1)
    p2 = (c2[0] - ux * r2, c2[1] - uy * r2)
    svg.add(f'<line x1="{p1[0]:.1f}" y1="{p1[1]:.1f}" x2="{p2[0]:.1f}" y2="{p2[1]:.1f}" stroke="{color}" stroke-width="3"/>')
    head(svg, p2, p1, color)
    return ((p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2)


def curve(svg, c1, r1, c2, r2, bulge, color):
    mx, my = (c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2
    ux, uy = unit(c2[0] - c1[0], c2[1] - c1[1])
    nx, ny = -uy, ux
    ctrl = (mx + nx * bulge, my + ny * bulge)
    a = unit(ctrl[0] - c1[0], ctrl[1] - c1[1])
    b = unit(ctrl[0] - c2[0], ctrl[1] - c2[1])
    p1 = (c1[0] + a[0] * r1, c1[1] + a[1] * r1)
    p2 = (c2[0] + b[0] * r2, c2[1] + b[1] * r2)
    svg.add(f'<path d="M{p1[0]:.1f},{p1[1]:.1f} Q{ctrl[0]:.1f},{ctrl[1]:.1f} {p2[0]:.1f},{p2[1]:.1f}" '
            f'fill="none" stroke="{color}" stroke-width="3"/>')
    head(svg, p2, ctrl, color)
    return (0.25 * p1[0] + 0.5 * ctrl[0] + 0.25 * p2[0], 0.25 * p1[1] + 0.5 * ctrl[1] + 0.25 * p2[1])


def loop(svg, c, r, angle, color, size=120, spread=24):
    p1 = on_circle(c, r, angle - spread)
    p2 = on_circle(c, r, angle + spread)
    d1, d2 = dirv(angle - spread - 8), dirv(angle + spread + 8)
    c1 = (p1[0] + d1[0] * size, p1[1] + d1[1] * size)
    c2 = (p2[0] + d2[0] * size, p2[1] + d2[1] * size)
    svg.add(f'<path d="M{p1[0]:.1f},{p1[1]:.1f} C{c1[0]:.1f},{c1[1]:.1f} {c2[0]:.1f},{c2[1]:.1f} {p2[0]:.1f},{p2[1]:.1f}" '
            f'fill="none" stroke="{color}" stroke-width="3"/>')
    head(svg, p2, c2, color)
    return on_circle(c, r + size * 0.78, angle)


def label(svg, pos, num, text, color, anchor="middle", dx=0, dy=0, size=19):
    """Number badge + text on a white box. anchor: where the label sits relative to pos."""
    badge = 15
    tw = text_w(text, size)
    total = 2 * badge + 8 + tw
    x, y = pos[0] + dx, pos[1] + dy
    if anchor == "middle":
        x0 = x - total / 2
    elif anchor == "end":
        x0 = x - total
    else:
        x0 = x
    svg.add(f'<rect x="{x0 - 6:.1f}" y="{y - 20:.1f}" width="{total + 12:.1f}" height="36" rx="7" '
            f'fill="white" fill-opacity="0.93" stroke="{color}" stroke-width="1.2"/>')
    svg.add(f'<circle cx="{x0 + badge:.1f}" cy="{y - 2:.1f}" r="{badge}" fill="{color}"/>')
    svg.text(x0 + badge, y + 4, str(num), size=15, anchor="middle", weight="bold", fill="white")
    svg.text(x0 + 2 * badge + 8, y + 4, text, size=size)


def table(svg, x, y, cols, header, rows, colors, row_h=44, size=17):
    total = sum(cols)
    svg.add(f'<rect x="{x}" y="{y}" width="{total}" height="{row_h}" fill="#f1f3f4"/>')
    cx = x
    for wcol, h in zip(cols, header):
        svg.text(cx + 12, y + row_h / 2 + 6, h, size=size, weight="bold")
        cx += wcol
    for i, row in enumerate(rows):
        ry = y + row_h * (i + 1)
        if i % 2 == 1:
            svg.add(f'<rect x="{x}" y="{ry}" width="{total}" height="{row_h}" fill="#fafafa"/>')
        cx = x
        for j, (wcol, cell) in enumerate(zip(cols, row)):
            if j == 0:
                svg.add(f'<circle cx="{cx + 26}" cy="{ry + row_h / 2}" r="14" fill="{colors[i]}"/>')
                svg.text(cx + 26, ry + row_h / 2 + 5, cell, size=15, anchor="middle", weight="bold", fill="white")
            else:
                assert text_w(cell, size) < wcol - 14, f"cell too wide: {cell!r} ({text_w(cell, size):.0f} > {wcol - 14})"
                svg.text(cx + 12, ry + row_h / 2 + 6, cell, size=size)
            cx += wcol
    bottom = y + row_h * (len(rows) + 1)
    svg.add(f'<rect x="{x}" y="{y}" width="{total}" height="{bottom - y}" fill="none" stroke="#dadce0" stroke-width="1.5"/>')
    cx = x
    for wcol in cols[:-1]:
        cx += wcol
        svg.add(f'<line x1="{cx}" y1="{y}" x2="{cx}" y2="{bottom}" stroke="#dadce0" stroke-width="1"/>')
    return bottom


def legend(svg, x, y):
    items = [("normal", "start / normal step"), ("accept", "frame accepted"), ("drop", "frame dropped"),
             ("timeout", "timeout"), ("error", "goes to error")]
    for col, txt in items:
        svg.add(f'<line x1="{x}" y1="{y}" x2="{x + 40}" y2="{y}" stroke="{COL[col]}" stroke-width="4"/>')
        svg.text(x + 50, y + 6, txt, size=17)
        x += 60 + text_w(txt, 17) + 30


# --------------------------------------------------------------------------------------
def initiator(path):
    W, H = 1800, 1330
    s = Svg(W, H)
    s.text(40, 52, "P2P protocol — Initiator I (finite state machine)", size=32, weight="bold")
    s.text(40, 86, "Mealy machine: each arrow is  event [guard] / action.  The number on an arrow refers to the table below.",
           size=19, fill="#5f6368")
    legend(s, 40, 122)

    idle, wait, done, err = (230, 440), (820, 440), (1460, 250), (1460, 640)
    # start marker
    s.add(f'<circle cx="62" cy="{idle[1]}" r="12" fill="{INK}"/>')
    s.add(f'<line x1="74" y1="{idle[1]}" x2="134" y2="{idle[1]}" stroke="{INK}" stroke-width="3"/>')
    head(s, (148, idle[1]), (74, idle[1]), INK)
    s.text(62, idle[1] + 40, "power on", size=16, anchor="middle", fill="#5f6368")

    m = line(s, idle, 82, wait, 108, COL["normal"])
    label(s, m, 1, "start", COL["normal"], dy=-24)

    a = loop(s, wait, 108, 90, COL["accept"], size=135)
    label(s, a, 2, "PONG x = n+1, k+1 < t", COL["accept"], dy=-6)
    a = loop(s, wait, 108, 270, COL["timeout"], size=135)
    label(s, a, 3, "timeout, retries < MAX", COL["timeout"], dy=24)
    a = loop(s, wait, 108, 145, COL["drop"], size=130)
    label(s, a, 4, "drops: duplicate, replay, invalid (4–6)", COL["drop"], anchor="end", dx=-6, dy=-4)

    m = line(s, wait, 108, done, 88, COL["accept"])
    label(s, m, 7, "PONG x = n+1, k+1 = t", COL["accept"], dx=-20, dy=-72)
    m = line(s, wait, 108, err, 88, COL["error"])
    label(s, m, 8, "timeout, retries = MAX", COL["error"], dx=90, dy=-52)
    m = curve(s, wait, 108, err, 88, 150, COL["error"])
    label(s, m, 9, "ERROR(x = n)", COL["error"], dx=0, dy=30)

    state(s, idle, 82, "I_IDLE", "#f1f3f4", "#5f6368", sub="not started")
    state(s, wait, 108, "I_WAIT_PONG", "#e8f0fe", "#1a73e8", sub="waiting for R")
    state(s, done, 88, "I_DONE", "#e6f4ea", COL["accept"], final=True, sub="success")
    state(s, err, 88, "I_ERROR", "#fce8e6", COL["error"], final=True, sub="gave up")

    cols = [56, 290, 560, 820]
    rows = [
        ("1", "I_IDLE → I_WAIT_PONG", "start", "n ← n0, k ← 0, send PING(n, a_I), start_timer(W)"),
        ("2", "I_WAIT_PONG → I_WAIT_PONG", "recv PONG(x, a_R) [x = n+1 ∧ k+1 < t]", "k++, n ← x+1, send PING(n, a_I), restart_timer(W)      (accept_continue)"),
        ("3", "I_WAIT_PONG → I_WAIT_PONG", "timeout [retries < MAX]", "retries++, resend(last_msg), restart_timer(W)"),
        ("4", "I_WAIT_PONG → I_WAIT_PONG", "recv PONG(x, a_R) [x = n−1]", "drop: reply to a PING we resent      (duplicate_drop)"),
        ("5", "I_WAIT_PONG → I_WAIT_PONG", "recv PONG [other x]  or  ERROR [x ≠ n]", "drop: stale or replayed frame      (replay_drop)"),
        ("6", "I_WAIT_PONG → I_WAIT_PONG", "recv [sender ≠ a_R ∨ tag fails ∨ bad type]", "drop: forged, tampered or unexpected      (invalid_drop)"),
        ("7", "I_WAIT_PONG → I_DONE", "recv PONG(x, a_R) [x = n+1 ∧ k+1 = t]", "k++, stop_timer, send END(x+1, a_I)      (accept_end)"),
        ("8", "I_WAIT_PONG → I_ERROR", "timeout [retries = MAX]", "log \"responder unreachable\""),
        ("9", "I_WAIT_PONG → I_ERROR", "recv ERROR(x, a_R) [x = n]", "stop_timer, log \"responder out of sync (rebooted?)\""),
    ]
    colors = [COL["normal"], COL["accept"], COL["timeout"], COL["drop"], COL["drop"], COL["drop"],
              COL["accept"], COL["error"], COL["error"]]
    bottom = table(s, 40, 780, cols, ["#", "From → To", "Event [guard]", "Action"], rows, colors)

    s.text(40, bottom + 40, "Variables (EFSM):  n = counter in the last PING sent · k = completed rounds · t = T rounds · "
           "W = 2000 ms · MAX = 8 resends · last_msg = last frame sent", size=17)
    s.text(40, bottom + 70, "Every other event in a state is listed in the code and only logged (Appendix C.2).  "
           "I_DONE and I_ERROR are final: they ignore all events.", size=17)
    s.save(path)
    return W, H


def responder(path):
    W, H = 1900, 1440
    s = Svg(W, H)
    s.text(40, 52, "P2P protocol — Responder R (finite state machine)", size=32, weight="bold")
    s.text(40, 86, "Mealy machine: each arrow is  event [guard] / action.  The number on an arrow refers to the table below.",
           size=19, fill="#5f6368")
    legend(s, 40, 122)

    idle, sess = (430, 500), (1200, 500)
    s.add(f'<circle cx="120" cy="{idle[1]}" r="12" fill="{INK}"/>')
    s.add(f'<line x1="132" y1="{idle[1]}" x2="{idle[0] - 110}" y2="{idle[1]}" stroke="{INK}" stroke-width="3"/>')
    head(s, (idle[0] - 96, idle[1]), (132, idle[1]), INK)
    s.text(120, idle[1] + 40, "power on", size=16, anchor="middle", fill="#5f6368")

    m = curve(s, idle, 95, sess, 115, -170, COL["accept"])
    label(s, m, 1, "PING x = n0 (new session)", COL["accept"], dy=-4)

    a = loop(s, idle, 95, 90, COL["error"], size=120)
    label(s, a, 2, "PING x ≠ n0 → send ERROR", COL["error"], dy=-8)
    a = loop(s, idle, 95, 270, COL["drop"], size=120)
    label(s, a, 3, "other frame / stray timeout", COL["drop"], dy=26)

    a = loop(s, sess, 115, 95, COL["accept"], size=125, spread=18)
    label(s, a, 4, "PING x = e", COL["accept"], dy=-8)
    a = loop(s, sess, 115, 50, COL["drop"], size=120, spread=18)
    label(s, a, 8, "old / ahead PING, invalid → drop", COL["drop"], anchor="start", dx=12, dy=-8)
    a = loop(s, sess, 115, 5, COL["accept"], size=120, spread=18)
    label(s, a, 5, "PING x = e−2 → resend PONG", COL["accept"], anchor="start", dx=12, dy=6)
    a = loop(s, sess, 115, -40, COL["accept"], size=120, spread=18)
    label(s, a, 6, "PING x = n0 → restart", COL["accept"], anchor="start", dx=12, dy=12)
    a = loop(s, sess, 115, 270, COL["timeout"], size=120, spread=18)
    label(s, a, 7, "timeout, idle+1 < 10", COL["timeout"], dy=30)

    m = curve(s, sess, 115, idle, 95, -150, COL["accept"])
    label(s, m, 9, "END x = e", COL["accept"], dy=4)
    m = curve(s, sess, 115, idle, 95, -330, COL["timeout"])
    label(s, m, 10, "timeout, idle+1 = 10 (abandon)", COL["timeout"], dy=4)

    state(s, idle, 95, "R_IDLE", "#f1f3f4", "#5f6368", sub="no session")
    state(s, sess, 115, "R_IN_SESSION", "#e8f0fe", "#1a73e8", sub="expecting e")

    cols = [56, 300, 540, 924]
    rows = [
        ("1", "R_IDLE → R_IN_SESSION", "recv PING(x, a_I) [x = n0]", "k ← 1, e ← x+2, idle ← 0, send PONG(x+1, a_R), start_timer(W)"),
        ("2", "R_IDLE → R_IDLE", "recv PING(x, a_I) [x ≠ n0]", "send ERROR(x, out of sync): R lost its state (reboot) or session ended"),
        ("3", "R_IDLE → R_IDLE", "recv other frame  or  stray timeout", "drop / ignore      (invalid_drop)"),
        ("4", "R_IN_SESSION → R_IN_SESSION", "recv PING(x, a_I) [x = e]", "k++, e ← x+2, idle ← 0, send PONG(x+1, a_R), restart_timer(W)"),
        ("5", "R_IN_SESSION → R_IN_SESSION", "recv PING(x, a_I) [x = e−2]", "idle ← 0, resend(last_msg), restart_timer(W): our PONG was lost"),
        ("6", "R_IN_SESSION → R_IN_SESSION", "recv PING(x, a_I) [x = n0]", "restart session as in 1: I rebooted and began again"),
        ("7", "R_IN_SESSION → R_IN_SESSION", "timeout [idle+1 < 10]", "idle++, restart_timer(W)"),
        ("8", "R_IN_SESSION → R_IN_SESSION", "recv PING [x older or ahead]  or  invalid", "drop, never abort      (replay_drop / invalid_drop)"),
        ("9", "R_IN_SESSION → R_IDLE", "recv END(x, a_I) [x = e]", "stop_timer, log SUCCESS"),
        ("10", "R_IN_SESSION → R_IDLE", "timeout [idle+1 = 10]", "log \"initiator silent\": ABANDONED (I gave up, or END was lost)"),
    ]
    colors = [COL["accept"], COL["error"], COL["drop"], COL["accept"], COL["accept"], COL["accept"],
              COL["timeout"], COL["drop"], COL["accept"], COL["timeout"]]
    bottom = table(s, 40, 850, cols, ["#", "From → To", "Event [guard]", "Action"], rows, colors)

    s.text(40, bottom + 40, "Variables (EFSM):  e = counter expected in the next PING · k = completed rounds · "
           "idle = silent timeouts in a row · W = 2000 ms · 10 = MAX + 2 (outlasts I's resends)", size=17)
    s.text(40, bottom + 70, "Every other event in a state is listed in the code and only logged (Appendix C.2).  "
           "R has no final state: after a session it waits for the next one.", size=17)
    s.save(path)
    return W, H


if __name__ == "__main__":
    out = sys.argv[1]
    print("initiator", *initiator(f"{out}/fsm_initiator.svg"))
    print("responder", *responder(f"{out}/fsm_responder.svg"))
