#!/usr/bin/env python3
"""Builds resources/pcb2gcode-options.json from pcb2gcode's src/options.cpp.

For pcb2gcode versions that can't print their own option metadata. It parses the
boost::program_options declarations and adds what options.cpp doesn't declare
(when options are used or required, examples, ranges, path kinds), taken from
check_parameters() and main.cpp; keep EXTRA below in step with those.

Usage: snapshot_from_options_cpp.py PCB2GCODE_SOURCE_DIR OUTPUT_JSON
"""
import json
import os
import re
import subprocess
import sys

if len(sys.argv) != 3:
    sys.exit(__doc__)
REPO = sys.argv[1]
OUT = sys.argv[2]
SRC = os.path.join(REPO, 'src', 'options.cpp')

text = open(SRC).read()


def balanced(s, i, open_c='(', close_c=')'):
    """Return index just past the bracket matching s[i] (== open_c)."""
    depth = 0
    j = i
    in_str = False
    while j < len(s):
        c = s[j]
        if in_str:
            if c == '\\':
                j += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return j + 1
        j += 1
    raise ValueError('unbalanced')


def split_top(s):
    """Split on top-level commas (outside (), <>, {}, strings)."""
    parts, cur, depth, in_str, k = [], '', 0, False, 0
    while k < len(s):
        c = s[k]
        if in_str:
            cur += c
            if c == '\\':
                cur += s[k + 1]
                k += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
            cur += c
        elif c in '(<{':
            depth += 1
            cur += c
        elif c == '>' and cur.endswith('-'):
            cur += c
        elif c in ')>}':
            depth -= 1
            cur += c
        elif c == ',' and depth == 0:
            parts.append(cur.strip())
            cur = ''
        else:
            cur += c
        k += 1
    if cur.strip():
        parts.append(cur.strip())
    return parts


