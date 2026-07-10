#!/usr/bin/env node
'use strict';
const fs = require('fs');
const path = require('path');

const USER = path.join(__dirname, '..', 'User');
const OUT = path.join(__dirname, '..', 'docs', 'FILE_INDEX.generated.md');

const rows = [];
(function walk(d) {
  for (const e of fs.readdirSync(d, { withFileTypes: true })) {
    const p = path.join(d, e.name);
    if (e.isDirectory()) walk(p);
    else if (/\.(c|h)$/.test(e.name)) {
      const c = fs.readFileSync(p, 'utf8');
      const rel = path.relative(USER, p).replace(/\\/g, '/');
      const m = c.match(/@file\s+(\S+)[\s\S]*?@brief\s+([^\n*]+)/);
      rows.push({ rel, brief: m ? m[2].trim() : '（缺少 @brief）' });
    }
  }
})(USER);

rows.sort((a, b) => a.rel.localeCompare(b.rel));

const body = [
  '# User 源码文件索引（自动生成）',
  '',
  '> 由 `node tools/generate_file_index.js` 扫描 `@file` / `@brief` 生成。',
  '',
  '| 路径 | 功能说明 |',
  '| --- | --- |',
  ...rows.map((r) => '| `' + r.rel + '` | ' + r.brief.replace(/\|/g, '\\|') + ' |'),
  '',
  '共 **' + rows.length + '** 个文件。',
  '',
].join('\n');

fs.writeFileSync(OUT, body, 'utf8');
console.log('Wrote', OUT, 'rows=', rows.length);
