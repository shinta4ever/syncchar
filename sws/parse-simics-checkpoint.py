cp_file = file(simenv.checkpoint_name, "r")
cp_lines = []
common_lines = []
in_common = False
for line in cp_file:
   if line == "OBJECT common TYPE common {\n":
      in_common = True

   if in_common:
      common_lines.append(line)
      if line == "}\n":
         in_common = False
   else:
      cp_lines.append(line)

cp_file = file(simenv.checkpoint_name, "w")
cp_file.writelines(cp_lines)

common_file = file(simenv.checkpoint_name + "-common", "w")
common_file.writelines(common_lines)
