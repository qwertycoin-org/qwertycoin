# Qwertycoin Core release package

This package contains only the supported Core programs:

- `qwertycoind`
- `qwertycoin-wallet-cli`
- `qwertycoin-wallet-rpc`

Windows uses the `.exe` suffix. Runtime libraries in `lib/` or beside the Windows executables belong to these exact programs. No GUI is included; Trezor support is disabled in this first portable build contract.

## Verify and start

Verify the archive against the top-level `SHA256SUMS` file from the GitHub Release before extracting it. The archive also contains an inner `.sha256` manifest covering every regular file.

Linux and macOS:

```sh
tar -xzf qwertycoin-<tag>-<platform>.tar.gz
cd qwertycoin-<tag>-<platform>
./qwertycoind --version
./qwertycoin-wallet-cli --help
```

Windows PowerShell:

```powershell
Expand-Archive qwertycoin-<tag>-windows-x86_64.zip -DestinationPath .
Set-Location .\qwertycoin-<tag>-windows-x86_64
.\qwertycoind.exe --version
.\qwertycoin-wallet-cli.exe --help
```

Start the daemon before opening a wallet. Bind RPC to loopback unless you have explicitly configured authentication, TLS and network access controls. Never expose an unrestricted wallet RPC or daemon RPC port to the public Internet.

See `BUILD-INFO.json` for exact source, submodule, toolchain, compatibility, signing, network and test provenance. `SHA256SUMS` proves integrity, not publisher identity.
