{
  self,
  nixpkgs,
  nix-darwin,
  system,
}:
let
  inherit (nixpkgs) lib;
  fixture = import ./check-service-options.nix { inherit lib; };
  evaluate =
    settings:
    (nix-darwin.lib.darwinSystem {
      inherit system;
      modules = [
        self.darwinModules.default
        {
          nix.enable = false;
          system.stateVersion = 6;
        }
        settings
      ];
    }).config;
  disabled = evaluate { };
  enabled = evaluate { services.qwertycoin-node.enable = true; };
  full = evaluate { services.qwertycoin-node = fixture.settings; };
  optionalServices = evaluate { services.qwertycoin-node = fixture.optionalSettings; };
  linux =
    (nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        self.nixosModules.default
        {
          boot.isContainer = true;
          system.stateVersion = "25.11";
          services.qwertycoin-node = fixture.settings;
        }
      ];
    }).config;
  cfg = full.services.qwertycoin-node;
  service = full.launchd.daemons.qwertycoin-node.serviceConfig;
  # Compare arguments with a common executable while retaining every setting.
  argv = configuration: lib.tail configuration.services.qwertycoin-node.daemonArguments;
in
assert !(disabled.launchd.daemons ? qwertycoin-node);
assert !(disabled.users.users ? qwertycoin);
assert lib.all (entry: entry.assertion) enabled.assertions;
assert lib.all (entry: entry.assertion) full.assertions;
assert fixture.check cfg;
assert fixture.checkOptional optionalServices.services.qwertycoin-node;
assert lib.all (entry: entry.assertion) optionalServices.assertions;
assert full.users.users.qwertycoin.home == "/var/empty";
assert service.EnvironmentVariables.HOME == cfg.dataDir;
assert argv full == argv linux;
assert service.Label == "org.qwertycoin.node";
assert service.UserName == "qwertycoin" && service.GroupName == "qwertycoin";
assert service.WorkingDirectory == "/var/lib/qwertycoin";
assert service.RunAtLoad && !service.KeepAlive.SuccessfulExit;
assert service.Umask == 63;
assert service.ExitTimeOut == 120 && service.ThrottleInterval == 10;
assert service.StandardErrorPath == "/var/lib/qwertycoin/launchd.stderr.log";
assert builtins.elem "qwertycoin" full.users.knownUsers;
assert full.users.users.qwertycoin.uid == cfg.darwin.uid;
assert lib.hasInfix "/usr/bin/install -d -m 0700" full.system.activationScripts.launchd.text;
assert lib.hasInfix "/var/lib/qwertycoin/epose" full.system.activationScripts.launchd.text;
assert lib.hasInfix "--unblockapp" full.system.activationScripts.postActivation.text;
assert !(lib.hasInfix "--unblockapp" enabled.system.activationScripts.postActivation.text);
assert lib.hasInfix "wait4path /nix/store" (lib.last service.ProgramArguments);
assert lib.hasInfix "notify $literal %s'" full.launchd.daemons.qwertycoin-node.command;
true
