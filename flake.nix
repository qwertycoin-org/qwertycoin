{
  description = "Qwertycoin Core package and development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
    nix-darwin = {
      url = "github:nix-darwin/nix-darwin/nix-darwin-25.11";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    # Keep these revisions in sync with the Git submodules.
    rapidjson = {
      url = "github:Tencent/rapidjson/129d19ba7f496df5e33658527a7158c79b99c21c";
      flake = false;
    };
    randomx = {
      url = "github:tevador/RandomX/6c4340ba4561aec9a3611c1aedf9931239777fb3";
      flake = false;
    };
    supercop = {
      url = "github:monero-project/supercop/633500ad8c8759995049ccd022107d1fa8a1bbc9";
      flake = false;
    };
    trezor-common = {
      url = "github:trezor/trezor-common/bff7fdfe436c727982cc553bdfb29a9021b423b0";
      flake = false;
    };
  };

  outputs =
    inputs@{ self, nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      pkgsFor = system: import nixpkgs { inherit system; };
      # Clang 20+ rejects enum conversions used by Boost's type traits.
      stdenvFor = pkgs: if pkgs.stdenv.cc.isClang then pkgs.llvmPackages_19.stdenv else pkgs.stdenv;
    in
    {
      overlays.default = final: prev: {
        qwertycoin = self.packages.${prev.stdenv.hostPlatform.system}.qwertycoin;
      };

      nixosModules.qwertycoin-node = import ./nix/qwertycoin-node.nix { inherit self; };
      nixosModules.default = self.nixosModules.qwertycoin-node;
      darwinModules.qwertycoin-node = import ./nix/darwin-node.nix { inherit self; };
      darwinModules.default = self.darwinModules.qwertycoin-node;

      checks = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          nixos-module =
            assert import ./nix/check-module.nix { inherit self nixpkgs system; };
            pkgs.runCommand "qwertycoin-node-module-check" { } ''touch "$out"'';
        }
        // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isDarwin {
          darwin-module =
            assert import ./nix/check-darwin-module.nix {
              inherit self nixpkgs system;
              nix-darwin = inputs.nix-darwin;
            };
            pkgs.runCommand "qwertycoin-darwin-module-check" { } ''touch "$out"'';
        }
      );

      packages = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
          inherit (pkgs) lib;
          stdenv = stdenvFor pkgs;
          qwertycoin = stdenv.mkDerivation {
            pname = "qwertycoin";
            version = (builtins.fromJSON (builtins.readFile ./docs/releases/current.json)).version;

            src = lib.cleanSourceWith {
              src = self;
              filter =
                path: type:
                lib.cleanSourceFilter path type
                && !(builtins.elem (baseNameOf path) [
                  "build"
                  "result"
                ])
                && !(lib.hasPrefix "result-" (baseNameOf path));
            };

            # Flake source snapshots omit Git submodules. Supply their pinned
            # contents without requiring Git or network access in the build.
            postUnpack =
              lib.concatMapStringsSep "\n"
                (name: ''
                  rm -rf "$sourceRoot/external/${name}"
                  cp -R ${inputs.${name}} "$sourceRoot/external/${name}"
                  chmod -R u+w "$sourceRoot/external/${name}"
                '')
                [
                  "rapidjson"
                  "randomx"
                  "supercop"
                  "trezor-common"
                ];

            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              pkg-config
              python3
            ];

            # Clang 19 still permits the legacy Boost enum trait conversions.
            env.NIX_CFLAGS_COMPILE = lib.optionalString stdenv.cc.isClang "-Wno-enum-constexpr-conversion";
            buildInputs =
              with pkgs;
              [
                boost
                openssl
                zeromq
                unbound
                libsodium
                hidapi
                libusb1
                readline
                expat
              ]
              ++ lib.optionals stdenv.hostPlatform.isLinux [ pkgs.libunwind ]
              ++ lib.optionals stdenv.hostPlatform.isDarwin [ pkgs.libiconv ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              # CMake 4 removed compatibility with the older bundled projects.
              "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
              "-DARCH=default"
              "-DBUILD_64=ON"
              "-DSTATIC=OFF"
              "-DBUILD_SHARED_LIBS=OFF"
              "-DBUILD_TESTS=OFF"
              "-DBUILD_DOCUMENTATION=OFF"
              "-DMANUAL_SUBMODULES=ON"
              "-DUSE_CCACHE=OFF"
              "-DUSE_DEVICE_TREZOR=OFF"
              "-DOPENSSL_ROOT_DIR=${lib.getDev pkgs.openssl}"
              "-DReadline_ROOT_DIR=${lib.getDev pkgs.readline}"
              "-DReadline_INCLUDE_DIR=${lib.getDev pkgs.readline}/include"
              "-DReadline_LIBRARY=${lib.getLib pkgs.readline}/lib/libreadline${stdenv.hostPlatform.extensions.sharedLibrary}"
            ];

            doInstallCheck = true;
            installCheckPhase = ''
              runHook preInstallCheck
              "$out/bin/qwertycoind" --version
              test -x "$out/bin/qwertycoin-wallet-cli"
              test -x "$out/bin/qwertycoin-wallet-rpc"
              runHook postInstallCheck
            '';

            meta = {
              description = "Qwertycoin Core daemon, wallet, and blockchain tools";
              homepage = "https://github.com/qwertycoin-org/qwertycoin";
              license = lib.licenses.bsd3;
              platforms = systems;
              mainProgram = "qwertycoind";
            };
          };
        in
        {
          inherit qwertycoin;
          default = qwertycoin;
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = (pkgs.mkShell.override { stdenv = stdenvFor pkgs; }) {
            inputsFrom = [ self.packages.${system}.qwertycoin ];
            packages = with pkgs; [
              git
              ccache
            ];
            OPENSSL_ROOT_DIR = "${pkgs.lib.getDev pkgs.openssl}";
            Readline_ROOT_DIR = "${pkgs.lib.getDev pkgs.readline}";
            Readline_LIBRARY = "${pkgs.lib.getLib pkgs.readline}/lib/libreadline${pkgs.stdenv.hostPlatform.extensions.sharedLibrary}";
            NIX_CFLAGS_COMPILE = pkgs.lib.optionalString pkgs.stdenv.cc.isClang "-Wno-enum-constexpr-conversion";
          };
        }
      );

      formatter = forAllSystems (system: (pkgsFor system).nixfmt-rfc-style);
    };
}
