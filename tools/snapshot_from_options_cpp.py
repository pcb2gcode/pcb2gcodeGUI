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
    """Returns (type, repeatable, comma_separated)."""
    if ct is None:
        return 'none', False, False
    simple = {
        'bool': 'bool', 'int': 'int', 'unsigned int': 'uint', 'size_t': 'uint', 'double': 'double',
        'string': 'string', 'Length': 'length', 'Velocity': 'velocity', 'Rpm': 'rpm', 'Time': 'time',
        'Percent': 'percent', 'boost::variant<Length, Percent>': 'length_or_percent',
        'BoardSide::BoardSide': 'enum', 'Software::Software': 'enum',
        'MillFeedDirection::MillFeedDirection': 'enum',
    }
    if ct in simple:
        return simple[ct], False, False
    lists = {
        'std::vector<CommaSeparated<Length>>': ('length', True, True),
        'std::vector<AvailableDrills>': ('available_drill', True, True),
        'std::vector<string>': ('string', True, False),
        'std::vector<CommaSeparated<string>>': ('string', True, True),
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

MILLDRILL_EXPR = {'all': [{'set': 'drill'}, {'finite': 'min-milldrill-hole-diameter'}]}
CONDITIONS = {
    'always': {'description': 'always', 'expression': True},
    'milling': {'description': 'front or back is set',
                'expression': {'any': [{'set': 'front'}, {'set': 'back'}]}},
    'drilling': {'description': 'drill is set', 'expression': {'set': 'drill'}},
    'milldrilling': {'description': 'drill is set and min-milldrill-hole-diameter is finite',
                     'expression': MILLDRILL_EXPR},
    'cutting': {'description': 'outline is set, or milldrilling',
                'expression': {'any': [{'set': 'outline'}, MILLDRILL_EXPR]}},
    'autolevelling': {'description': 'al-front is true or al-back is true',
                      'expression': {'any': [{'equal': ['al-front', 'true']},
                                             {'equal': ['al-back', 'true']}]}},
}

TYPES = {
    'bool': {'true': ['true', 'yes', 'on', '1'], 'false': ['false', 'no', 'off', '0']},
    'length': {'bare_unit': 'in', 'metric_bare_unit': 'mm', 'base_unit': 'm', 'units': [
        {'name': 'mm', 'aliases': ['millimeter', 'millimeters'], 'factor': 0.001},
        {'name': 'm', 'aliases': ['meter'], 'factor': 1},
        {'name': 'in', 'aliases': ['inch', 'inches'], 'factor': 0.0254},
        {'name': 'thou', 'aliases': ['thous', 'mil', 'mils'], 'factor': 0.0000254}]},
    'time': {'bare_unit': 'ms', 'base_unit': 's', 'units': [
        {'name': 's', 'aliases': ['second', 'seconds'], 'factor': 1},
        {'name': 'ms', 'aliases': ['millisecond', 'milliseconds', 'millis'], 'factor': 0.001},
        {'name': 'min', 'aliases': ['mins', 'minute', 'minutes'], 'factor': 60}]},
    'velocity': {'bare_unit': 'in/min', 'metric_bare_unit': 'mm/min'},
    'revolution': {'base_unit': 'rev', 'units': [
        {'name': 'rev', 'aliases': ['revs', 'rotation', 'rotations', 'revolution', 'revolutions',
                                    'cycle', 'cycles'], 'factor': 1}]},
    'rpm': {'bare_unit': 'rpm', 'units': [{'name': 'rpm', 'aliases': ['RPM'], 'factor': 1}]},
    'percent': {'bare_unit': 'fraction', 'units': [{'name': '%', 'aliases': [], 'factor': 0.01}]},
}

# What options.cpp doesn't declare: when options are used, file kinds and notes about defaults.
EXTRA = {
    'tolerance': {'bare_unit': 'in', 'metric_bare_unit': 'mm', 'default_note': '0.0004in'},
    'g64': {'bare_unit': 'in', 'metric_bare_unit': 'mm'},
    'config': {'path': 'input_file', 'file_format': 'millproject'},
    'output-dir': {'path': 'directory', 'default_note': 'current directory'},
    'basename': {'default_note': 'no prefix'},
    'preamble-text': {'path': 'input_file', 'file_format': 'text'},
    'preamble': {'path': 'input_file', 'file_format': 'gcode'},
    'postamble': {'path': 'input_file', 'file_format': 'gcode'},
    'drill': {'path': 'input_file', 'file_format': 'excellon'},
    'milldrill-diameter': {'used_when': 'milldrilling', 'default_note': 'same as --cutter-diameter'},
    'min-milldrill-hole-diameter': {'used_when': 'drilling', 'default_note': 'inf: no milldrilling'},
    'min-milldrill-entry-diameter': {'used_when': 'milldrilling'},
    'max-milldrill-entry-diameter': {'used_when': 'milldrilling'},
    'milldrill-stepover': {'used_when': 'milldrilling'},
    'zdrill': {'used_when': 'drilling'},
    'zmilldrill': {'used_when': 'milldrilling', 'default_note': 'same as --zdrill'},
    'drill-feed': {'used_when': 'drilling'},
    'drill-speed': {'used_when': 'drilling'},
    'drill-front': {'used_when': 'drilling'},
    'drill-side': {'used_when': 'drilling'},
    'drills-available': {'used_when': 'drilling'},
    'onedrill': {'used_when': 'drilling'},
    'drill-output': {'used_when': 'drilling', 'path': 'output_file', 'file_format': 'gcode',
                     'relative_to': 'output-dir'},
    'nog91-1': {'used_when': 'drilling'},
    'nog81': {'used_when': 'drilling'},
    'nom6': {'used_when': 'drilling'},
    'milldrill-output': {'used_when': 'milldrilling', 'path': 'output_file', 'file_format': 'gcode',
                         'relative_to': 'output-dir'},
    'front': {'path': 'input_file', 'file_format': 'gerber'},
    'back': {'path': 'input_file', 'file_format': 'gerber'},
    'outline': {'path': 'input_file', 'file_format': 'gerber'},
    'mill-vertfeed': {'default_note': 'half of --mill-feed'},
    'mill-infeed': {'default_note': 'the whole depth in one pass'},
    'front-output': {'path': 'output_file', 'file_format': 'gcode', 'relative_to': 'output-dir'},
    'back-output': {'path': 'output_file', 'file_format': 'gcode', 'relative_to': 'output-dir'},
    'draw-gerber-lines': {'used_when': 'always'},
    'cut-vertfeed': {'default_note': 'half of --cut-feed'},
    'zbridges': {'default_note': 'same as --zsafe'},
    'outline-output': {'path': 'output_file', 'file_format': 'gcode', 'relative_to': 'output-dir'},
    'backtrack': {'default_note': 'inf: retrace whenever it is faster'},
    'spindown-time': {'default_note': 'same as --spinup-time'},
}
# Options used only while a condition holds, by group (overridden by EXTRA).
GROUP_USED_WHEN = {'milling': 'milling', 'outline': 'cutting', 'autolevelling': 'autolevelling'}
ALWAYS_USED = {'front', 'back', 'outline', 'drill', 'al-front', 'al-back'}

REPLACEMENTS = {
    'milldrill': ('min-milldrill-hole-diameter', {'true': '0'}),
    'drill-front': ('drill-side', {'true': 'front', 'false': 'back'}),
    'cut-front': ('cut-side', {'true': 'front', 'false': 'back'}),
    'g64': ('tolerance', None),
}

CONDITION_OF_CHECK = {
    'check_generic_parameters': 'always',
    'check_milling_parameters': 'milling',
    'check_drilling_parameters': 'drilling',
    'check_cutting_parameters': 'cutting',
}
# Required options per check, as found in the missing_option() calls.
required = []
for fn, cond in CONDITION_OF_CHECK.items():
    body = text[text.index('static void ' + fn):]
    body = body[:body.index('\n}\n')]
    for m in re.finditer(r'missing_option\("([^"]+)",\s*((?:"(?:[^"\\]|\\.)*"\s*)+),\s*"([^"]+)"\),\s*(ERR_\w+)',
                         body):
        option, description, example, code = m.group(1), cstrings(m.group(2)), m.group(3), m.group(4)
        when = 'autolevelling' if option in ('software', 'al-x', 'al-y', 'al-probefeed') else cond
        required.append((option, description, example, code, when))

# Range checks in check_parameters(): (option, when, min, min_exclusive, max, max_exclusive, message, code)
RANGES = [
    ('spinup-time', 'always', '0', False, None, False, "--spinup-time can't be negative.", 'ERR_NEGATIVESPINUP'),
    ('spindown-time', 'always', '0', False, None, False, "--spindown-time can't be negative.", 'ERR_NEGATIVESPINDOWN'),
    ('tolerance', 'always', '0', False, None, False,
     "--tolerance can't be negative. Leave it out to use the default.", 'ERR_NEGATIVETOLERANCE'),
    ('tile-x', 'always', '1', False, None, False, '--tile-x must be at least 1.', 'ERR_NEGATIVETILEX'),
    ('tile-y', 'always', '1', False, None, False, '--tile-y must be at least 1.', 'ERR_NEGATIVETILEY'),
    ('al-x', 'autolevelling', '0', True, None, False, '--al-x must be greater than 0.', 'ERR_NEGATIVEALX'),
    ('al-y', 'autolevelling', '0', True, None, False, '--al-y must be greater than 0.', 'ERR_NEGATIVEALY'),
    ('al-probefeed', 'autolevelling', '0', True, None, False, '--al-probefeed must be greater than 0.',
     'ERR_NEGATIVEPROBEFEED'),
    ('mill-feed', 'milling', '0', True, None, False, '--mill-feed must be greater than 0.', 'ERR_NEGATIVEMILLFEED'),
    ('mill-vertfeed', 'milling', '0', True, None, False, '--mill-vertfeed must be greater than 0.',
     'ERR_NEGATIVEMILLVERTFEED'),
    ('mill-infeed', 'milling', '0', True, None, False, '--mill-infeed must be greater than 0.', 'ERR_LOWMILLINFEED'),
    ('mill-speed', 'milling', '0', False, None, False, "--mill-speed can't be negative.", 'ERR_NEGATIVEMILLSPEED'),
    ('drill-feed', 'drilling', '0', True, None, False, '--drill-feed must be greater than 0.', 'ERR_NEGATIVEDRILLFEED'),
    ('drill-speed', 'drilling', '0', False, None, False, "--drill-speed can't be negative.", 'ERR_NEGATIVEDRILLSPEED'),
    ('zcut', 'cutting', None, False, '0', False, "--zcut can't be above 0, the surface of the board.",
     'ERR_NEGATIVEZWORK'),
    ('cut-feed', 'cutting', '0', True, None, False, '--cut-feed must be greater than 0.', 'ERR_NEGATIVECUTFEED'),
    ('cut-vertfeed', 'cutting', '0', True, None, False, '--cut-vertfeed must be greater than 0.',
     'ERR_NEGATIVECUTVERTFEED'),
    ('cut-speed', 'cutting', '0', False, None, False, "--cut-speed can't be negative.", 'ERR_NEGATIVESPINDLESPEED'),
    ('cut-infeed', 'cutting', '0.001in', False, None, False, '--cut-infeed must be at least 0.001in.',
     'ERR_LOWCUTINFEED'),
    ('bridges', 'cutting', '0', False, None, False, "--bridges can't be negative.", 'ERR_NEGATIVEBRIDGE'),
    ('milldrill-stepover', 'milldrilling', '0%', True, '100%', False,
     '--milldrill-stepover must be greater than 0% and at most 100%.', 'ERR_INVALIDMILLDRILLSTEPOVER'),
    ('min-milldrill-entry-diameter', 'milldrilling', '100%', True, None, False,
     '--min-milldrill-entry-diameter must be greater than 100%, which is the milldrill diameter.',
     'ERR_LOWMILLDRILLENTRY'),
]

COMPARISONS = [
    ('greater', 'zsafe', 'zwork', 'milling', None, '--zsafe must be higher than --zwork.', 'ERR_ZSAFELOWERZWORK'),
    ('greater', 'zsafe', 'zdrill', 'drilling', None, '--zsafe must be higher than --zdrill.', 'ERR_ZSAFELOWERZDRILL'),
    ('greater', 'zchange', 'zdrill', 'drilling', {'equal': ['zchange-absolute', 'true']},
     '--zchange must be higher than --zdrill. If --zchange is a machine coordinate, add --zchange-absolute.',
     'ERR_ZSAFELOWERZCHANGE'),
    ('greater', 'zsafe', 'zcut', 'cutting', None, '--zsafe must be higher than --zcut.', 'ERR_ZSAFELOWERZCUT'),
    ('greater_or_equal', 'max-milldrill-entry-diameter', 'min-milldrill-entry-diameter', 'milldrilling', None,
     "--max-milldrill-entry-diameter can't be less than --min-milldrill-entry-diameter.", 'ERR_MILLDRILLENTRYORDER'),
]

FORBIDDEN = [
    (['mirror-absolute'], 'always', {'equal': ['mirror-absolute', 'false']},
     "--mirror-absolute=false isn't supported anymore. Remove it, and use --mirror-axis to choose where the "
     'back side is mirrored.', 'ERR_FALSEMIRRORABSOLUTE'),
    (['mill-feed-direction', 'tsp-2opt'], 'always',
     {'all': [{'not': {'equal': ['mill-feed-direction', 'any']}}, {'equal': ['tsp-2opt', 'true']}]},
     "--mill-feed-direction doesn't work with --tsp-2opt, which is on by default. Add --tsp-2opt=false.",
     'ERR_INVALIDPARAMETER'),
    (['vectorial'], 'milling', {'equal': ['vectorial', 'false']},
     "--vectorial=false isn't supported anymore. Remove it.", 'ERR_INVALIDPARAMETER'),
    (['drill-front', 'drill-side'], 'drilling', {'all': [{'set': 'drill-front'}, {'given': 'drill-side'}]},
     "--drill-front and --drill-side can't both be set. --drill-front is deprecated, so use only --drill-side.",
     'ERR_BOTHDRILLFRONTSIDE'),
    (['cut-front', 'cut-side'], 'cutting', {'all': [{'set': 'cut-front'}, {'given': 'cut-side'}]},
     "--cut-front and --cut-side can't both be set. --cut-front is deprecated, so use only --cut-side.",
     'ERR_BOTHCUTFRONTSIDE'),
]

header = open(os.path.join(REPO, 'src', 'options.hpp')).read()
exit_codes = sorted(((int(v), n) for n, v in re.findall(r'(ERR_\w+)\s*=\s*(\d+)', header)))
code_of = {n: c for c, n in exit_codes}

required_when = {r[0]: r[4] for r in required}
examples = {r[0]: r[2] for r in required}
examples['mill-diameters'] = '0.2mm'

options = []
seen = set()
for var, name, short, semantic, desc in entries:
    assert name not in seen, name
    seen.add(name)
    ct = cpp_type(semantic)
    typ, repeatable, comma = map_type(ct)
    o = {'name': name}
    if short:
        o['short'] = short
    o['group'] = group_ids[var]
    o['description'] = re.sub(r'\s+', ' ', desc).strip()
    o['type'] = typ
    d = convert_default(call_arg(semantic, 'default_value'))
    if repeatable:
        o['default'] = [] if d is None or d == '' else [d]
        o['repeatable'] = True
        if comma:
            o['comma_separated'] = True
        if '->multitoken()' in re.sub(r'\s+', '', semantic):
            o['multitoken'] = True
    else:
        o['default'] = d
    imp = call_arg(semantic, 'implicit_value')
    if imp is not None:
        o['implicit'] = convert_default(imp)
    if ct in CHOICES:
        o['choices'] = CHOICES[ct]
    for r in RANGES:
        if r[0] == name:
            rng = {}
            if r[2] is not None:
                rng['min'] = r[2]
                if r[3]:
                    rng['min_exclusive'] = True
            if r[4] is not None:
                rng['max'] = r[4]
                if r[5]:
                    rng['max_exclusive'] = True
            o['range'] = rng
    if name in examples:
        o['example'] = examples[name]
    if name in required_when:
        o['required_when'] = required_when[name]
    o['used_when'] = ('always' if name in ALWAYS_USED else
                      GROUP_USED_WHEN.get(group_ids[var], 'always'))
    if var == 'cli_options':
        o['cli_only'] = True
    if '[DEPRECATED' in desc:
        o['deprecated'] = True
        if name in REPLACEMENTS:
            o['replaced_by'], value_map = REPLACEMENTS[name]
            if value_map:
                o['value_map'] = value_map
    o.update(EXTRA.get(name, {}))
    options.append(o)

for k in EXTRA:
    assert k in seen, 'EXTRA refers to unknown option ' + k

rules = []
for option, description, example, code, when in required:
    rules.append({'kind': 'required', 'options': [option], 'when': when, 'code': code_of[code],
                  'message': 'Missing --%s, %s. For example: --%s=%s' % (option, description, option, example),
                  'description': description, 'example': example})
for option, when, mn, mn_ex, mx, mx_ex, message, code in RANGES:
    rule = {'kind': 'range', 'options': [option], 'when': when, 'code': code_of[code], 'message': message}
    if mn is not None:
        rule['min'] = mn
        if mn_ex:
            rule['min_exclusive'] = True
    if mx is not None:
        rule['max'] = mx
        if mx_ex:
            rule['max_exclusive'] = True
    rules.append(rule)
for kind, a, b, when, unless, message, code in COMPARISONS:
    rule = {'kind': kind, 'options': [a, b], 'when': when, 'code': code_of[code], 'message': message}
    if unless:
        rule['unless'] = unless
    rules.append(rule)
for opts, when, condition, message, code in FORBIDDEN:
    rules.append({'kind': 'forbidden', 'options': opts, 'when': when, 'if': condition, 'code': code_of[code],
                  'message': message})

commit = subprocess.check_output(['git', '-C', REPO, 'rev-parse', 'HEAD']).decode().strip()
describe = subprocess.check_output(['git', '-C', REPO, 'describe', '--always', '--tags']).decode().strip()
version = re.search(r'VERSION\s+(\d+\.\d+\.\d+)', open(os.path.join(REPO, 'CMakeLists.txt')).read()).group(1)

doc = {
    'format_version': 1,
    'program': 'pcb2gcode',
    'version': version,
    'git_commit': describe,
    'source_commit': commit,
    'note': 'Made from src/options.cpp by tools/snapshot_from_options_cpp.py, in the format of '
            'pcb2gcode --dump-options. Used when the installed pcb2gcode cannot describe its options.',
    'types': TYPES,
    'conditions': CONDITIONS,
    'groups': [{'id': group_ids[g], 'title': group_titles[g]} for g in group_order],
    'options': options,
    'rules': rules,
    'exit_codes': [{'code': c, 'name': n} for c, n in exit_codes],
}
with open(OUT, 'w') as f:
    json.dump(doc, f, indent=2, ensure_ascii=False)
    f.write('\n')
print(len(options), 'options and', len(rules), 'rules written to', OUT)
