{ lib }:
let
  # One fixture drives both backends, so their generated argv must stay equal.
  settings = {
    enable = true;
    openFirewall = true;
    epose = {
      enable = true;
      rewardAddress = "QWC_PUBLIC_REWARD_ADDRESS";
      endpoint.host = "node.example.org";
      discoveryEndpoints = [
        "http://seed.example.org:8198"
        "http://[2001:db8::1]:8198"
      ];
    };
    p2p = {
      ipv6.enable = true;
      ipv6.port = 18196;
      peers = [
        "peer-a.example.org:8196"
        "[2001:db8::2]:8196"
      ];
      priorityNodes = [ "priority.example.org:8196" ];
    };
    rpc = {
      ipv6.enable = true;
      tls.mode = "disabled";
      banExemptAddresses = [ "127.0.0.2" ];
    };
    logging.level = "1";
    limits = {
      upload = 1024;
      concurrency = 0;
      rpcConnections = 42;
    };
    database.syncMode = "safe:sync";
    proxy.address = "127.0.0.1:9050";
    settings = {
      block-notify = "/srv/qwertycoin/notify $literal %s";
      show-time-stats = 1;
      keep-fakechain = false;
      test-drop-download = null;
      limit-rate-up = 99; # Typed options have precedence.
    };
    extraArgs = [ "--max-log-files=2" ];
  };
  optionalSettings = {
    enable = true;
    network = "stagenet";
    prune = true;
    sync = {
      prunedBlocks = true;
      fast = false;
    };
    zmq = {
      enable = true;
      restricted = true;
      publish = [ "tcp://127.0.0.1:8200" ];
    };
    mining = {
      enable = true;
      address = "QWC_PUBLIC_MINING_ADDRESS";
      threads = 2;
      background = true;
    };
    rpc.tls = {
      mode = "enabled";
      certificate = "/run/secrets/node.crt";
      privateKey = "/run/secrets/node.key";
    };
  };
  checkOptional =
    cfg:
    let
      has = arg: builtins.elem arg cfg.daemonArguments;
    in
    assert has "--stagenet" && has "--prune-blockchain" && has "--sync-pruned-blocks";
    assert has "--data-dir=/var/lib/qwertycoin";
    assert cfg.effectiveDataDir == "/var/lib/qwertycoin/stagenet";
    assert has "--fast-block-sync=0";
    assert !(has "--no-zmq");
    assert has "--zmq-rpc-bind-ip=127.0.0.1" && has "--restricted-zmq-rpc";
    assert has "--zmq-pub=tcp://127.0.0.1:8200";
    assert has "--start-mining=QWC_PUBLIC_MINING_ADDRESS" && has "--mining-threads=2";
    assert has "--bg-mining-enable";
    assert has "--rpc-ssl=enabled" && has "--rpc-ssl-private-key=/run/secrets/node.key";
    assert !(has "--epose-v2-service");
    true;
  check =
    cfg:
    let
      args = cfg.daemonArguments;
      has = arg: builtins.elem arg args;
    in
    assert has "--epose-v2-service";
    assert has "--epose-v2-keystore=/var/lib/qwertycoin-epose/keystore";
    assert has "--epose-v2-endpoint-port=8198";
    assert has "--rpc-bind-ip=127.0.0.1";
    assert has "--rpc-bind-ipv6-address=::1";
    assert has "--rpc-restricted-bind-ip=0.0.0.0";
    assert has "--rpc-restricted-bind-ipv6-address=::";
    assert has "--rpc-restricted-bind-port=8198";
    assert has "--p2p-use-ipv6" && has "--rpc-use-ipv6";
    assert has "--p2p-bind-ipv6-address=::";
    assert has "--p2p-bind-port-ipv6=18196";
    assert has "--add-peer=peer-a.example.org:8196" && has "--add-peer=[2001:db8::2]:8196";
    assert has "--add-priority-node=priority.example.org:8196";
    assert has "--epose-v2-discovery-endpoint=http://seed.example.org:8198";
    assert has "--epose-v2-discovery-endpoint=http://[2001:db8::1]:8198";
    assert has "--limit-rate-up=1024" && !(has "--limit-rate-up=99");
    assert has "--max-concurrency=0";
    assert has "--rpc-max-connections=42";
    assert has "--rpc-ssl=disabled";
    assert has "--rpc-ban-exempt-address=127.0.0.2";
    assert has "--db-sync-mode=safe:sync";
    assert has "--proxy=127.0.0.1:9050";
    assert has "--block-notify=/srv/qwertycoin/notify $literal %s";
    assert has "--show-time-stats=1";
    assert !(has "--keep-fakechain") && !(has "--test-drop-download");
    assert lib.last args == "--max-log-files=2";
    true;
in
{
  inherit
    settings
    check
    optionalSettings
    checkOptional
    ;
}
