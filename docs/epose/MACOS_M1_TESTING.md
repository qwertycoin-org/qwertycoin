# Qwertycoin EPoSE M1 Testing

This runbook is for local Apple Silicon testing of PR #3 on an M1/M2/M3 Mac.
Prefer the native Homebrew build. Docker's `linux/amd64` emulation works as a
fallback for smoke tests, but it is much slower and does not represent native
RandomX performance.

## Fastest Path

```bash
git checkout feature/epose-hardening-v2
git pull --ff-only
git submodule update --init --force
brew update
brew install cmake boost hidapi openssl zmq libpgm unbound libunwind-headers protobuf ccache
```

Keep the local loop focused. The useful M1 feedback is whether native macOS
builds, EPoSE unit tests, fuzz seeds, and a daemon smoke work on Apple Silicon.
Large 10/25-node Docker harnesses, sanitizer matrix runs, and live 3-host
partition tests are still better executed on seed host A/seed host B where the environment
is already warmed and reproducible.

Configure the build once:

```bash
cmake -S . -B build/macos-arm64-epose \
  -DARCH=armv8-a \
  -DBUILD_64=ON \
  -DBUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DMANUAL_SUBMODULES=1 \
  -DUSE_DEVICE_TREZOR=ON
```

Start with the focused EPoSE test binaries. This keeps the first Apple Silicon
signal fast and avoids hiding daemon or wallet build errors in a large parallel
log:

```bash
cmake --build build/macos-arm64-epose \
  --parallel "$(sysctl -n hw.logicalcpu)" \
  --target epose_unit_tests epose_fuzz_tests
```

Run the focused tests:

```bash
./build/macos-arm64-epose/tests/unit_tests/epose_unit_tests

for seed in tests/data/fuzz/epose/*; do
  ./build/macos-arm64-epose/tests/fuzz/epose_fuzz_tests "$seed"
done
```

Run a basic local daemon smoke:

```bash
cmake --build build/macos-arm64-epose \
  --parallel "$(sysctl -n hw.logicalcpu)" \
  --target daemon

./build/macos-arm64-epose/bin/qwertycoind \
  --testnet \
  --offline \
  --fixed-difficulty 1 \
  --data-dir "$PWD/.qwc-m1-testnet" \
  --log-file "$PWD/.qwc-m1-testnet/qwertycoind.log"
```

In another terminal:

```bash
curl -s http://127.0.0.1:8197/get_epose_info
```

Stop the daemon before deleting the temporary data directory:

```bash
rm -rf .qwc-m1-testnet
```

If the daemon target fails, rerun it single-threaded with a full Make log and
inspect the actual compiler or linker error above the final `Error 2` summary:

```bash
cmake --build build/macos-arm64-epose \
  --target daemon \
  --parallel 1 \
  -- VERBOSE=1 2>&1 | tee build/macos-arm64-epose/daemon-build.log

tail -120 build/macos-arm64-epose/daemon-build.log
```

## Docker Fallback

Use this only when native Homebrew dependencies are not available. It builds an
amd64 Linux image under emulation, so it is expected to be slow on Apple
Silicon.

```bash
docker buildx build \
  --platform linux/amd64 \
  --build-arg NPROC=2 \
  --build-arg EPOSE_BUILD_TARGETS="epose_unit_tests epose_fuzz_tests daemon simplewallet" \
  -f Dockerfile.epose-dev \
  -t qwertycoin-v2-node:epose-m1-emulated \
  --load .

docker run --rm \
  --platform linux/amd64 \
  --entrypoint /usr/local/bin/qwertycoin-epose-unit-tests \
  qwertycoin-v2-node:epose-m1-emulated

docker run --rm \
  --platform linux/amd64 \
  --entrypoint /bin/sh \
  qwertycoin-v2-node:epose-m1-emulated \
  -c 'for seed in /usr/local/share/qwertycoin/tests/data/fuzz/epose/*; do /usr/local/bin/qwertycoin-epose-fuzz-tests "$seed" || exit 1; done'
```

## What To Report

Record the following with any M1 result:

```text
Mac model
macOS version
Xcode command line tools version
Homebrew prefix
compiler version
build command
EPoSE unit-test result
fuzz seed result
daemon smoke result
any crash log or failing command
```

Do not use local M1 timings as mainnet admission-difficulty data unless the
native RandomX benchmark target has also been built and run on the same host.
