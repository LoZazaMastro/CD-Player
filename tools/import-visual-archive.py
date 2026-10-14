"""Preserve every live gallery drawing program and build the native selection catalog."""
from pathlib import Path
from html.parser import HTMLParser
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'references/iwrzwr-visual-archive-main/dist'
OUTPUT = ROOT / 'assets/visualizers'
OUTPUT.mkdir(parents=True, exist_ok=True)

class Tree(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.nodes = [{'tag':'body','attrs':{},'parent':-1}]
        self.stack = [0]
        self.scripts = []
        self.capture = None
    def handle_starttag(self, tag, attrs):
        if tag in ('script','style'):
            self.capture = (tag, '')
            return
        if self.capture is not None:
            return
        index = len(self.nodes)
        self.nodes.append({'tag':tag, 'attrs':dict((k,v or '') for k,v in attrs), 'parent':self.stack[-1]})
        if tag not in ('br','img','input','hr','meta','link','source','wbr'):
            self.stack.append(index)
    def handle_endtag(self, tag):
        if self.capture is not None:
            if tag == self.capture[0]:
                if tag == 'script' and self.capture[1].strip():
                    self.scripts.append(self.capture[1])
                self.capture = None
            return
        for at in range(len(self.stack)-1, 0, -1):
            if self.nodes[self.stack[at]]['tag'] == tag:
                self.stack = self.stack[:at]
                break
    def handle_data(self, text):
        if self.capture is not None:
            self.capture = (self.capture[0], self.capture[1]+text)

catalog = json.loads((SOURCE/'catalog.json').read_text(encoding='utf-8'))
entries = []
families = []
for family in catalog:
    if family['group'] not in ('İlk denemeler', 'Gece serileri', 'Matrix ve ses'):
        continue
    data = json.loads((SOURCE/'studies'/ (family['id']+'.json')).read_text(encoding='utf-8'))
    tree = Tree()
    tree.feed(data['html'])
    assert tree.scripts, family['id']
    program = {'nodes': tree.nodes, 'scripts':tree.scripts}
    (OUTPUT/(family['id']+'.json')).write_text(json.dumps(program,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    for index, name in enumerate(family['names']):
        entries.append({'title':name,'family':family['id'],'index':index})
    families.append(family['id'])
assert len(entries) == 152
(OUTPUT/'catalog.json').write_text(json.dumps(entries,indent=2),encoding='utf-8')
header = ['// CD Player - Complete iwrzwr live-gallery study catalog.','// Copyright (C) 2026 Andrea; studies (C) 2026 Kagan Yaldizkaya, MIT.','// SPDX-License-Identifier: GPL-3.0-or-later','#pragma once','#include <array>','#include <string_view>','namespace cd {','struct ArchiveEntry { std::string_view title, family; unsigned index; };',f'inline constexpr std::array<ArchiveEntry,{len(entries)}> archive_entries = {{{{']
for entry in entries:
    header.append('{'+json.dumps(entry['title'])+','+json.dumps(entry['family'])+','+str(entry['index'])+'},')
header += ['}};','inline constexpr unsigned visualizer_count = unsigned(archive_entries.size()) + 9;','}']
(ROOT/'src/app/archive_catalog.hpp').write_text('\n'.join(header)+'\n',encoding='utf-8')
shutil.copy2(ROOT/'src/third_party/quickjs/LICENSE',ROOT/'licenses/QuickJS-MIT.txt')
print(f'{len(entries)} real drawing studies, {len(families)} families, 9 separate composition studies')