def cstrings(s):
    return ''.join(m.group(1).encode().decode('unicode_escape')
                   for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', s))


# Group variable -> title
group_titles = {}
m = re.search(r'cli_options\("([^"]*)"\),\s*cfg_options\("([^"]*)"\)', text)
group_titles['cli_options'] = m.group(1)
group_titles['cfg_options'] = m.group(2)
for m in re.finditer(r'po::options_description\s+(\w+)\("([^"]*)"\)', text):
    group_titles[m.group(1)] = m.group(2)

group_ids = {
    'cli_options': 'cli',
    'cfg_options': 'generic',
    'drilling_options': 'drilling',
    'milling_options': 'milling',
    'outline_options': 'outline',
    'optimization_options': 'optimization',
    'autolevelling_options': 'autolevelling',
    'alignment_options': 'alignment',
    'cnc_options': 'cnc',
}

entries = []  # (group_var, name, short, semantic, description)
for m in re.finditer(r'(\w+)\.add_options\(\)', text):
    var = m.group(1)
    if var not in group_ids:
        continue
    k = m.end()
    while True:
        while k < len(text) and text[k] in ' \t\r\n':
            k += 1
        if k >= len(text) or text[k] != '(':
            break
        end = balanced(text, k)
        body = text[k + 1:end - 1]
        args = split_top(body)
        name_full = cstrings(args[0])
        name, _, short = name_full.partition(',')
        semantic = args[1] if len(args) == 3 else None
        desc = cstrings(args[-1])
        entries.append((var, name, short, semantic, desc))
        k = end

# Group order as printed by --help: cli, generic (own options), then sub-groups in add order.
group_order = ['cli_options', 'cfg_options']
for m in re.finditer(r'cfg_options\.add\((\w+)\)', text):
    group_order.append(m.group(1))


def cpp_type(semantic):
    if semantic is None:
        return None
    m = re.match(r'po::value<', semantic)
    start = semantic.index('<')
    end = balanced(semantic, start, '<', '>')
    return semantic[start + 1:end - 1].strip()


def call_arg(semantic, fn):
    if semantic is None:
        return None
    i = semantic.find(fn + '(')
    if i < 0:
        return None
    j = i + len(fn)
    end = balanced(semantic, j)
    return semantic[j + 1:end - 1].strip()


def convert_default(expr):
    if expr is None:
        return None
    e = re.sub(r'\s+', ' ', expr)
    if e in ('false', 'true'):
        return e
    if re.fullmatch(r'-?\d+(\.\d+)?', e):
        return e
    if re.fullmatch(r'\(\w+(\s\w+)?\)\s*\d+', e):
        return re.search(r'\d+', e).group(0)
    m = re.fullmatch(r'parse_unit<\w+>\("([^"]*)"\)', e)
    if m:
        return m.group(1).replace(' ', '')
    if 'numeric_limits<double>::infinity()' in e:
        return 'inf'
    if e in ('{{Length(0)}}', 'Length(0)'):
        return '0'
    m = re.fullmatch(r'std::vector<CommaSeparated<string>>\{\{"([^"]*)"\}\}', e)
    if m:
        return m.group(1)
    if e.startswith('std::vector<AvailableDrills>{}') or e.startswith('std::vector<string>{}'):
        return ''
    m = re.fullmatch(r'"((?:[^"\\]|\\.)*)"', e)
    if m:
        return m.group(1)
    enum_map = {'BoardSide::AUTO': 'auto', 'MillFeedDirection::ANY': 'any'}
    if e in enum_map:
        return enum_map[e]
    raise ValueError('unknown default expression: ' + e)


def map_type(ct):
    if ct is None:
        return 'none', None
    simple = {
        'bool': 'bool', 'int': 'int', 'unsigned int': 'uint', 'size_t': 'size', 'double': 'double',
        'string': 'string', 'Length': 'length', 'Velocity': 'velocity', 'Rpm': 'rpm', 'Time': 'time',
        'Percent': 'percent', 'boost::variant<Length, Percent>': 'length_or_percent',
        'BoardSide::BoardSide': 'enum', 'Software::Software': 'enum',
        'MillFeedDirection::MillFeedDirection': 'enum',
    }
    if ct in simple:
        return simple[ct], None
    lists = {
        'std::vector<CommaSeparated<Length>>': ('length', 'comma'),
        'std::vector<AvailableDrills>': ('drill', 'comma'),
        'std::vector<string>': ('string', 'repeat'),
        'std::vector<CommaSeparated<string>>': ('string', 'comma'),
    }
    if ct in lists:
        return lists[ct]
    raise ValueError('unknown type: ' + ct)


CHOICES = {
    'BoardSide::BoardSide': [{'value': 'auto'}, {'value': 'front'}, {'value': 'back'}],
    'Software::Software': [{'value': 'linuxcnc'}, {'value': 'mach3'}, {'value': 'mach4'},
                           {'value': 'custom'}],
    'MillFeedDirection::MillFeedDirection': [
        {'value': 'any'},
        {'value': 'climb', 'aliases': ['clockwise']},
        {'value': 'conventional', 'aliases': ['anticlockwise', 'counterclockwise']},
    ],
}

MILL = 'front || back'
MILLDRILL = 'drill && min-milldrill-hole-diameter < inf'
CUT = 'outline || (' + MILLDRILL + ')'
AL = 'al-front || al-back'

# Extra metadata not declared in options.cpp.
EXTRA = {
    # Generic
    'tolerance': {'bare_unit': 'length', 'min': '0', 'default_note': '0.0004in'},
    'output-dir': {'path': 'directory', 'default_note': 'current directory'},
    'basename': {'default_note': 'no prefix'},
    'preamble-text': {'path': 'input-text'},
    'preamble': {'path': 'input-gcode'},
    'postamble': {'path': 'input-gcode'},
    # Drilling
    'drill': {'path': 'input-excellon'},
    'milldrill-diameter': {'used_when': MILLDRILL, 'min': '0', 'min_exclusive': True,
                           'default_note': 'same as --cutter-diameter'},
    'min-milldrill-hole-diameter': {'used_when': 'drill', 'min': '0',
                                    'default_note': 'inf: milldrilling is off'},
    'min-milldrill-entry-diameter': {'used_when': MILLDRILL, 'min': '100%', 'min_exclusive': True},
    'max-milldrill-entry-diameter': {'used_when': MILLDRILL, 'min': '100%', 'min_exclusive': True},
    'milldrill-stepover': {'used_when': MILLDRILL, 'min': '0%', 'min_exclusive': True, 'max': '100%'},
    'zdrill': {'used_when': 'drill', 'required_when': 'drill', 'example': '-2.5mm'},
    'zmilldrill': {'used_when': MILLDRILL, 'default_note': 'same as --zdrill'},
    'drill-feed': {'used_when': 'drill', 'required_when': 'drill', 'example': '500mm/min',
                   'min': '0', 'min_exclusive': True},
    'drill-speed': {'used_when': 'drill', 'required_when': 'drill', 'example': '10000rpm', 'min': '0'},
    'drill-front': {'used_when': 'drill'},
    'drill-side': {'used_when': 'drill'},
    'drills-available': {'used_when': 'drill', 'default_note': 'use the sizes in the drill file'},
    'onedrill': {'used_when': 'drill'},
    'drill-output': {'used_when': 'drill', 'path': 'output-file'},
    'nog91-1': {'used_when': 'drill'},
    'nog81': {'used_when': 'drill'},
    'nom6': {'used_when': 'drill'},
    'milldrill-output': {'used_when': MILLDRILL, 'path': 'output-file'},
    # Milling
    'front': {'path': 'input-gerber'},
    'back': {'path': 'input-gerber'},
    'voronoi': {'used_when': MILL},
    'offset': {'used_when': MILL},
    'mill-diameters': {'used_when': MILL, 'example': '0.2mm', 'min': '0'},
    'milling-overlap': {'used_when': MILL},
    'isolation-width': {'used_when': MILL, 'min': '0'},
    'extra-passes': {'used_when': MILL},
    'pre-milling-gcode': {'used_when': MILL},
    'post-milling-gcode': {'used_when': MILL},
    'zwork': {'used_when': MILL, 'required_when': MILL, 'example': '-0.05mm'},
    'mill-feed': {'used_when': MILL, 'required_when': MILL, 'example': '600mm/min',
                  'min': '0', 'min_exclusive': True},
    'mill-vertfeed': {'used_when': MILL, 'min': '0', 'min_exclusive': True,
                      'default_note': 'half of --mill-feed'},
    'mill-infeed': {'used_when': MILL, 'min': '0', 'min_exclusive': True,
                    'default_note': 'full depth in one pass'},
    'mill-speed': {'used_when': MILL, 'required_when': MILL, 'example': '10000rpm', 'min': '0'},
    'mill-feed-direction': {'used_when': MILL},
    'invert-gerbers': {'used_when': MILL},
    'draw-gerber-lines': {'used_when': 'front || back || outline'},
    'preserve-thermal-reliefs': {'used_when': '(' + MILL + ') && voronoi'},
    'front-output': {'used_when': 'front', 'path': 'output-file'},
    'back-output': {'used_when': 'back', 'path': 'output-file'},
    # Outline
    'outline': {'path': 'input-gerber'},
    'fill-outline': {'used_when': 'outline'},
    'cutter-diameter': {'used_when': CUT, 'required_when': CUT, 'example': '3mm',
                        'min': '0', 'min_exclusive': True},
    'zcut': {'used_when': CUT, 'required_when': CUT, 'example': '-2.5mm', 'max': '0'},
    'cut-feed': {'used_when': CUT, 'required_when': CUT, 'example': '600mm/min',
                 'min': '0', 'min_exclusive': True},
    'cut-vertfeed': {'used_when': CUT, 'min': '0', 'min_exclusive': True,
                     'default_note': 'half of --cut-feed'},
    'cut-speed': {'used_when': CUT, 'required_when': CUT, 'example': '10000rpm', 'min': '0'},
    'cut-infeed': {'used_when': CUT, 'required_when': CUT, 'example': '1mm', 'min': '0.001in'},
    'cut-front': {'used_when': 'outline'},
    'cut-side': {'used_when': 'outline'},
    'bridges': {'used_when': 'outline', 'min': '0'},
    'bridgesnum': {'used_when': 'outline'},
    'zbridges': {'used_when': 'outline', 'default_note': 'same as --zsafe'},
    'outline-output': {'used_when': 'outline', 'path': 'output-file'},
    # Optimization
    'optimise': {'min': '0'},
    'path-finding-limit': {},
    'g0-vertical-speed': {'min': '0', 'min_exclusive': True},
    'g0-horizontal-speed': {'min': '0', 'min_exclusive': True},
    'backtrack': {'min': '0', 'default_note': 'inf: no limit'},
    # Autolevelling
    'al-front': {'used_when': 'front'},
    'al-back': {'used_when': 'back'},
    'software': {'used_when': AL, 'required_when': AL, 'example': 'linuxcnc'},
    'al-x': {'used_when': AL, 'required_when': AL, 'example': '15mm', 'min': '0', 'min_exclusive': True},
    'al-y': {'used_when': AL, 'required_when': AL, 'example': '15mm', 'min': '0', 'min_exclusive': True},
    'al-probefeed': {'used_when': AL, 'required_when': AL, 'example': '100mm/min',
                     'min': '0', 'min_exclusive': True},
    'al-probe-on': {'used_when': AL},
    'al-probe-off': {'used_when': AL},
    'al-probecode': {'used_when': AL},
    'al-probevar': {'used_when': AL},
    'al-setzzero': {'used_when': AL},
    # CNC
    'zsafe': {'required_when': 'true', 'example': '5mm'},
    'spinup-time': {'min': '0'},
    'spindown-time': {'min': '0', 'default_note': 'same as --spinup-time'},
    'zchange': {'required_when': 'true', 'example': '10mm'},
    'tile-x': {'min': '1'},
    'tile-y': {'min': '1'},
}

REPLACED_BY = {
    'milldrill': 'min-milldrill-hole-diameter',
    'drill-front': 'drill-side',
    'cut-front': 'cut-side',
    'extra-passes': 'isolation-width',
    'g64': 'tolerance',
}

CONSTRAINTS = [
    {'when': MILL, 'require': 'zsafe > zwork', 'message': '--zsafe must be higher than --zwork.'},
    {'when': 'drill', 'require': 'zsafe > zdrill', 'message': '--zsafe must be higher than --zdrill.'},
    {'when': 'drill && !zchange-absolute', 'require': 'zchange > zdrill',
     'message': '--zchange must be higher than --zdrill. If --zchange is a machine coordinate, '
                'add --zchange-absolute.'},
    {'when': CUT, 'require': 'zsafe > zcut', 'message': '--zsafe must be higher than --zcut.'},
    {'when': MILLDRILL, 'require': 'max-milldrill-entry-diameter >= min-milldrill-entry-diameter',
     'message': "--max-milldrill-entry-diameter can't be less than --min-milldrill-entry-diameter."},
    {'when': 'mill-feed-direction != any', 'require': '!tsp-2opt',
     'message': "--mill-feed-direction doesn't work with --tsp-2opt, which is on by default. "
                'Add --tsp-2opt=false.'},
]

options = []
seen = set()
for var, name, short, semantic, desc in entries:
    assert name not in seen, name
    seen.add(name)
    ct = cpp_type(semantic)
    typ, multiple = map_type(ct)
    o = {'name': name}
    if short:
        o['short_name'] = short
    o['group'] = group_ids[var]
    o['description'] = re.sub(r'\s+', ' ', desc).strip()
    o['type'] = typ
    if multiple:
        o['multiple'] = multiple
    if ct in CHOICES:
        o['choices'] = CHOICES[ct]
    d = convert_default(call_arg(semantic, 'default_value'))
    o['default'] = d
    imp = call_arg(semantic, 'implicit_value')
    if imp is not None:
        o['implicit'] = convert_default(imp)
    if var == 'cli_options':
        o['cli_only'] = True
    if '[DEPRECATED' in desc:
        o['deprecated'] = True
        if name in REPLACED_BY:
            o['replaced_by'] = REPLACED_BY[name]
    o.update(EXTRA.get(name, {}))
    options.append(o)

for k in EXTRA:
    assert k in seen, 'EXTRA refers to unknown option ' + k

commit = subprocess.check_output(['git', '-C', REPO, 'rev-parse', 'HEAD']).decode().strip()
version = re.search(r'VERSION\s+(\d+\.\d+\.\d+)', open(os.path.join(REPO, 'CMakeLists.txt')).read()).group(1)

doc = {
    'format': 'pcb2gcode-options',
    'format_version': 1,
    'pcb2gcode_version': version,
    'pcb2gcode_commit': commit,
    'note': 'Used when the installed pcb2gcode cannot describe its own options. '
            'Regenerate with tools/update-option-snapshot.sh.',
    'groups': [{'id': group_ids[g], 'title': group_titles[g]} for g in group_order],
    'options': options,
    'constraints': CONSTRAINTS,
}
with open(OUT, 'w') as f:
    json.dump(doc, f, indent=2, ensure_ascii=False)
    f.write('\n')
print(len(options), 'options written to', OUT)
