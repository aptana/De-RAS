// Builds the De-RAS user manuals (.docx) from the Markdown sources in this folder.
// Usage:  python3 build_manuals.py   (builds both manuals in two passes so the table of contents has page numbers)
// Single pass: node build_manual.js <source.md> <out.docx> [pages.json]
// Needs: npm install docx@9 marked@12 (on NODE_PATH); LibreOffice and pdftotext for the page-number pass.
// Markdown conventions: front matter (title, subtitle, tagline, edition, date, note, header, toc, page, figure),
// "# " chapter (new page), "## " section, "### " subsection, pipe tables, lists, images,
// and callouts written as "> CATATAN: …", "> TIPS: …", "> PERHATIAN: …", "> STATUS: …" (or NOTE/TIP/CAUTION).
const fs = require('fs');
const path = require('path');
const { marked } = require('marked');
const D = require('docx');
const { Document, Packer, Paragraph, TextRun, ImageRun, Table, TableRow, TableCell, WidthType, BorderStyle,
  ShadingType, AlignmentType, HeadingLevel, Header, Footer, PageNumber, TableOfContents, LevelFormat,
  TabStopType, PageBreak, LineRuleType } = D;

const [, , srcArg, outArg, pagesArg] = process.argv;
const PAGES = pagesArg && fs.existsSync(pagesArg) ? JSON.parse(fs.readFileSync(pagesArg, 'utf8')) : {};
const tocEntries = [];
const src = path.resolve(srcArg), out = path.resolve(outArg), base = path.dirname(src);
let text = fs.readFileSync(src, 'utf8');
const fm = {};
text = text.replace(/^---\n([\s\S]*?)\n---\n/, (_, body) => { body.split('\n').forEach(l => { const i = l.indexOf(':'); if (i > 0) fm[l.slice(0, i).trim()] = l.slice(i + 1).trim(); }); return ''; });

const NAVY = '01215F', PRI = '0151B7', LINE = 'B9CBE3', GREY = '5B6B82';
const CONTENT_W = 9026; // A4 with 2.54 cm margins, in DXA
const LOGO = path.resolve(base, '../../../branding/De-RAS-logo-1024.png');
const LOGO_SMALL = path.resolve(base, '../../../branding/icons/De-RAS-64.png');
const CALLOUT = {
  CATATAN: ['0151B7', 'EAF3FD'], NOTE: ['0151B7', 'EAF3FD'],
  TIPS: ['2E9E6B', 'EAF7F1'], TIP: ['2E9E6B', 'EAF7F1'],
  PERHATIAN: ['B87F00', 'FFF6E0'], CAUTION: ['B87F00', 'FFF6E0'],
  STATUS: ['0077E8', 'E8F6FE'],
};

