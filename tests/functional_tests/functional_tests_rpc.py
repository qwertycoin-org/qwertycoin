#!/usr/bin/env python

from __future__ import print_function
import sys
import time
import subprocess
from signal import SIGTERM
import socket
import string
import os
import shutil

USAGE = 'usage: functional_tests_rpc.py <python> <srcdir> <builddir> [<tests-to-run> | all]'
# RandomX keeps its dataset resident in the long-lived daemon. Run mining last
# so constrained CI hosts can still exercise every wallet/P2P group first.
DEFAULT_TESTS = ['address_book', 'bans', 'blockchain', 'cold_signing', 'contracts', 'contracts_names', 'daemon_info', 'get_output_distribution', 'integrated_address', 'multisig', 'p2p', 'proofs', 'rpc_payment', 'sign_message', 'transfer', 'txpool', 'uri', 'validate_address', 'wallet', 'mining']
NAMES_TESTS = ('contracts_names', 'contracts_names_release')
try:
  python = sys.argv[1]
  srcdir = sys.argv[2]
  builddir = sys.argv[3]
except:
  print(USAGE)
  sys.exit(1)

try:
  sys.argv[4]
except:
  print(USAGE)
  print('Available tests: ' + ', '.join(DEFAULT_TESTS + ['contracts_names_release']))
  print('Or run all with "all"')
  sys.exit(0)

try:
  tests = sys.argv[4:]
  if tests == ['all']:
    tests = DEFAULT_TESTS
except:
  tests = DEFAULT_TESTS
names_enabled = any(test in tests for test in NAMES_TESTS)

# RandomX retains a large dataset in the daemon even after mining stops. When
# the complete suite is requested, run mining in a fresh process phase so the
# wallet/P2P phase can release all daemon memory before RandomX initializes.
isolated = [test for test in NAMES_TESTS + ('mining',) if test in tests]
if isolated and len(tests) > 1:
  phases = [[test for test in tests if test not in isolated]] + [[test] for test in isolated]
  for phase in phases:
    if not phase:
      continue
    command = [sys.executable, os.path.abspath(__file__), python, srcdir, builddir] + phase
    if subprocess.call(command) != 0:
      sys.exit(1)
  print('Done, ' + str(len(tests)) + '/' + str(len(tests)) + ' tests passed in isolated phases')
  sys.exit(0)

if names_enabled:
  try:
    outputs = subprocess.check_output([srcdir + '/prepare_names_sdk.sh'], text=True).splitlines()
    os.environ['QWC_NAMES_WASM_DIR'], os.environ['QWC_NAMES_BIN'] = outputs
  except Exception as error:
    print('Failed to prepare names SDK: ' + str(error))
    sys.exit(1)

if names_enabled:
  sys.path.insert(0, srcdir + '/../../utils/python-rpc')
  from contracts_common import DAEMON_INDEX, MINER_ADDRESS, SERVICE_NODE_INDEXES

# a main offline qwertycoind, does most of the tests
# a restricted RPC qwertycoind setup with RPC payment
# two local online qwertycoind instances connected to each other
# an offline qwertycoind with the contracts hard fork active from height 2
N_MONERODS = SERVICE_NODE_INDEXES.stop if names_enabled else 5

# 4 wallets connected to the main offline qwertycoind
# 1 wallet connected to the first local online qwertycoind
# 1 offline wallet
WALLET_DIRECTORY = builddir + "/functional-tests-directory"
FUNCTIONAL_TESTS_DIRECTORY = builddir + "/tests/functional_tests"
DIFFICULTY = 10
os.makedirs(WALLET_DIRECTORY, exist_ok=True)
os.makedirs(FUNCTIONAL_TESTS_DIRECTORY, exist_ok=True)

