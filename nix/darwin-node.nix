{ self }:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.services.qwertycoin-node;
  opt =
    default: description:
    lib.mkOption {
      type = lib.types.ints.positive;
      inherit default description;
    };
  prepare = lib.concatMapStringsSep "\n" (dir: ''
    /usr/bin/install -d -m 0700 -o ${lib.escapeShellArg cfg.user} -g ${lib.escapeShellArg cfg.group} -- ${lib.escapeShellArg dir}
  '') cfg.managedDirectories;
  allowInbound =
    cfg.openFirewall || (cfg.rpc.restrictedListener.enable && cfg.rpc.restrictedListener.openFirewall);
in
{
  imports = [ (import ./node-options.nix { inherit self; }) ];
  options.services.qwertycoin-node.darwin = {
    uid = opt 398 "UID of the managed macOS service account. Choose an unused ID on the host.";
    gid = opt 398 "GID of the managed macOS service group. Choose an unused ID on the host.";
  };
  config = lib.mkIf cfg.enable {
    users.knownUsers = [ cfg.user ];
    users.knownGroups = [ cfg.group ];
    users.groups.${cfg.group} = {
      gid = cfg.darwin.gid;
    };
    users.users.${cfg.user} = {
      uid = cfg.darwin.uid;
      gid = cfg.darwin.gid;
      description = "Qwertycoin node service";
      # Keep the directory-service record stable when the chain is moved.
      home = "/var/empty";
      createHome = false;
      isHidden = true;
    };
    # Accounts are created before this activation stage. Prepare state before
    # launchd loads the service, including the separate persistent EPoSe identity.
    system.activationScripts.launchd.text = lib.mkBefore prepare;
    system.activationScripts.postActivation.text = lib.mkIf allowInbound ''
      /usr/libexec/ApplicationFirewall/socketfilterfw --add ${lib.escapeShellArg "${cfg.package}/bin/qwertycoind"}
      /usr/libexec/ApplicationFirewall/socketfilterfw --unblockapp ${lib.escapeShellArg "${cfg.package}/bin/qwertycoind"}
    '';
    launchd.daemons.qwertycoin-node = {
      environment.HOME = cfg.effectiveDataDir;
      # nix-darwin wraps command with wait4path so the Nix volume is available
      # during boot. Shell quoting preserves literal arguments, including $/%.
      command = lib.escapeShellArgs cfg.daemonArguments;
      serviceConfig = {
        Label = "org.qwertycoin.node";
        UserName = cfg.user;
        GroupName = cfg.group;
        WorkingDirectory = cfg.effectiveDataDir;
        RunAtLoad = true;
        KeepAlive = {
          SuccessfulExit = false;
        };
        ThrottleInterval = cfg.restartDelay;
        ExitTimeOut = cfg.stopTimeout;
        Umask = 63;
        SoftResourceLimits.NumberOfFiles = cfg.limitNOFILE;
        StandardOutPath = "${cfg.effectiveDataDir}/launchd.stdout.log";
        StandardErrorPath = "${cfg.effectiveDataDir}/launchd.stderr.log";
      };
    };
  };
}