// ---------- inline ----------
function inlineRuns(tokens, style = {}) {
  const runs = [];
  for (const t of tokens || []) {
    switch (t.type) {
      case 'strong': runs.push(...inlineRuns(t.tokens, { ...style, bold: true })); break;
      case 'em': runs.push(...inlineRuns(t.tokens, { ...style, italics: true })); break;
      case 'codespan': runs.push(new TextRun({ text: decode(t.text), font: 'Consolas', size: (style.size || 22) - 2, color: '01378A', bold: style.bold })); break;
      case 'link': runs.push(...inlineRuns(t.tokens, { ...style, color: PRI })); break;
      case 'br': runs.push(new TextRun({ break: 1 })); break;
      case 'escape': case 'text':
        if (t.tokens && t.tokens.length) runs.push(...inlineRuns(t.tokens, style));
        else runs.push(new TextRun({ text: decode(t.text), ...style })); break;
      default: if (t.text) runs.push(new TextRun({ text: decode(t.text), ...style }));
    }
  }
  return runs;
}
function decode(s) { return String(s).replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&#39;/g, "'"); }

// ---------- blocks ----------
const children = [];
let fig = 0;
const border = { style: BorderStyle.SINGLE, size: 4, color: LINE };
const cellBorders = { top: border, bottom: border, left: border, right: border };
const noBorder = { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' };

function para(runs, opts = {}) { return new Paragraph({ children: runs, spacing: { after: 120 }, ...opts }); }

function table(tok) {
  const ncol = tok.header.length;
  const lens = tok.header.map((h, i) => Math.max(plain(h.tokens).length, ...tok.rows.map(r => plain(r[i].tokens).length)));
  const weights = lens.map(l => Math.min(Math.max(l, 6), 60));
  const sum = weights.reduce((a, b) => a + b, 0);
  const widths = weights.map(w => Math.round(CONTENT_W * w / sum));
  widths[ncol - 1] += CONTENT_W - widths.reduce((a, b) => a + b, 0);
  const mkCell = (cell, i, head) => new TableCell({
    width: { size: widths[i], type: WidthType.DXA }, borders: cellBorders,
    shading: head ? { fill: NAVY, type: ShadingType.CLEAR, color: 'auto' } : undefined,
    margins: { top: 70, bottom: 70, left: 110, right: 110 },
    children: [new Paragraph({ spacing: { before: 0, after: 0 }, children: inlineRuns(cell.tokens, head ? { bold: true, color: 'FFFFFF', size: 20 } : { size: 20 }) })],
  });
  return new Table({
    width: { size: CONTENT_W, type: WidthType.DXA }, columnWidths: widths,
    rows: [new TableRow({ tableHeader: true, children: tok.header.map((c, i) => mkCell(c, i, true)) }),
      ...tok.rows.map(r => new TableRow({ cantSplit: true, children: r.map((c, i) => mkCell(c, i, false)) }))],
  });
}
function plain(tokens) { return (tokens || []).map(t => t.tokens ? plain(t.tokens) : (t.text || '')).join(''); }

function callout(tok) {
  const inner = tok.tokens.find(t => t.type === 'paragraph');
  const raw = inner ? inner.tokens : [];
  const first = raw[0] && raw[0].type === 'text' ? raw[0].text : '';
  const m = first.match(/^([A-Z]+):\s*/);
  const key = m && CALLOUT[m[1]] ? m[1] : 'CATATAN';
  const [col, fill] = CALLOUT[key];
  const rest = m ? [{ ...raw[0], text: first.slice(m[0].length), tokens: undefined }, ...raw.slice(1)] : raw;
  return [new Table({
    width: { size: CONTENT_W, type: WidthType.DXA }, columnWidths: [CONTENT_W],
    rows: [new TableRow({ cantSplit: true, children: [new TableCell({
      width: { size: CONTENT_W, type: WidthType.DXA },
      borders: { top: noBorder, bottom: noBorder, right: noBorder, left: { style: BorderStyle.SINGLE, size: 24, color: col } },
      shading: { fill, type: ShadingType.CLEAR, color: 'auto' },
      margins: { top: 100, bottom: 100, left: 180, right: 160 },
      children: [new Paragraph({ spacing: { before: 0, after: 0 }, children: [new TextRun({ text: (m ? m[1] : key) + '  ', bold: true, color: col }), ...inlineRuns(rest)] })],
    })] })],
  }), new Paragraph({ spacing: { after: 60 }, children: [] })];
}

function image(t) {
  const file = path.resolve(base, t.href);
  const data = fs.readFileSync(file);
  // read PNG/JPEG size
  let w = 1600, h = 900;
  if (data[0] === 0x89) { w = data.readUInt32BE(16); h = data.readUInt32BE(20); }
  else { let i = 2; while (i < data.length) { const len = data.readUInt16BE(i + 2); const mk = data[i + 1]; if (mk >= 0xC0 && mk <= 0xC2) { h = data.readUInt16BE(i + 5); w = data.readUInt16BE(i + 7); break; } i += 2 + len; } }
  const W = 600, H = Math.round(W * h / w);
  fig += 1;
  return [
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 120, after: 60, line: 240, lineRule: LineRuleType.AUTO }, keepNext: true,
      children: [new ImageRun({ type: file.endsWith('.png') ? 'png' : 'jpg', data, transformation: { width: W, height: H }, altText: { title: t.text, description: t.text, name: 'fig' + fig } })] }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 200 }, children: [new TextRun({ text: `${fm.figure || 'Figure'} ${fig}. ${t.text}`, italics: true, size: 18, color: GREY })] }),
  ];
}

