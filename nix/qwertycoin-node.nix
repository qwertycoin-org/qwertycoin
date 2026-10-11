{ self }:
{
  config,
  lib,
  pkgs,
  utils,
  ...
}:
let
  cfg = config.services.qwertycoin-node;
  restricted = cfg.rpc.restrictedListener;
  ports =
    lib.optionals cfg.openFirewall (
      [ cfg.p2p.port ] ++ lib.optional cfg.p2p.ipv6.enable cfg.p2p.ipv6.port
    )
    ++ lib.optional (restricted.enable && restricted.openFirewall) restricted.port;
  prepare = lib.concatMapStringsSep "\n" (dir: ''
    mkdir -p -- ${lib.escapeShellArg dir}
    chmod 0700 -- ${lib.escapeShellArg dir}
  '') cfg.managedDirectories;
in
{
  imports = [ (import ./node-options.nix { inherit self; }) ];
  config = lib.mkIf cfg.enable {
    networking.firewall.allowedTCPPorts = lib.unique ports;
    users.groups.${cfg.group} = { };
    users.users.${cfg.user} = {
      isSystemUser = true;
      group = cfg.group;
      home = cfg.dataDir;
      createHome = true;
      homeMode = "0700";
    };
    systemd.services.qwertycoin-node = {
      description = "Qwertycoin node";
      wantedBy = [ "multi-user.target" ];
      wants = [ "network-online.target" ];
      after = [ "network-online.target" ];
      unitConfig.RequiresMountsFor = [ cfg.dataDir ];
      preStart = prepare;
      serviceConfig = {
        Type = "simple";
        User = cfg.user;
        Group = cfg.group;
        WorkingDirectory = cfg.dataDir;
        ExecStart = utils.escapeSystemdExecArgs cfg.daemonArguments;
        Restart = "on-failure";
        RestartSec = cfg.restartDelay;
        TimeoutStopSec = cfg.stopTimeout;
        LimitNOFILE = cfg.limitNOFILE;
        UMask = "0077";
        NoNewPrivileges = true;
        PrivateTmp = true;
        ProtectHome = true;
        ProtectSystem = "strict";
        ReadWritePaths = lib.unique (
          [ cfg.dataDir ]
          # The default identity directory is created by preStart. Its parent
          # is already writable; systemd must not require the child to exist
          # while setting up the namespace before preStart runs.
          ++ lib.optional (
            cfg.epose.enable
            && dirOf cfg.epose.keystore != cfg.dataDir
            && !lib.hasPrefix "${cfg.dataDir}/" (dirOf cfg.epose.keystore)
          ) (dirOf cfg.epose.keystore)
          ++ cfg.extraWritablePaths
        );
      };
    };
  };
}