qwertycoind_base = [builddir + "/bin/qwertycoind", "--regtest", "--fixed-difficulty", str(DIFFICULTY), "--p2p-bind-port", "qwertycoind_p2p_port", "--rpc-bind-port", "qwertycoind_rpc_port", "--zmq-rpc-bind-port", "qwertycoind_zmq_port", "--zmq-pub", "qwertycoind_zmq_pub", "--non-interactive", "--disable-dns-checkpoints", "--check-updates", "disabled", "--rpc-ssl", "disabled", "--data-dir", "qwertycoind_data_dir", "--log-level", "1", "--rpc-max-connections-per-private-ip", "100", "--rpc-max-connections", "100"]
RPC_PAYMENT_ADDRESS = "QWC1h8SDGxBj74KgQDGKRKAjTfWJywnvM1ZRGv9yX9o89N8qGedHKLheKPmtkLpTmpBWGDT3ZuLUmNVvz3LmU5LrA3gGed58sq"
qwertycoind_extra = [
  ["--offline"],
  ["--rpc-payment-address", RPC_PAYMENT_ADDRESS, "--rpc-payment-difficulty", str(DIFFICULTY), "--rpc-payment-credits", "5000", "--offline"],
  ["--add-exclusive-node", "127.0.0.1:18283"],
  ["--add-exclusive-node", "127.0.0.1:18282"],
  ["--offline", "--regtest-fork-height", "2"],
]
if names_enabled:
  discovery = [item for i in SERVICE_NODE_INDEXES
               for item in ("--epose-v2-discovery-endpoint", "http://127.0.0.1:" + str(18180 + i))]
  for i in SERVICE_NODE_INDEXES:
    data_dir = WALLET_DIRECTORY + '/qwertycoind' + str(i)
    if os.path.exists(data_dir):
      shutil.rmtree(data_dir)
    keystore_dir = WALLET_DIRECTORY + '/epose' + str(i)
    if os.path.exists(keystore_dir):
      shutil.rmtree(keystore_dir)
    os.makedirs(keystore_dir, mode=0o700, exist_ok=True)
    os.chmod(keystore_dir, 0o700)
    qwertycoind_extra[i:i + 1] = [
      (["--regtest-fork-height", "2", "--regtest-epose-v2", "--epose-v2-service",
        "--max-connections-per-ip", "20",
        "--epose-v2-keystore", keystore_dir + '/keystore',
        "--epose-v2-reward-address", MINER_ADDRESS,
        "--epose-v2-endpoint-host", "127.0.0.1",
        "--epose-v2-endpoint-port", str(18180 + i)] + discovery
       + (["--add-exclusive-node", "127.0.0.1:18285"] if i == 4 else
          ["--add-exclusive-node", "127.0.0.1:18284"]))]
if 'contracts' in tests:
  qwertycoind_extra[4] += ["--rpc-restricted-bind-port", "18584"]
wallet_base = [builddir + "/bin/qwertycoin-wallet-rpc", "--wallet-dir", WALLET_DIRECTORY, "--rpc-bind-port", "wallet_port", "--disable-rpc-login", "--rpc-ssl", "disabled", "--daemon-ssl", "disabled", "--log-level", "1", "--allow-mismatched-daemon-version"]
wallet_extra = [
  ["--daemon-port", "18180"],
  ["--daemon-port", "18180"],
  ["--daemon-port", "18180"],
  ["--daemon-port", "18180"],
  ["--daemon-port", "18182"],
  ["--offline"],
  ["--daemon-port", "18184", "--regtest"],
]

command_lines = []
processes = []
outputs = []
ports = []

for i in range(len(qwertycoind_extra)):
  command_lines.append([str(18180+i) if x == "qwertycoind_rpc_port" else str(18280+i) if x == "qwertycoind_p2p_port" else str(18380+i) if x == "qwertycoind_zmq_port" else "tcp://127.0.0.1:" + str(18480+i) if x == "qwertycoind_zmq_pub" else builddir + "/functional-tests-directory/qwertycoind" + str(i) if x == "qwertycoind_data_dir" else x for x in qwertycoind_base])
  command_lines[-1] += qwertycoind_extra[i]
  outputs.append(open(FUNCTIONAL_TESTS_DIRECTORY + '/qwertycoind' + str(i) + '.log', 'a+'))
  ports.append(18180+i)
  if i == 4 and 'contracts' in tests:
    ports.append(18584)

