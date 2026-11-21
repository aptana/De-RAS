#!/usr/bin/env python3
"""Build the De-RAS manuals (.docx) from manual.id.md / manual.en.md (each one that exists).
Pass 1 builds the document, LibreOffice renders it to PDF, pdftotext finds the page of every
chapter and section, and pass 2 rebuilds the document with those page numbers in the contents list.
Needs: node with docx@9 and marked@12 on NODE_PATH, LibreOffice (soffice), poppler-utils (pdftotext)."""
import json, os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
JOBS = [("manual.id.md", "../De-RAS_Panduan_Pengguna_ID.docx"), ("manual.en.md", "../De-RAS_User_Manual_EN.docx")]

def norm(s): return " ".join(s.replace(" ", " ").split())

def pages_for(docx, headings):
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(["soffice", "--headless", "--convert-to", "pdf", "--outdir", tmp, docx], check=True, capture_output=True)
        pdf = os.path.join(tmp, os.path.splitext(os.path.basename(docx))[0] + ".pdf")
        n = int([l for l in subprocess.run(["pdfinfo", pdf], capture_output=True, text=True).stdout.splitlines() if l.startswith("Pages:")][0].split()[1])
        texts = [subprocess.run(["pdftotext", "-layout", "-f", str(p), "-l", str(p), pdf, "-"], capture_output=True, text=True).stdout for p in range(1, n + 1)]
    lines = [[norm(l) for l in t.splitlines()] for t in texts]
    start = next(i for i, t in enumerate(lines) if any(norm(headings[0]["text"]) == l for l in t) and i > 2)
    out, p = {}, start
    for h in headings:
        key = norm(h["text"])
        q = p
        while q < n and key not in lines[q]: q += 1
        if q == n: raise SystemExit(f"heading not found in PDF: {h['text']}")
        out[h["text"]] = str(q + 1); p = q
    return out

# The published repository carries the English manual only; a missing source
# is skipped with a warning instead of stopping the build.
built = 0
for src, dst in JOBS:
    src, dst = os.path.join(HERE, src), os.path.normpath(os.path.join(HERE, dst))
    if not os.path.exists(src):
        print(f"warning: {os.path.basename(src)} not found; {os.path.basename(dst)} not built", file=sys.stderr)
        continue
    subprocess.run(["node", os.path.join(HERE, "build_manual.js"), src, dst], check=True)
    heads = json.load(open(dst + ".headings.json"))
    pages = pages_for(dst, heads)
    pj = dst + ".pages.json"; json.dump(pages, open(pj, "w"))
    subprocess.run(["node", os.path.join(HERE, "build_manual.js"), src, dst, pj], check=True)
    if pages_for(dst, heads) != pages: print("warning: page numbers shifted between passes", file=sys.stderr)
    os.remove(pj); os.remove(dst + ".headings.json")
    print("ok", dst)
    built += 1
if built == 0: raise SystemExit("no manual source found (manual.id.md / manual.en.md)")
