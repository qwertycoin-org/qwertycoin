{ self }:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.services.qwertycoin-node;
  inherit (lib) types;
  opt =
    type: default: description:
    lib.mkOption { inherit type default description; };
  nullable = type: description: opt (types.nullOr type) null description;
  strings = description: opt (types.listOf types.str) [ ] description;
  automaticLimit = types.addCheck types.int (value: value >= -1);
  flag = description: opt types.bool false description;
  absolute = value: lib.hasPrefix "/" value;
  restricted = cfg.rpc.restrictedListener;
in
{
  options.services.qwertycoin-node = {
    enable = lib.mkEnableOption "the Qwertycoin node";
    package = lib.mkOption {
      type = types.package;
      default = pkgs.qwertycoin or self.packages.${pkgs.stdenv.hostPlatform.system}.qwertycoin;
      defaultText = lib.literalExpression "qwertycoin.packages.\${pkgs.stdenv.hostPlatform.system}.qwertycoin";
      description = "Package providing qwertycoind.";
    };
    user = opt types.str "qwertycoin" "Dedicated service account name.";
    group = opt types.str "qwertycoin" "Dedicated service group name.";
    dataDir =
      opt types.str "/var/lib/qwertycoin"
        "Base directory passed to the daemon. The daemon appends testnet, stagenet, or fake for non-mainnet networks, including for custom paths.";
    network = opt (types.enum [
      "mainnet"
      "testnet"
      "stagenet"
      "regtest"
    ]) "mainnet" "Network to join.";
    keepFakechain =
      opt types.bool (cfg.network == "regtest")
        "Preserve the regtest blockchain across restarts. Disabling this deletes the regtest database at every daemon startup; other networks are unaffected.";
    openFirewall = flag "Allow inbound P2P traffic: open TCP ports on NixOS, or allow the daemon application through the macOS application firewall.";
    configFile = nullable types.str "Runtime configuration file readable by the service account. Store credentials here, outside the Nix store.";
    extraArgs = strings "Additional daemon arguments, passed literally after the generated settings.";
    settings =
      opt
        (types.attrsOf (
          types.nullOr (
            types.oneOf [
              types.bool
              types.int
              types.str
              (types.listOf (types.either types.str types.int))
            ]
          )
        ))
        { }
        "Other daemon options, using their CLI names without --. True emits a switch, false/null omit it, and lists repeat the option. Typed options take precedence. Do not put secrets here.";
    extraWritablePaths = strings "Additional directories the service may write to. Ownership and existence are managed by the administrator.";
    restartDelay = opt types.ints.positive 10 "Seconds between restart attempts.";
    stopTimeout = opt types.ints.positive 120 "Seconds allowed for a clean shutdown.";
    limitNOFILE = opt types.ints.positive 65536 "Service file descriptor limit.";

    p2p = {
      address = opt types.str "0.0.0.0" "P2P IPv4 bind address.";
      port = opt types.port 8196 "P2P IPv4 TCP port.";
      ipv6.enable = flag "Enable P2P IPv6.";
      ipv6.address = opt types.str "::" "P2P IPv6 bind address.";
      ipv6.port = opt types.port cfg.p2p.port "P2P IPv6 TCP port.";
      ipv6.ignoreIPv4 = flag "Permit IPv4 binding to fail when P2P IPv6 is enabled.";
      externalPort = nullable types.port "Advertised P2P port when using NAT.";
      peers = strings "Peers to add to the peer list (host:port).";
      priorityNodes = strings "Peers to keep connected (host:port).";
      exclusiveNodes = strings "Connect only to these peers (host:port).";
      seedNodes = strings "Seed peers used to retrieve peer addresses (host:port).";
      banList = nullable types.str "Runtime IP ban-list file.";
      hidePort = flag "Do not announce this node as a peer-list candidate.";
      allowLocalIP = flag "Allow local addresses in the peer list.";
      dnsBlocklist = flag "Enable the DNS peer blocklist.";
      padTransactions = flag "Pad relayed transactions.";
      txProxy = strings "Transaction proxy specifications, for example tor,127.0.0.1:9050,100.";
      anonymousInbound = strings "Anonymous inbound endpoint specifications.";
    };

    rpc = {
      address = opt types.str "127.0.0.1" "Administrative RPC IPv4 bind address.";
      port = opt types.port 8197 "Administrative RPC TCP port.";
      restricted = flag "Restrict commands on the primary RPC listener.";
      confirmExternalBind = opt types.bool cfg.epose.enable "Confirm non-loopback RPC bindings.";
      ipv6.enable = flag "Enable IPv6 on RPC listeners.";
      ipv6.address = opt types.str "::1" "Administrative RPC IPv6 bind address.";
      ipv6.ignoreIPv4 = flag "Permit IPv4 binding to fail when RPC IPv6 is enabled.";
      restrictedListener = {
        enable =
          opt types.bool cfg.epose.enable
            "Enable a separate restricted RPC listener (including EPoSe probes).";
        address = opt types.str (
          if cfg.epose.enable then "0.0.0.0" else "127.0.0.1"
        ) "Restricted RPC IPv4 bind address.";
        ipv6Address = opt types.str (
          if cfg.epose.enable then "::" else "::1"
        ) "Restricted RPC IPv6 bind address.";
        port = opt types.port 8198 "Restricted RPC TCP port.";
        openFirewall = opt types.bool (
          cfg.openFirewall && cfg.epose.enable
        ) "Allow inbound restricted RPC traffic. Administrative RPC is never opened automatically.";
      };
      publicNode = flag "Advertise restricted RPC availability over P2P.";
      accessControlOrigins = strings "Allowed RPC CORS origins.";
      disableBan = flag "Disable RPC failure bans.";
      banExemptAddresses = strings "Trusted proxy addresses exempt from RPC failure bans.";
      tls = {
        mode = opt (types.enum [
          "enabled"
          "disabled"
          "autodetect"
        ]) "autodetect" "RPC TLS mode.";
        privateKey = nullable types.str "Runtime PEM private-key file, outside the Nix store.";
        certificate = nullable types.str "Runtime PEM certificate file.";
        caCertificates = nullable types.str "Runtime PEM CA certificate bundle.";
        allowedFingerprints = strings "Allowed RPC TLS certificate fingerprints.";
        allowChained = flag "Allow chained certificates.";
      };
      bootstrap.address = nullable types.str "Bootstrap daemon URL, or auto.";
      bootstrap.proxy = nullable types.str "Bootstrap daemon SOCKS proxy (host:port).";
    };

    epose = {
      enable = lib.mkEnableOption "the EPoSe v2 service producer";
      keystore =
        opt types.str "/var/lib/qwertycoin-epose/keystore"
          "Persistent EPoSe v2 identity keystore, outside the Nix store. The daemon creates it with protected permissions.";
      rewardAddress =
        opt types.str ""
          "Primary public QWC reward address; no wallet private key is used.";
      endpoint.host =
        opt types.str ""
          "Canonical public IPv4, IPv6, or lowercase DNS host serving EPoSe probes.";
      endpoint.port =
        opt types.port restricted.port
          "Public restricted RPC probe port, including an external NAT port if applicable.";
      discoveryEndpoints = strings "Public http://host:port discovery endpoints (use brackets around IPv6 hosts).";
      repairKeystorePermissions = flag "One-time recovery of the keystore file permissions; disable after recovery.";
    };

    logging = {
      file = opt types.str "${cfg.effectiveDataDir}/qwertycoind.log" "Daemon log file.";
      level = nullable types.str "Log level or category filter.";
      maxFileSize = nullable types.ints.unsigned "Maximum log file size in bytes.";
      maxFiles = nullable types.ints.unsigned "Maximum rotated log file count.";
    };
    limits = {
      upload = nullable automaticLimit "Upload limit in KiB/s (-1 restores the daemon default).";
      download = nullable automaticLimit "Download limit in KiB/s (-1 restores the daemon default).";
      outgoingPeers = nullable automaticLimit "Maximum outbound peer count (-1 uses the daemon default).";
      incomingPeers = nullable automaticLimit "Maximum inbound peer count (-1 uses the daemon default).";
      connectionsPerIP = nullable types.ints.positive "Maximum P2P connections per IP.";
      concurrency = nullable types.ints.unsigned "Maximum worker threads (0 means automatic).";
      txpoolWeight = nullable types.ints.unsigned "Maximum transaction pool weight in bytes.";
      rpcConnections = nullable types.ints.positive "Maximum RPC connection count.";
      rpcConnectionsPerPublicIP = nullable types.ints.positive "Maximum RPC connections per public IP.";
      rpcConnectionsPerPrivateIP = nullable types.ints.positive "Maximum RPC connections per private IP.";
      rpcResponseSoftLimit = nullable types.ints.unsigned "RPC response queue soft limit in bytes.";
    };
    prune = flag "Prune the blockchain (incompatible with EPoSe service production).";
    sync = {
      offline = flag "Disable all peer communication.";
      enable = opt types.bool true "Synchronize the blockchain with peers.";
      prunedBlocks = flag "Allow synchronization from pruned peers.";
      blockSize = nullable types.ints.unsigned "Blocks synchronized per batch (0 means adaptive).";
      downloadMaxSize = nullable types.ints.unsigned "Block download queue size in bytes (0 means default).";
      fast = opt types.bool true "Enable fast block synchronization.";
      preparationThreads = nullable types.ints.unsigned "Block preparation worker count.";
      disableDNSCheckpoints = flag "Disable DNS checkpoint retrieval.";
      enforceDNSCheckpoints = flag "Enforce DNS checkpoints.";
    };
    database.syncMode = nullable types.str "LMDB sync mode, for example safe:sync.";
    database.salvage = flag "Attempt database salvage.";
    database.keepAlternativeBlocks = flag "Retain alternative blocks across restarts.";
    proxy.address = nullable types.str "Outbound SOCKS proxy (host:port).";
    proxy.allowDNSLeaks = flag "Allow DNS requests outside the proxy.";
    checkUpdates = opt (types.enum [
      "disabled"
      "notify"
    ]) "disabled" "Update check policy. Package updates are managed through Nix.";
    zmq = {
      enable = flag "Enable ZMQ RPC.";
      address = opt types.str "127.0.0.1" "ZMQ RPC bind address.";
      port = opt types.port 8199 "ZMQ RPC TCP port.";
      restricted = flag "Restrict sensitive ZMQ RPC methods.";
      confirmExternalBind = flag "Confirm external ZMQ RPC bindings.";
      publish = strings "ZMQ publication endpoints (tcp:// or ipc://).";
    };
    mining = {
      enable = lib.mkEnableOption "mining";
      address = opt types.str "" "Public mining reward address.";
      threads = opt types.ints.unsigned 0 "Mining thread count (0 means automatic).";
      background = flag "Enable background mining.";
      ignoreBattery = flag "Allow background mining when AC power cannot be detected.";
    };
    daemonArguments = lib.mkOption {
      type = types.listOf types.str;
      internal = true;
      readOnly = true;
      description = "Shared daemon argv, including the executable.";
    };
    effectiveDataDir = lib.mkOption {
      type = types.str;
      internal = true;
      readOnly = true;
      default =
        cfg.dataDir
        + {
          mainnet = "";
          testnet = "/testnet";
          stagenet = "/stagenet";
          regtest = "/fake";
        }
        .${cfg.network};
      description = "Data directory after the daemon applies its network suffix.";
    };
    managedDirectories = lib.mkOption {
      type = types.listOf types.str;
      internal = true;
      readOnly = true;
      description = "Directories created and owned by the service.";
    };
  };

  config = lib.mkIf cfg.enable {
    services.qwertycoin-node.daemonArguments = import ./node-arguments.nix { inherit cfg lib; };
    services.qwertycoin-node.managedDirectories = [
      cfg.effectiveDataDir
    ]
    ++ lib.optional (
      cfg.epose.enable && dirOf cfg.epose.keystore == "/var/lib/qwertycoin-epose"
    ) "/var/lib/qwertycoin-epose";
    environment.systemPackages = [ cfg.package ];
    assertions = [
      {
        assertion = absolute cfg.dataDir && cfg.dataDir != "/";
        message = "qwertycoin-node.dataDir must be an absolute non-root directory.";
      }
      {
        assertion = !cfg.p2p.ipv6.ignoreIPv4 || cfg.p2p.ipv6.enable;
        message = "p2p.ipv6.ignoreIPv4 requires p2p.ipv6.enable.";
      }
      {
        assertion = !cfg.rpc.ipv6.ignoreIPv4 || cfg.rpc.ipv6.enable;
        message = "rpc.ipv6.ignoreIPv4 requires rpc.ipv6.enable.";
      }
      {
        assertion = !restricted.enable || restricted.port != cfg.rpc.port;
        message = "The administrative and restricted RPC ports must differ.";
      }
      {
        assertion =
          !cfg.epose.enable
          || (!cfg.prune && !cfg.sync.offline && cfg.sync.enable && cfg.network == "mainnet");
        message = "EPoSe v2 requires an online, synchronized, unpruned mainnet node.";
      }
      {
        assertion =
          !cfg.epose.enable
          || (
            cfg.epose.rewardAddress != ""
            && cfg.epose.endpoint.host != ""
            && cfg.epose.discoveryEndpoints != [ ]
          );
        message = "EPoSe v2 requires rewardAddress, endpoint.host, and discoveryEndpoints.";
      }
      {
        assertion =
          !cfg.epose.enable
          || (absolute cfg.epose.keystore && !lib.hasPrefix "/nix/store/" cfg.epose.keystore);
        message = "The EPoSe keystore must be an absolute runtime path outside the Nix store.";
      }
      {
        assertion = !cfg.epose.enable || restricted.enable;
        message = "EPoSe v2 requires the separate restricted RPC listener.";
      }
      {
        assertion = !cfg.epose.enable || cfg.rpc.confirmExternalBind;
        message = "EPoSe v2 requires rpc.confirmExternalBind for the public probe listener.";
      }
      {
        assertion = !cfg.mining.enable || cfg.mining.address != "";
        message = "mining.enable requires a public mining.address.";
      }
      {
        assertion = cfg.zmq.enable || cfg.zmq.publish == [ ];
        message = "ZMQ publication endpoints require zmq.enable.";
      }
      {
        assertion = lib.all (name: builtins.match "[a-z0-9][a-z0-9-]*" name != null) (
          lib.attrNames cfg.settings
        );
        message = "settings keys must be daemon option names without --.";
      }
    ];
  };
}
