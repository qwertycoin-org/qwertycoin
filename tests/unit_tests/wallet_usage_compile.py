import json
import pathlib
import shlex
import subprocess
import sys
import tempfile


compile_commands = json.loads(pathlib.Path(sys.argv[1]).read_text())
command = next(entry for entry in compile_commands if entry["file"].endswith("/src/simplewallet/simplewallet.cpp"))
arguments = shlex.split(command["command"])
arguments.insert(arguments.index("-c"), "-Werror=unused-value")
with tempfile.TemporaryDirectory() as directory:
    arguments[arguments.index("-o") + 1] = str(pathlib.Path(directory) / "simplewallet.o")
    sys.exit(subprocess.run(arguments, cwd=command["directory"], check=False).returncode)
