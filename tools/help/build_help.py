#!/usr/bin/env python3
"""Builds resources/help/help.json and docs/manual/help-topics.{id,en}.md from help_source.py."""
import html, json, os, sys
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", ".."))
sys.path.insert(0, here)
from help_source import T  # noqa: E402

LBL = {"id": ("Fungsi", "Langkah", "Data yang dibutuhkan", "Hasil", "Status"), "en": ("Purpose", "Steps", "Data needed", "Result", "Status")}
STATUS = {"available": ("Tersedia", "Available"), "preview": ("Pratinjau", "Preview"), "partial": ("Parsial", "Partial"), "unavailable": ("Belum tersedia", "Not available yet")}

def body_html(t, li):
    i = 0 if li == "id" else 1
    L = LBL[li]
    out = []
    if t["p"][i]: out.append(f"<h3>{L[0]}</h3><p>{html.escape(t['p'][i])}</p>")
    if t["steps"][i]: out.append(f"<h3>{L[1]}</h3><ol>" + "".join(f"<li>{html.escape(s)}</li>" for s in t["steps"][i]) + "</ol>")
    if t["d"][i]: out.append(f"<h3>{L[2]}</h3><p>{html.escape(t['d'][i])}</p>")
    if t["r"][i]: out.append(f"<h3>{L[3]}</h3><p>{html.escape(t['r'][i])}</p>")
    out.append(f"<p><b>{L[4]}:</b> {STATUS[t['st']][i]}</p>")
    return "".join(out)

def body_md(t, li):
    i = 0 if li == "id" else 1
    L = LBL[li]
    out = [f"_{t['s'][i]}_", ""]
    if t["p"][i]: out += [f"**{L[0]}.** {t['p'][i]}", ""]
    if t["steps"][i]: out += [f"**{L[1]}:**", ""] + [f"{n}. {s}" for n, s in enumerate(t["steps"][i], 1)] + [""]
    if t["d"][i]: out += [f"**{L[2]}.** {t['d'][i]}", ""]
    if t["r"][i]: out += [f"**{L[3]}.** {t['r'][i]}", ""]
    out += [f"**{L[4]}:** {STATUS[t['st']][i]}", ""]
    return out

topics = []
for k, t in T.items():
    for r in t["rel"]:
        assert r in T, f"{k}: unknown related topic {r}"
    topics.append({"id": k, "title": {"id": t["t"][0], "en": t["t"][1]}, "short": {"id": t["s"][0], "en": t["s"][1]},
                   "body": {"id": body_html(t, "id"), "en": body_html(t, "en")}, "status": t["st"], "related": t["rel"]})
os.makedirs(os.path.join(root, "resources", "help"), exist_ok=True)
with open(os.path.join(root, "resources", "help", "help.json"), "w", encoding="utf-8") as f:
    json.dump({"generator": "tools/help/build_help.py", "topics": topics}, f, ensure_ascii=False, indent=1)
os.makedirs(os.path.join(root, "docs", "manual"), exist_ok=True)
for li, title in (("id", "Bantuan per fitur (topik ⓘ)"), ("en", "Help per feature (ⓘ topics)")):
    lines = [f"# {title}", "", "<!-- Dibuat otomatis dari tools/help/help_source.py; jangan diedit manual. / Generated from tools/help/help_source.py; do not edit by hand. -->", ""]
    for k, t in T.items():
        lines += [f"## {t['t'][0 if li == 'id' else 1]}", "", f"`{k}`", ""] + body_md(t, li)
    with open(os.path.join(root, "docs", "manual", f"help-topics.{li}.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
print(f"{len(topics)} topics written")