function list(tok, level = 0) {
  const outp = [];
  tok.items.forEach(item => {
    const runs = [];
    const nested = [];
    for (const c of item.tokens) {
      if (c.type === 'text' || c.type === 'paragraph') runs.push(...inlineRuns(c.tokens || [{ type: 'text', text: c.text }]));
      else if (c.type === 'list') nested.push(c);
    }
    const p = tok.ordered
      ? new Paragraph({ numbering: { reference: 'num-' + numInstance, level }, spacing: { after: 80 }, children: runs })
      : new Paragraph({ numbering: { reference: 'bullets', level }, spacing: { after: 80 }, children: runs });
    outp.push(p);
    nested.forEach(n => outp.push(...list(n, level + 1)));
  });
  return outp;
}

let numInstance = 0;
const numConfigs = [];
function newNumbering() {
  numInstance += 1;
  numConfigs.push({ reference: 'num-' + numInstance, levels: [
    { level: 0, format: LevelFormat.DECIMAL, text: '%1.', alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 500, hanging: 300 } } } },
    { level: 1, format: LevelFormat.LOWER_LETTER, text: '%2.', alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 1000, hanging: 300 } } } }] });
}

const tokens = marked.lexer(text);
let firstChapter = true;
for (const t of tokens) {
  switch (t.type) {
    case 'heading': {
      const lvl = [HeadingLevel.HEADING_1, HeadingLevel.HEADING_2, HeadingLevel.HEADING_3][t.depth - 1];
      children.push(new Paragraph({ heading: lvl, pageBreakBefore: t.depth === 1 && !firstChapter ? true : undefined, keepNext: true, children: inlineRuns(t.tokens) }));
      if (t.depth <= 2) tocEntries.push({ depth: t.depth, text: decode(plain(t.tokens)) });
      if (t.depth === 1) firstChapter = false;
      break;
    }
    case 'paragraph': {
      const img = t.tokens.length === 1 && t.tokens[0].type === 'image' ? t.tokens[0] : null;
      if (img) children.push(...image(img)); else children.push(para(inlineRuns(t.tokens)));
      break;
    }
    case 'table': children.push(table(t), new Paragraph({ spacing: { after: 60 }, children: [] })); break;
    case 'blockquote': children.push(...callout(t)); break;
    case 'list': if (t.ordered) newNumbering(); children.push(...list(t)); break;
    case 'code':
      t.text.split('\n').forEach((l, i, a) => children.push(new Paragraph({ shading: { fill: 'F2F5FA', type: ShadingType.CLEAR, color: 'auto' }, spacing: { before: i ? 0 : 80, after: i === a.length - 1 ? 160 : 0 },
        children: [new TextRun({ text: l || ' ', font: 'Consolas', size: 19 })] })));
      break;
    case 'space': case 'hr': break;
    default: if (t.text) children.push(para([new TextRun(decode(t.text))]));
  }
}

