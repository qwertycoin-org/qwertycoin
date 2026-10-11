{ cfg, lib }:
let
  # Render booleans as switches, lists as repeated options, and values as one
  # argv entry. Both service backends consume this exact argv list.
  render =
    name: value:
    if value == null || value == false then
      [ ]
    else if value == true then
      [ "--${name}" ]
    else if builtins.isList value then
      lib.concatMap (item: render name item) value
    else
      [ "--${name}=${toString value}" ];
  rl = cfg.rpc.restrictedListener;
  options = {
    non-interactive = true;
    data-dir = cfg.dataDir;
    log-file = cfg.logging.file;
    log-level = cfg.logging.level;
    max-log-file-size = cfg.logging.maxFileSize;
    max-log-files = cfg.logging.maxFiles;
    config-file = cfg.configFile;
    testnet = cfg.network == "testnet";
    stagenet = cfg.network == "stagenet";
    regtest = cfg.network == "regtest";
    p2p-bind-ip = cfg.p2p.address;
    p2p-bind-port = cfg.p2p.port;
    p2p-use-ipv6 = cfg.p2p.ipv6.enable;
    p2p-bind-ipv6-address = if cfg.p2p.ipv6.enable then cfg.p2p.ipv6.address else null;
    p2p-bind-port-ipv6 = if cfg.p2p.ipv6.enable then cfg.p2p.ipv6.port else null;
    p2p-ignore-ipv4 = cfg.p2p.ipv6.ignoreIPv4;
    p2p-external-port = cfg.p2p.externalPort;
    add-peer = cfg.p2p.peers;
    add-priority-node = cfg.p2p.priorityNodes;
    add-exclusive-node = cfg.p2p.exclusiveNodes;
    seed-node = cfg.p2p.seedNodes;
    ban-list = cfg.p2p.banList;
    hide-my-port = cfg.p2p.hidePort;
    allow-local-ip = cfg.p2p.allowLocalIP;
    enable-dns-blocklist = cfg.p2p.dnsBlocklist;
    pad-transactions = cfg.p2p.padTransactions;
    no-igd = true;
    tx-proxy = cfg.p2p.txProxy;
    anonymous-inbound = cfg.p2p.anonymousInbound;
    rpc-bind-ip = cfg.rpc.address;
    rpc-bind-port = cfg.rpc.port;
    restricted-rpc = cfg.rpc.restricted;
    confirm-external-bind = cfg.rpc.confirmExternalBind;
    rpc-use-ipv6 = cfg.rpc.ipv6.enable;
    rpc-bind-ipv6-address = if cfg.rpc.ipv6.enable then cfg.rpc.ipv6.address else null;
    rpc-ignore-ipv4 = cfg.rpc.ipv6.ignoreIPv4;
    rpc-restricted-bind-port = if rl.enable then rl.port else null;
    rpc-restricted-bind-ip = if rl.enable then rl.address else null;
    rpc-restricted-bind-ipv6-address =
      if rl.enable && cfg.rpc.ipv6.enable then rl.ipv6Address else null;
    public-node = cfg.rpc.publicNode;
    rpc-access-control-origins =
      if cfg.rpc.accessControlOrigins == [ ] then
        null
      else
        lib.concatStringsSep "," cfg.rpc.accessControlOrigins;
    disable-rpc-ban = cfg.rpc.disableBan;
    rpc-ban-exempt-address = cfg.rpc.banExemptAddresses;
    rpc-ssl = cfg.rpc.tls.mode;
    rpc-ssl-private-key = cfg.rpc.tls.privateKey;
    rpc-ssl-certificate = cfg.rpc.tls.certificate;
    rpc-ssl-ca-certificates = cfg.rpc.tls.caCertificates;
    rpc-ssl-allowed-fingerprints = cfg.rpc.tls.allowedFingerprints;
    rpc-ssl-allow-chained = cfg.rpc.tls.allowChained;
    bootstrap-daemon-address = cfg.rpc.bootstrap.address;
    bootstrap-daemon-proxy = cfg.rpc.bootstrap.proxy;
    epose-v2-service = cfg.epose.enable;
    epose-v2-keystore = if cfg.epose.enable then cfg.epose.keystore else null;
    epose-v2-reward-address = if cfg.epose.enable then cfg.epose.rewardAddress else null;
    epose-v2-endpoint-host = if cfg.epose.enable then cfg.epose.endpoint.host else null;
    epose-v2-endpoint-port = if cfg.epose.enable then cfg.epose.endpoint.port else null;
    epose-v2-discovery-endpoint = if cfg.epose.enable then cfg.epose.discoveryEndpoints else [ ];
    epose-v2-repair-keystore-permissions = cfg.epose.enable && cfg.epose.repairKeystorePermissions;
    limit-rate-up = cfg.limits.upload;
    limit-rate-down = cfg.limits.download;
    out-peers = cfg.limits.outgoingPeers;
    in-peers = cfg.limits.incomingPeers;
    max-connections-per-ip = cfg.limits.connectionsPerIP;
    max-concurrency = cfg.limits.concurrency;
    max-txpool-weight = cfg.limits.txpoolWeight;
    rpc-max-connections = cfg.limits.rpcConnections;
    rpc-max-connections-per-public-ip = cfg.limits.rpcConnectionsPerPublicIP;
    rpc-max-connections-per-private-ip = cfg.limits.rpcConnectionsPerPrivateIP;
    rpc-response-soft-limit = cfg.limits.rpcResponseSoftLimit;
    prune-blockchain = cfg.prune;
    offline = cfg.sync.offline;
    no-sync = !cfg.sync.enable;
    sync-pruned-blocks = cfg.sync.prunedBlocks;
    block-sync-size = cfg.sync.blockSize;
    block-download-max-size = cfg.sync.downloadMaxSize;
    fast-block-sync = if cfg.sync.fast then 1 else 0;
    prep-blocks-threads = cfg.sync.preparationThreads;
    disable-dns-checkpoints = cfg.sync.disableDNSCheckpoints;
    enforce-dns-checkpointing = cfg.sync.enforceDNSCheckpoints;
    db-sync-mode = cfg.database.syncMode;
    db-salvage = cfg.database.salvage;
    keep-alt-blocks = cfg.database.keepAlternativeBlocks;
    proxy = cfg.proxy.address;
    proxy-allow-dns-leaks = cfg.proxy.allowDNSLeaks;
    check-updates = cfg.checkUpdates;
    no-zmq = !cfg.zmq.enable;
    zmq-rpc-bind-ip = if cfg.zmq.enable then cfg.zmq.address else null;
    zmq-rpc-bind-port = if cfg.zmq.enable then cfg.zmq.port else null;
    restricted-zmq-rpc = cfg.zmq.enable && cfg.zmq.restricted;
    confirm-zmq-rpc-external-bind = cfg.zmq.enable && cfg.zmq.confirmExternalBind;
    zmq-pub = cfg.zmq.publish;
    start-mining = if cfg.mining.enable then cfg.mining.address else null;
    mining-threads = if cfg.mining.enable then cfg.mining.threads else null;
    bg-mining-enable = cfg.mining.enable && cfg.mining.background;
    bg-mining-ignore-battery = cfg.mining.enable && cfg.mining.ignoreBattery;
  };
in
[ "${cfg.package}/bin/qwertycoind" ]
++ lib.concatLists (lib.mapAttrsToList render (cfg.settings // options))
++ cfg.extraArgs
