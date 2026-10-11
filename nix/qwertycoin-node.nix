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
      home = cfg.effectiveDataDir;
      createHome = true;
      homeMode = "0700";
    };
    # Create state before systemd builds the service's read-only namespace.
    systemd.tmpfiles.settings.qwertycoin-node = lib.genAttrs cfg.managedDirectories (_: {
      d = {
        mode = "0700";
        user = cfg.user;
        group = cfg.group;
      };
    });
    systemd.services.qwertycoin-node = {
      description = "Qwertycoin node";
      wantedBy = [ "multi-user.target" ];
      wants = [ "network-online.target" ];
      unitConfig.RequiresMountsFor =
        cfg.managedDirectories ++ lib.optional cfg.epose.enable (dirOf cfg.epose.keystore);
      after = [
        "network-online.target"
        "systemd-tmpfiles-setup.service"
      ];
      preStart = prepare;
      serviceConfig = {
        Type = "simple";
        User = cfg.user;
        Group = cfg.group;
        WorkingDirectory = cfg.effectiveDataDir;
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
          [ cfg.effectiveDataDir ]
          ++ lib.optional (
            cfg.epose.enable
            && dirOf cfg.epose.keystore != cfg.effectiveDataDir
            && !lib.hasPrefix "${cfg.effectiveDataDir}/" (dirOf cfg.epose.keystore)
          ) (dirOf cfg.epose.keystore)
          ++ cfg.extraWritablePaths
        );
      };
    };
  };
}
