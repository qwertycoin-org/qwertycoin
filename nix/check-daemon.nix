{
  self,
  nixpkgs,
  nix-darwin,
  system,
}:
let
  pkgs = import nixpkgs { inherit system; };
  inherit (pkgs) lib;
  evaluate =
    settings:
    (
      if pkgs.stdenv.hostPlatform.isLinux then
        nixpkgs.lib.nixosSystem {
          inherit system;
          modules = [
            self.nixosModules.default
            {
              boot.isContainer = true;
              system.stateVersion = "25.11";
              services.qwertycoin-node = settings;
            }
          ];
        }
      else
        nix-darwin.lib.darwinSystem {
          inherit system;
          modules = [
            self.darwinModules.default
            {
              nix.enable = false;
              system.stateVersion = 6;
              services.qwertycoin-node = settings;
            }
          ];
        }
    ).config.services.qwertycoin-node;
  fixture =
    network: custom:
    let
      cfg = evaluate (
        {
          enable = true;
          inherit network;
          sync.offline = true;
          rpc.tls.mode = "disabled";
          p2p.address = "127.0.0.1";
          limits.concurrency = 1;
          settings = {
            fixed-difficulty = 1;
            disable-dns-checkpoints = true;
          };
        }
        // lib.optionalAttrs custom { dataDir = "/custom/qwertycoin"; }
      );
    in
    {
      inherit network custom;
      inherit (cfg) dataDir effectiveDataDir daemonArguments;
    };
  fixtures =
    lib.concatMap
      (
        network:
        map (fixture network) [
          false
          true
        ]
      )
      [
        "mainnet"
        "testnet"
        "stagenet"
        "regtest"
      ];
in
pkgs.runCommand "qwertycoin-daemon-runtime-check"
  {
    nativeBuildInputs = [ pkgs.python3 ];
    __darwinAllowLocalNetworking = true;
    passthru = { inherit fixtures; };
  }
  ''
    python3 ${./check-daemon.py} ${pkgs.writeText "qwertycoin-runtime-fixtures.json" (builtins.toJSON fixtures)}
    touch "$out"
  ''
