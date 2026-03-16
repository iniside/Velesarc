import os
import re

# Your block comment header
NEW_LICENSE = '''\
/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */
'''

EXTENSIONS = ('.h', '.cpp')

def has_shebang(line):
    return line.startswith('#!')


def is_in_intermediate(path):
    return any(part.lower() == 'intermediate' for part in path.replace('\\', '/').split('/'))

def strip_top_block_comments(text):
    lines = text.splitlines(keepends=True)
    output = []
    i = 0
    # Skip all top block comments (and the whitespace or line comments between them)
    while i < len(lines):
        line = lines[i]
        # Skip blank lines
        if line.strip() == '':
            i += 1
            continue
        # Skip // comments
        if line.strip().startswith('//'):
            i += 1
            continue
        # Remove block comments one after the other
        if line.lstrip().startswith('/*'):
            # Find end of this comment
            while i < len(lines) and '*/' not in lines[i]:
                i += 1
            # Skip the line with the */
            if i < len(lines):
                i += 1
            continue
        # As soon as we see non-comment, non-blank content, stop
        break
    # Return the rest from i onwards
    return ''.join(lines[i:]).lstrip('\r\n')

def update_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        old_text = f.read()
    new_body = strip_top_block_comments(old_text)
    # Only do anything if body actually changes
    if new_body == old_text.lstrip('\r\n'):
        print(f"License not found or already updated: {path}")
        return
    backup_path = path + '.bak'
    os.rename(path, backup_path)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(NEW_LICENSE.rstrip() + '\n\n' + new_body)
    print(f"Updated: {path} (backup at {backup_path})")

for root, dirs, files in os.walk('.'):
    dirs[:] = [d for d in dirs if d.lower() != 'intermediate']
    for fname in files:
        if fname.endswith(EXTENSIONS):
            path = os.path.join(root, fname)
            if not is_in_intermediate(path):
                update_file(path)



