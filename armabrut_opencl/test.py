import subprocess
import os
import sys

skey = "432421239530A741F13EC72964DDF67BD5DEA9CD59D38329CF773ABE6CA7699DDCFE2396DA6EC77F91551B"
salt = "B4FA5ADD"
chks = ["61242B27", "874ABA70", "1C191384", "999AD987"]
for chk in chks:
  for i in range(1, 6):
    offset = i * 4
    data = skey[offset:(offset + 8)]
    args = ["D:/mazda3/software/armabrut_opencl_v0.9.5/armabrut_opencl.exe", "-a", "8", "-h", chk, "-p", salt, "-d", data]
    print("Chk: %s Data: %s" % (chk, data))
    sys.stdout.flush()
    process = subprocess.Popen(args)
    process.wait()
    print("\n")
    sys.stdout.flush()