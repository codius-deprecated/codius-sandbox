#!/usr/bin/env python
import sys

names = {}
fh = open(sys.argv[1], 'r')
for f in fh.readlines():
  tokens = f.strip().split(' ')
  if '#define' in tokens and len(tokens) > 2:
    name, value = tokens[1:3]
    if name.startswith('__NR_'):
      names[int(value)] = name[5:]

syscallMap = []
for i in sorted(names.keys()):
  syscallMap.append('{%s, "%s"}'%(i, names[i]))
out = open(sys.argv[2], 'w')
out.write("std::map<int, std::string> names = {\n")
out.write(',\n'.join(syscallMap))
out.write("\n};")
