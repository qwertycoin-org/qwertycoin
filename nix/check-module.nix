{
  self,
  nixpkgs,
  system,
}:
let
  inherit (nixpkgs) lib;
  fixture = import ./check-service-options.nix { inherit lib; };
  pkgs = import nixpkgs { inherit system; };
  overlayPkgs = import nixpkgs {
    inherit system;
    overlays = [ self.overlays.default ];
  };
  evaluate =
    settings:
    (lib.nixosSystem {
      inherit system;
      modules = [
        self.nixosModules.default
        {
          boot.isContainer = true;
          system.stateVersion = "25.11";
        }
        settings
      ];
    }).config;

  disabled = evaluate { };
  complete = evaluate { services.qwertycoin-node = fixture.settings; };
  optionalServices = evaluate { services.qwertycoin-node = fixture.optionalSettings; };
  invalidEPoSe = evaluate {
    services.qwertycoin-node = fixture.settings // {
      prune = true;
    };
  };
  invalidIPv6 = evaluate {
    services.qwertycoin-node = {
      enable = true;
      p2p.ipv6.ignoreIPv4 = true;
    };
  };
  enabled = evaluate {
    nixpkgs.overlays = [ self.overlays.default ];
    services.qwertycoin-node = {
      enable = true;
      openFirewall = true;
    };
  };
  custom = evaluate {
    services.qwertycoin-node = {
      enable = true;
      dataDir = "/srv/qwertycoin";
      network = "testnet";
      p2p.port = 18196;
      rpc.port = 18197;
      rpc.restricted = true;
      prune = true;
      configFile = "/run/secrets/qwertycoin.conf";
      extraArgs = [
        "--log-level=1"
        "--ban-list=/srv/qwertycoin/ban $list%.txt"
      ];
    };
  };
  overridden = evaluate {
    services.qwertycoin-node = {
      enable = true;
      package = pkgs.hello;
    };
  };
  invalid = evaluate {
    services.qwertycoin-node = {
      enable = true;
      dataDir = "relative";
    };
  };
  service = enabled.systemd.services.qwertycoin-node;
  command = service.serviceConfig.ExecStart;
  customCommand = custom.systemd.services.qwertycoin-node.serviceConfig.ExecStart;
in
assert !(disabled.systemd.services ? qwertycoin-node);
assert !(disabled.users.users ? qwertycoin);
assert fixture.check complete.services.qwertycoin-node;
assert fixture.checkOptional optionalServices.services.qwertycoin-node;
assert lib.all (entry: entry.assertion) optionalServices.assertions;
assert lib.all (entry: entry.assertion) complete.assertions;
assert !(lib.all (entry: entry.assertion) invalidEPoSe.assertions);
assert !(lib.all (entry: entry.assertion) invalidIPv6.assertions);
assert builtins.elem 18196 complete.networking.firewall.allowedTCPPorts;
assert builtins.elem 8198 complete.networking.firewall.allowedTCPPorts;
assert !(builtins.elem 8197 complete.networking.firewall.allowedTCPPorts);
assert
  complete.systemd.services.qwertycoin-node.serviceConfig.ReadWritePaths == [ "/var/lib/qwertycoin" ];
assert lib.hasInfix "notify $$literal %%s"
  complete.systemd.services.qwertycoin-node.serviceConfig.ExecStart;
assert lib.all (entry: entry.assertion) enabled.assertions;
assert lib.all (entry: entry.assertion) custom.assertions;
assert enabled.services.qwertycoin-node.package.outPath == overlayPkgs.qwertycoin.outPath;
assert overlayPkgs.qwertycoin.outPath == self.packages.${system}.qwertycoin.outPath;
assert lib.hasInfix "--rpc-bind-ip=127.0.0.1" command;
assert lib.hasInfix "--p2p-bind-port=8196" command;
assert lib.hasInfix "--non-interactive" command;
assert lib.hasInfix "--no-zmq" command;
assert !(lib.hasInfix "--testnet" command);
assert builtins.elem 8196 enabled.networking.firewall.allowedTCPPorts;
assert !(builtins.elem 8197 enabled.networking.firewall.allowedTCPPorts);
assert service.serviceConfig.User == "qwertycoin";
assert service.serviceConfig.ReadWritePaths == [ "/var/lib/qwertycoin" ];
assert enabled.users.users.qwertycoin.homeMode == "0700";
assert builtins.elem enabled.services.qwertycoin-node.package enabled.environment.systemPackages;
assert custom.users.users.qwertycoin.home == "/srv/qwertycoin";
assert lib.hasInfix "--testnet" customCommand;
assert lib.hasInfix "--p2p-bind-port=18196" customCommand;
assert lib.hasInfix "--rpc-bind-port=18197" customCommand;
assert lib.hasInfix "--restricted-rpc" customCommand;
assert lib.hasInfix "--prune-blockchain" customCommand;
assert lib.hasInfix "--config-file=/run/secrets/qwertycoin.conf" customCommand;
assert lib.hasInfix "--log-level=1" customCommand;
assert lib.hasInfix "ban $$list%%.txt" customCommand;
assert !(builtins.elem 18196 custom.networking.firewall.allowedTCPPorts);
assert overridden.services.qwertycoin-node.package.outPath == pkgs.hello.outPath;
assert lib.hasInfix (builtins.unsafeDiscardStringContext "${pkgs.hello}/bin/qwertycoind")
  overridden.systemd.services.qwertycoin-node.serviceConfig.ExecStart;
assert !(lib.all (entry: entry.assertion) invalid.assertions);
true
