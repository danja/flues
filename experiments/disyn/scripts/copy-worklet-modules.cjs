#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

const srcDir = path.resolve(__dirname, '../src/audio/modules');
const destDir = path.resolve(__dirname, '../dist/assets/modules');

if (!fs.existsSync(srcDir)) {
  process.exit(0);
}

fs.mkdirSync(destDir, { recursive: true });

for (const file of fs.readdirSync(srcDir)) {
  if (file.endsWith('.js')) {
    fs.copyFileSync(path.join(srcDir, file), path.join(destDir, file));
  }
}
