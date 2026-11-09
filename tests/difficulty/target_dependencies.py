from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temporary:
    source = Path(temporary) / "source"
    source.mkdir()
    (source / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.10)
project(difficulty_dependency_probe CXX)
file(WRITE "${CMAKE_BINARY_DIR}/dummy.cpp" "int main() { return 0; }\n")
add_library(cryptonote_basic INTERFACE)
function(monero_add_minimal_executable name)
  add_executable(${name} "${CMAKE_BINARY_DIR}/dummy.cpp")
endfunction()
set(PYTHON_EXECUTABLE python3)
add_custom_target(unit_tests)
include("${SOURCE_ROOT}/tests/difficulty/CMakeLists.txt")
""")
    build = Path(temporary) / "build"
    subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Unix Makefiles", f"-DSOURCE_ROOT={root}", "-DIOS=ON"], check=True, stdout=subprocess.DEVNULL)
    graph = (build / "CMakeFiles" / "Makefile2").read_text()
    assert "CMakeFiles/unit_tests.dir/all: CMakeFiles/check_difficulty_target_call_sites.dir/all" in graph
