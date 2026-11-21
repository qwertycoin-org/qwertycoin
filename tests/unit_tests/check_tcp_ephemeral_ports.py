import argparse
import errno
import socket
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("unit_tests")
    args = parser.parse_args()
    binary = Path(args.unit_tests).resolve()
    listeners = []
    try:
        for port in (5262, 5263, 5626):
            listener = socket.socket()
            try:
                listener.bind(("127.0.0.1", port))
                listener.listen(1)
            except OSError as error:
                listener.close()
                if error.errno != errno.EADDRINUSE:
                    raise
            else:
                listeners.append(listener)

        try:
            result = subprocess.run(
                [str(binary), "--gtest_filter=boosted_tcp_server.worker_threads_are_exception_resistant:boosted_tcp_server.strand_deadlock:boosted_tcp_server.shutdown:test_epee_connection.test_lifetime:test_epee_connection.ssl_shutdown"],
                cwd=binary.parents[2],
                timeout=40,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
        except subprocess.TimeoutExpired:
            return 124
        print(result.stdout, end="")
        if result.returncode != 0:
            return result.returncode
        return 0 if "[  PASSED  ] 5 tests." in result.stdout.splitlines() else 1
    finally:
        for listener in listeners:
            listener.close()


if __name__ == "__main__":
    raise SystemExit(main())