// ---------- cover + toc ----------
const cover = [
  new Paragraph({ spacing: { before: 1800, after: 240, line: 240, lineRule: LineRuleType.AUTO }, alignment: AlignmentType.CENTER, children: [new ImageRun({ type: 'png', data: fs.readFileSync(LOGO), transformation: { width: 210, height: 210 }, altText: { title: 'De-RAS logo', description: 'De-RAS logo', name: 'logo' } })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 80 }, children: [new TextRun({ text: fm.title || 'De-RAS', bold: true, size: 64, color: NAVY })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 120 }, children: [new TextRun({ text: fm.subtitle || '', size: 34, color: PRI })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 600 }, children: [new TextRun({ text: fm.tagline || '', size: 21, color: GREY })] }),
  new Paragraph({ border: { top: { style: BorderStyle.SINGLE, size: 4, color: LINE, space: 12 } }, alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [new TextRun({ text: fm.edition || '', size: 20, color: GREY })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 400 }, children: [new TextRun({ text: fm.date || '', size: 20, color: GREY })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: fm.note || '', italics: true, size: 17, color: GREY })] }),
  new Paragraph({ children: [new PageBreak()] }),
  new Paragraph({ spacing: { after: 240 }, children: [new TextRun({ text: fm.toc || 'Contents', bold: true, size: 32, color: NAVY })] }),
];
function tocParas() {
  return tocEntries.map(e => new Paragraph({
    tabStops: [{ type: TabStopType.RIGHT, position: CONTENT_W, leader: 'dot' }],
    indent: e.depth === 2 ? { left: 400 } : undefined,
    spacing: { before: e.depth === 1 ? 100 : 0, after: e.depth === 1 ? 30 : 20 },
    children: [new TextRun({ text: e.text, bold: e.depth === 1, size: e.depth === 1 ? 20 : 19, color: e.depth === 1 ? NAVY : undefined }),
      new TextRun({ text: '\t' + (PAGES[e.text] || '00'), bold: e.depth === 1, size: e.depth === 1 ? 20 : 19 })],
  }));
}

const header = new Header({ children: [new Paragraph({
  border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: LINE, space: 4 } },
  tabStops: [{ type: TabStopType.RIGHT, position: CONTENT_W }],
  children: [new ImageRun({ type: 'png', data: fs.readFileSync(LOGO_SMALL), transformation: { width: 16, height: 16 }, altText: { title: 'logo', description: 'De-RAS logo', name: 'hlogo' } }),
    new TextRun({ text: '  De-RAS', bold: true, size: 17, color: NAVY }), new TextRun({ text: '\t' + (fm.header || ''), size: 17, color: GREY })],
})] });
const footer = new Footer({ children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [
  new TextRun({ children: [(fm.page || 'Page') + ' ', PageNumber.CURRENT, ' / ', PageNumber.TOTAL_PAGES], size: 18, color: '666666' })] })] });

const doc = new Document({
  creator: 'De-RAS', title: `${fm.title} — ${fm.subtitle}`, description: fm.edition,
  styles: {
    default: { document: { run: { font: 'Calibri', size: 22 }, paragraph: { spacing: { after: 120, line: 276, lineRule: LineRuleType.AUTO } } } },
    paragraphStyles: [
      { id: 'Heading1', name: 'Heading 1', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { size: 32, bold: true, color: NAVY }, paragraph: { spacing: { before: 120, after: 200 }, outlineLevel: 0, border: { bottom: { style: BorderStyle.SINGLE, size: 6, color: NAVY, space: 4 } } } },
      { id: 'Heading2', name: 'Heading 2', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { size: 26, bold: true, color: PRI }, paragraph: { spacing: { before: 280, after: 120 }, outlineLevel: 1 } },
      { id: 'Heading3', name: 'Heading 3', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { size: 23, bold: true, color: NAVY }, paragraph: { spacing: { before: 200, after: 100 }, outlineLevel: 2 } },
    ],
  },
  numbering: { config: [
    { reference: 'bullets', levels: [
      { level: 0, format: LevelFormat.BULLET, text: '•', alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 500, hanging: 280 } } } },
      { level: 1, format: LevelFormat.BULLET, text: '–', alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 1000, hanging: 280 } } } }] },
    ...numConfigs] },
  sections: [
    { properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 } } }, children: cover.slice(0, 7) },
    { properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 } } },
      headers: { default: header }, footers: { default: footer }, children: [cover[8], ...tocParas(), new Paragraph({ children: [new PageBreak()] }), ...children] },
  ],
});
Packer.toBuffer(doc).then(buf => { fs.writeFileSync(out, buf); fs.writeFileSync(out + '.headings.json', JSON.stringify(tocEntries)); console.log('wrote', out, buf.length, 'bytes,', fig, 'figures'); });
