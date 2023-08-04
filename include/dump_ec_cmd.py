commands = []

with open('ec_commands.h', 'r') as f:
  for line in f:
    if '#define EC_CMD' in line:
      line_s = line.strip().split(' ')
      if line_s[0] == '#define':
        try:
          commands.append((int(line_s[2], 16), line_s[1]))
        except Exception:
          pass

commands = sorted(commands)
print(len(commands))

#with open('ec_command_names.h', 'w') as f:
#  for code, name in commands:
#    f.write('\t{%d, "%s"},\n' % (code, name))