for i in range(len(wallet_extra)):
  command_lines.append([str(18090+i) if x == "wallet_port" else x for x in wallet_base])
  command_lines[-1] += wallet_extra[i]
  outputs.append(open(FUNCTIONAL_TESTS_DIRECTORY + '/wallet' + str(i) + '.log', 'a+'))
  ports.append(18090+i)

print('Starting servers...')
try:
  PYTHONPATH = os.environ['PYTHONPATH'] if 'PYTHONPATH' in os.environ else ''
  if len(PYTHONPATH) > 0:
    PYTHONPATH += ':'
  PYTHONPATH += srcdir + '/../../utils/python-rpc'
  os.environ['PYTHONPATH'] = PYTHONPATH
  os.environ['WALLET_DIRECTORY'] = WALLET_DIRECTORY
  os.environ['FUNCTIONAL_TESTS_DIRECTORY'] = FUNCTIONAL_TESTS_DIRECTORY
  os.environ['SOURCE_DIRECTORY'] = srcdir
  os.environ['PYTHONIOENCODING'] = 'utf-8'
  os.environ['DIFFICULTY'] = str(DIFFICULTY)
  os.environ['MAKE_TEST_SIGNATURE'] = FUNCTIONAL_TESTS_DIRECTORY + '/make_test_signature'
  os.environ['SEEDHASH_EPOCH_BLOCKS'] = "8"
  os.environ['SEEDHASH_EPOCH_LAG'] = "4"

  for i in range(len(command_lines)):
    #print('Running: ' + str(command_lines[i]))
    processes.append(subprocess.Popen(command_lines[i], stdout = outputs[i]))
except Exception as e:
  print('Error: ' + str(e))
  sys.exit(1)

def kill():
  for i in range(len(processes)):
    try: processes[i].send_signal(SIGTERM)
    except: pass

# wait for error/startup
for i in range(30 if names_enabled else 10):
  time.sleep(1)
  all_open = True
  for port in ports:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(1)
    if s.connect_ex(('127.0.0.1', port)) != 0:
      all_open = False
      break
    s.close()
  if all_open:
    break

if not all_open:
  print('Failed to start wallet or daemon')
  kill()
  sys.exit(1)

# online daemons need some time to connect to peers to be ready
time.sleep(2)

PASS = []
FAIL = []
for test in tests:
  try:
    print('[TEST STARTED] ' + test)
    sys.stdout.flush()
    if test == 'contracts_names_release':
      os.environ['QWC_NAMES_SERVICE_PIDS'] = ','.join(str(processes[i].pid) for i in SERVICE_NODE_INDEXES if i != DAEMON_INDEX)
    cmd = [python, srcdir + '/' + test + ".py"]
    subprocess.check_call(cmd)
    PASS.append(test)
    print('[TEST PASSED] ' + test)
  except:
    FAIL.append(test)
    print('[TEST FAILED] ' + test)
    pass

print('Stopping servers...')
kill()

# wait for exit, the poll method does not work (https://bugs.python.org/issue2475) so we wait, possibly forever if the process hangs
if True:
  for p in processes:
    p.wait()
else:
  for i in range(10):
    n_returncode = 0
    for p in processes:
      p.poll()
      if p.returncode:
        n_returncode += 1
    if n_returncode == len(processes):
      print('All done: ' + string.join([x.returncode for x in processes], ', '))
      break
    time.sleep(1)
  for p in processes:
    if not p.returncode:
      print('Failed to stop process')

if len(FAIL) == 0:
  print('Done, ' + str(len(PASS)) + '/' + str(len(tests)) + ' tests passed')
else:
  print('Done, ' + str(len(FAIL)) + '/' + str(len(tests)) + ' tests failed: ' + ', '.join(FAIL))

sys.exit(0 if len(FAIL) == 0 else 1)
