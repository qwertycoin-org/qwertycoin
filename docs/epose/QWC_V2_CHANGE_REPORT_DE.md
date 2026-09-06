# Qwertycoin v2 / EPoSE Gesamtbericht

> Historical snapshot - not current protocol documentation. This report
> describes an earlier PoC/hardening state and may mention transparent rewards,
> testnet-only assumptions, or old PR gates. For current `main`, use
> `README.md`, `PROTOCOL.md`, `REWARDS.md`, `SERVICE_NODE.md`, and
> `IMPLEMENTATION_REPORT.md`.

Stand: 2026-09-01

Repository: `<project-repository>`
Aktueller Branch bei Erstellung: `main` -> Bericht erstellt auf `docs/qwc-v2-change-report`

Basis: Monero `v0.18.5.1`

Aktueller Main-Stand: `4e697d4af` (`Merge pull request #5 from feature/epose-release-readiness`)

## Zweck

Dieser Bericht fasst die bisher gemeinsam umgesetzten Änderungen im Core-Repository `qwertycoin-v2-poc` zusammen. Er ist als Bewertungsgrundlage gedacht: Was ist drin, was wurde validiert, welche Entscheidungen sind noch offen, und was fehlt vor einem echten öffentlichen Release.

Nicht enthalten sind Detailänderungen im separaten Explorer-Repository, außer dort, wo sie für Betrieb/Validierung des Core-Netzwerks relevant sind.

## Kurzfazit

Qwertycoin v2 ist jetzt technisch deutlich mehr als ein reiner Fork-Start. Der aktuelle `main` enthält:

- QWC-v2-Chainstart ab Genesis mit aktivem QWC-HF17-Protokoll.
- Wiederhergestellte QWC-Parameter für Supply, 8 Dezimalstellen, Ports, Adresspräfix und Branding.
- EPoSE als deterministische Service-Node-Schicht mit Registrierung, Attestations, Qualification, Reward-Auswahl und Coinbase-Validierung.
- Daemon-/Wallet-/RPC-Unterstützung für Service-Node-Betrieb.
- Umfangreiche Unit-, Fuzz-, Sanitizer- und Multi-Node-Harness-Tests.
- Live-Validierung mit drei bekannten Service Nodes und zusätzlichem externem Fresh-Bootstrap-Test auf `independent validation host`.
- Mainnet-Deployment-Manifeste und einen manuellen Gitian-Workflow mit SHA-256-Artefakten.

Meine Einschätzung: Der aktuelle Stand ist gut für eine kontrollierte öffentliche Test-/Relaunch-Phase mit klarer Kommunikation. Für ein endgültiges Mainnet ohne Reset fehlen noch finalisierte Tokenomics-/Privacy-/Release-Entscheidungen und längere unabhängige Validierung.

## PR-Übersicht

| PR | Status | Ziel | Merge |
| --- | --- | --- | --- |
| #2 `feat: add EPoSE v2 primitives` | merged | EPoSE-Grundlagen, QWC-v2-Baseline, Tests, RPC, Wallet-Flow | `4ae933da2`, 2026-08-29 |
| #3 `feat: harden EPoSE admission and reward epochs` | merged | Hardening, RandomX-bound Admission, Reorg-/Fuzz-/Branding-Ausbau | `bfc5b8844`, 2026-08-31 |
| #4 `Validate QWC v2 mainnet EPoSE rollout` | merged | 3-Node-Mainnet-Validierung, Seeds, Compose-Manifeste | `3b504be62`, 2026-09-01 |
| #5 `fix: complete EPoSE release readiness` | merged into `main` | Fresh Bootstrap, Operator-Flow, Gitian-Release-Artefakte | `4e697d4af`, 2026-09-01 |

Hinweis: PR #4 wurde zuerst in `feature/epose-hardening-v2` gemergt. PR #5 wurde danach auf `main` umgestellt und gemergt. Anschließend wurden die überflüssigen Feature-Branches gelöscht. `origin` enthält aktuell nur noch `main`.

## Umfang

Gegenüber Monero `v0.18.5.1` enthält der aktuelle Stand grob:

- 191 geänderte Dateien
- ca. 12.905 hinzugefügte Zeilen
- ca. 2.037 entfernte Zeilen
- 142 Non-Merge-Commits
- relevante neue Bereiche: `src/epose/`, `tests/epose/`, EPoSE-RPCs, QWC-Branding, Deployment-Manifeste, Release-Workflow, Dokumentation

## Chain- und QWC-Baseline

Umgesetzt:

- QWC v2 startet als neue Chain.
- QWC HF17 / `HF_VERSION_QWC_EPOSE = 17` ist ab Height `0` aktiv; das EPoSE-Wireformat hat unabhängig davon Version 2.
- Die Chain durchläuft nicht künstlich historische Monero-Hardforks.
- Hardfork-Infrastruktur bleibt erhalten, damit spätere QWC-Upgrades sauber als neue Versionen aktivierbar sind.
- Blocktarget v2 ist `120` Sekunden.
- QWC Supply wurde auf `184,467,440.73709551 QWC` mit 8 Dezimalstellen gesetzt.
- Final subsidy pro Minute: `0.3 QWC`.
- Historische QWC-Portfamilie:
  - P2P: `8196`
  - Daemon RPC: `8197`
  - Wallet RPC: `8198`
  - ZMQ RPC: `8199`
- Mainnet-Adresspräfix: `0x14820c`, Adressen beginnen mit `QWC`.
- `CRYPTONOTE_NAME` ist `qwertycoin`.

Bewertung:

- Für einen v2-Relaunch ist diese Richtung sinnvoll, weil sie alte Fork-Übergänge vermeidet.
- Die v2-Chain ist nicht kompatibel mit alter Chain-/Wallet-Historie. Alte Seeds/Keys können nur als Schlüsselmaterial wiederverwendet werden, nicht als alter Balance-Import.

Noch offen:

- Finale Bestätigung aller historischen Adresspräfixe für Integrated/Subaddress/Testnet/Stagenet.
- Finale Mainnet-Genesis-Artefakte und öffentliche Release-Kommunikation.
- Falls es eine Migration alter Ansprüche geben soll, muss diese außerhalb der alten Chain-State-Kompatibilität definiert werden.

## EPoSE-Kern

Neu eingeführt wurde `src/epose/` mit:

- Service-Node-Identity
- Service-Node-Registration
- Endpoint-Commitment
- Admission Proof
- Attestation-Struktur
- Verifier-Committee-Auswahl
- Challenge-/Response-Hashing
- Chain-State für Registrierungen, Attestations, Qualification und Reward-Auswahl
- Service-Registry-Hilfen
- Service-Node-Konfiguration und Key-Datei-Handling

EPoSE ist bewusst als zusätzliche Service-Schicht gebaut. PoW/RandomX bleibt die primäre Chain-Sicherheit.

## Service-Node-Registrierung

Umgesetzt:

- Service Nodes registrieren sich über signierte `tx_extra_nonce`-Payloads.
- Eine Registrierung enthält Service Public Key, Reward-Adresse, Endpoint-Commitment, Registration Epoch, Expiry Epoch, Nonce, Admission Hash und Signatur.
- Private Service Keys bleiben lokal.
- Registrierungen sind netzwerk-, identitäts-, reward-, endpoint-, epoch- und seed-gebunden.
- Der Daemon kann als Service Node gestartet werden:
  - `--service-node`
  - `--service-node-key`
  - `--service-reward-address`
  - `--service-node-advertise-address`
- Service-Node-Modus lehnt pruned Chains ab.
- Daemon kann lokale Registration-Payloads erzeugen.
- Wallet CLI hat den Operator-Befehl `register_service_node`.

Live validiert:

- Wallet-CLI-Flow erzeugte reale Registration-TX:
  `562c84fb9e90c003b062eb4ce7d9250a7c394f3559605a6ae5607e999c5e30ca`
- Nach Mining bis Height `300` meldete `get_service_node_status` für den neuen Service-Key `found:true`.

Bewertung:

- Der Operator-Flow ist funktional bewiesen.
- Für Mainnet sollte noch mehr UX-/Fehlerfall-Test erfolgen: falsches Netzwerk, falsche Reward-Adresse, unzureichende Balance, erneute Registrierung, abgelaufene Registrierung.

## Admission Proof / Sybil-Schutz

Umgesetzt:

- Admission Proof ist nicht mehr nur ein schneller Hash, sondern RandomX-bound.
- Proof ist gebunden an Netzwerk, Service Public Key, Reward-Adresse, Endpoint-Commitment, Epoch, Previous Epoch Hash und Nonce.
- Benchmark- und Simulationscode wurde ergänzt.

Aktuelle Testparameter:

- `EPOSE_ADMISSION_LEADING_ZERO_BITS = 8`
- `EPOSE_VERIFIER_COMMITTEE_SIZE = 5`
- `EPOSE_MIN_ATTESTATIONS = 2`

Bewertung:

- Für Testnet/Smoke ist das ausreichend.
- Für Mainnet ist das noch nicht final. Die aktuelle Simulation zeigt: Committee Size `5` mit Threshold `2` ist bei hoher kontrollierter Identitätsquote zu schwach.

Noch offen:

- Finale Admission-Difficulty anhand echter Hardware-Benchmarks.
- Größeres Committee und höherer Threshold für Mainnet prüfen.
- Optional: ökonomische Kosten/Stake/Collateral bewusst entscheiden, falls reine Admission-Arbeit nicht reicht.

## Attestations und Qualification

Umgesetzt:

- Attestations sind signiert.
- Sie sind netzwerk-, subject-, verifier-, epoch-, challenge- und response-gebunden.
- Self-Attestations werden abgelehnt.
- Doppelte Votes desselben Verifiers zählen nicht mehrfach.
- Unregistrierte, inaktive oder nicht ausgewählte Verifier werden abgelehnt.
- Verifier Committee wird deterministisch aus Epoch Seed und Service Keys gebildet.
- Qualification basiert auf eindeutigen gültigen Attestations.
- State wird deterministisch gepruned.

Bewertung:

- Der Konsenspfad ist für die aktuelle Testphase solide modelliert.
- Der reale Service-Proof ist noch abstrahiert. Aktuell ist das eher ein kryptografisch sauberer Attestation-Mechanismus als ein vollständiger Beweis realer Servicequalität.

Noch offen:

- Genauer definieren, was ein Service Node beweisen muss: Erreichbarkeit, Latenz, Chain-Tip, RPC-Fähigkeit, Datenverfügbarkeit, Version, Uptime.
- Schutz gegen „freundliche Cluster“, die sich gegenseitig attestieren.
- Langfristige Regeln für Slashing/Disqualification sind noch nicht implementiert.

## Epochs und Rewards

Aktuelle Konstanten:

- Epoch Length: `720` Blöcke
- Finality Depth: `60` Blöcke
- Registration TTL: `30` Epochs
- Service Reward: `1000` bps = `10%`

Umgesetzt:

- Pro Block wird maximal eine qualifizierte Service Node bezahlt.
- Auswahl des Payees ist deterministisch.
- Rewards verwenden die qualifizierte Menge aus dem vorher finalisierten Reward-Source-Epoch, nicht aus dem aktuellen manipulierbaren Epoch-Zustand.
- Coinbase-Transaktion muss den korrekten Service-Reward-Output enthalten, wenn ein qualifizierter Payee existiert.
- Fehlender, falscher, überzahlter, unterzahlter oder doppelter Service-Reward-Output wird abgelehnt.
- `get_service_rewards` wurde korrigiert, damit RPC und Konsens dieselbe Reward-Source-Epoch verwenden.

Aktuelle Testformel:

```text
total_reward = base_reward + transaction_fees
service_reward = floor(total_reward * 1000 / 10000)
miner_reward = total_reward - service_reward
```

Live validiert:

- Service Reward wurde bei Block `222` bewiesen.
- Coinbase total reward: `35169477060`
- Miner output: `31652529354`
- Service reward output: `3516947706`
- Split: `10%`

Bewertung:

- Technisch ist der Reward-Pfad end-to-end bewiesen.
- Der aktuelle transparente Reward-Output ist ein Testnet-Kompromiss.
- Der Split inklusive Fees ist nicht automatisch die beste Mainnet-Entscheidung.

Noch offen:

- Final entscheiden: Service Reward aus Subsidy-only oder aus Subsidy + Fees.
- Finalen Service-Reward-Prozentsatz festlegen.
- Privacy-kompatiblen Service-Reward-Output entwerfen oder bewusst transparent akzeptieren.

## Reorg, Restart und State-Rebuild

Umgesetzt:

- EPoSE-State wird aus kanonischen Chain-Daten rekonstruiert.
- Block-Validation wendet EPoSE-Änderungen atomar an.
- Bei Fehlern wird der EPoSE-State zurückgerollt.
- Snapshot-/Restore-Mechanik für kurze Reorgs.
- Deterministischer State Hash für Node-Vergleich.
- Fresh Sync eines Validators kann denselben EPoSE-State rekonstruieren.
- A-B-A-Reorg- und Partition/Heal-Szenarien wurden im Harness getestet.

Bewertung:

- Für die aktuelle Implementierungsstufe ist das gut.
- Dedizierte persistente LMDB-Indizes für EPoSE-State existieren noch nicht. Das ist bewusst einfacher, kann aber bei großen Chains später teuer werden.

Noch offen:

- Performance von Full Rebuild bei langer Chain messen.
- Persistente Derived-State-Indizes entwerfen, falls Rebuild-Zeit zu groß wird.
- Mehr reale Reorg-/Chaos-Tests außerhalb des Harness.

## RPC und CLI

Neue bzw. relevante Daemon-RPCs:

- `get_epose_info`
- `get_service_nodes`
- `get_service_node_status`
- `get_epose_epoch`
- `get_service_rewards`
- `get_service_node_registration_payload`

RPC liefert:

- EPoSE enabled state
- Epoch-Kontext
- Service Nodes
- Qualification Count
- State Hash
- Reward-Auswahl
- lokale Service-Node-Konfiguration, soweit öffentlich

Nicht geliefert:

- private Service Keys
- geheime Operator-Daten

CLI/Daemon:

- Daemon-Kommandos für EPoSE-Status und Registration Payload.
- Service-Node-Start prüft Reward-Adresse und Advertise-Adresse.
- Unsafe Admin-RPCs bleiben im öffentlichen restricted RPC blockiert.

Bewertung:

- Beobachtung und Operator-Basis sind vorhanden.
- Für externe Integrationen ist API-Stabilisierung/Dokumentation der Response-Schemas noch ein Punkt.

## Wallet

Umgesetzt:

- `qwertycoin-wallet-cli` kann Service Nodes registrieren.
- Wallet-Display wurde auf 8 Dezimalstellen/QWC-Einheiten ausgerichtet.
- Wallet URI/QR verwendet `qwertycoin:` statt `monero:`.
- Offline-Transaction-, Proof- und verwandte Dateinamen wurden QWC-gebrandet.
- Wallet-Hilfe zeigt Qwertycoin-Defaults, insbesondere Daemon RPC `8197`.
- OpenAlias-Marker ist `oa1:qwc`; `oa1:xmr` wird nicht als QWC akzeptiert.

Bewertung:

- Operator-Flow ist grundsätzlich nutzbar.
- Wallet-UX braucht vor öffentlichem Release noch reale Nutzer-Smokes auf macOS/Linux/Windows.

## Branding und Binary Names

Umgesetzt:

- Öffentliche Daemon-/Wallet-Binaries:
  - `qwertycoind`
  - `qwertycoin-wallet-cli`
  - `qwertycoin-wallet-rpc`
- Public Utility Names:
  - `qwertycoin-blockchain-*`
  - `qwertycoin-gen-ssl-cert`
  - weitere QWC-gebrandete Hilfsprogramme
- Alte Docker-Runtime-Aliases für `monerod`, `monero-wallet-cli`, `monero-wallet-rpc` wurden entfernt.
- Public Help-/Version-Texte wurden weitgehend QWC-gebrandet.
- README wurde stark überarbeitet.
- Update-Check wurde von Monero-Infrastruktur entkoppelt.
- DNS-/OpenAlias-/Donation-Records wurden dokumentiert und release-gated.

Bewertung:

- Öffentlich sichtbares Branding ist weitgehend bereinigt.
- Interne Monero-Namen bleiben an vielen Stellen bewusst erhalten, wo sie Upstream-Namespace, Dateiformat, Library-Kompatibilität oder Attribution betreffen.

Noch offen:

- Finaler Release-Artifakt-Smoke auf allen Plattformen.
- Optional: letzte öffentliche String-Audits vor Release.

## P2P, Seeds und Bootstrap

Umgesetzt:

- Built-in Mainnet-DNS-Seeds:
  - `seed-00.qwertycoin.org`
  - `seed-01.qwertycoin.org`
  - `seed-02.qwertycoin.org`
- Statische Mainnet-Seed-Fallbacks für bekannte Nodes.
- Fix für Fresh Seed-only Bootstrap: erfolgreich erreichte Seeds werden als bekannte Peers behalten, auch wenn der Seed-Handshake nur `just_take_peerlist` nutzt.
- Damit funktionieren Seeds auch dann, wenn Seed-Daemons mit `--hide-my-port` laufen und sich nicht selbst zurückmelden.
- `--max-connections-per-ip=4` wurde in Mainnet-Smoke-Manifeste aufgenommen, damit temporäre Same-IP-Smokes nicht blockieren.

Live validiert:

- Frischer Seed-only Container auf seed host A ging von Height `1` auf `293`, synchronisierte und hatte denselben EPoSE-State-Hash wie live.
- Externer vierter Host `independent validation host` startete mit leerem Docker-Volume, synchronisierte von Height `1` auf `300` und matchte Top Hash plus EPoSE State.

Bewertung:

- Der zuvor offene Fresh-Bootstrap-Punkt ist damit gelöst.
- Für Public Release sollten mindestens zwei bis drei echte externe Hosts dauerhaft als normale Peers/Seeds laufen.

## Deployment

Hinzugefügt:

- `deploy/mainnet/docker-compose.seed-a.yml`
- `deploy/mainnet/docker-compose.seed-b.yml`
- `deploy/mainnet/docker-compose.seed-c.yml`
- vorherige Testnet-/Beta-Compose-Dateien

Live validiert:

- Drei bekannte Nodes liefen konsistent mit `qwertycoin-v2-node:mainnet-epose-validation`.
- Später wurde seed host A auf `qwertycoin-v2-node:epose-release-readiness` aktualisiert.
- `independent validation host` wurde nur als isolierter temporaerer Fresh-Bootstrap-Host genutzt; produktive Dienste wurden nicht veraendert.

Bekannter operativer Punkt:

- Direkter SSH-Rollout auf seed host B und seed host C war in einem spaeteren Schritt aus der Arbeitsumgebung nicht verfuegbar. Die P2P-Ports waren erreichbar, aber das Release-Readiness-Image wurde live nur auf seed host A ausgerollt.

Bewertung:

- Für Testbetrieb ausreichend.
- Für Production braucht es noch saubere, wiederholbare Deployment-Automation statt manueller Host-Schritte.

## Release / Gitian

Umgesetzt:

- Gitian-Workflow wurde wieder nutzbar gemacht.
- Manuelle Inputs für Branch/Tag/Commit wurden ergänzt.
- Plattformjobs erzeugen `SHA256SUMS`.
- Workflow kann Linux/macOS/Windows-Artefakte vorbereiten.

Bewertung:

- Release-Pipeline ist vorbereitet, aber noch nicht final bewiesen.

Noch offen:

- Gitian-Run muss vollständig grün durchlaufen.
- Checksums müssen veröffentlicht und idealerweise signiert werden.
- Reproduzierbare Builds sollten von mindestens zwei unabhängigen Umgebungen bestätigt werden.
- Release Notes und Download-Seite müssen finalisiert werden.

## Tests und Validierung

Umgesetzt bzw. dokumentiert:

- Fokus-Unit-Tests für EPoSE.
- Eigener `epose_unit_tests` Target.
- Fuzz Target `epose_fuzz_tests`.
- Seed Corpus für EPoSE Parser/State.
- ASan-Builds und ASan-Smokes.
- UBSan-Fuzz-Smoke.
- Admission Benchmarks.
- Sybil-/Committee-Simulator.
- Lokaler Multi-Node-Harness mit 3/5/10/25 Nodes.
- Tests für:
  - Registration Parsing und Signatures
  - Admission Proof Binding
  - Attestation Parsing und Signatures
  - Duplicate Vote Rejection
  - falsche/fehlende Verifier
  - Challenge/Response Binding
  - Reward Split
  - Payee Rotation
  - Reward RPC Epoch Boundary
  - Reorg Rollback
  - Fresh Sync Rebuild
  - SIGKILL/Restart
  - Mixed valid/invalid payload rollback
  - QWC Address Prefixes
  - QWC URI Scheme
  - Branding Smoke Checks

Wichtige validierte Ergebnisse:

- EPoSE Unit Tests liefen mehrfach grün, zuletzt dokumentiert bis 67/67 bzw. 64/64 je nach Build-Target.
- 11/11 EPoSE Fuzz Seeds liefen ohne Crash.
- 25-Node-Harness validierte Registrierung, Attestation, Qualification, Rewards, Persisted Restart und Partition/Heal/Rejoin/Reorg.
- Reale 3-Host-Tests validierten Registrierung, Qualification, Rewards, Restart/Rejoin und Partition/Rejoin.
- Mainnet-Smoke mit 3 bekannten Nodes validierte gleichen Height/Top Hash/State Hash.
- Service Reward wurde live an Block `222` nachgewiesen.
- Fresh Bootstrap wurde extern auf `independent validation host` nachgewiesen.
- M1/macOS native Build- und Daemon-Smoke wurde vorbereitet und auf einem Apple-Silicon-Host erfolgreich gestartet.

Nicht sauber grün:

- UBSan Unit-Test-Binary stoppt vor GTest durch eine geerbte Boost-Serialization-Static-Init-Meldung. Der EPoSE-Fuzz-Pfad war UBSan-clean, aber ein kompletter UBSan-Unit-Pass ist nicht belegt.
- GitHub Actions waren zeitweise durch Runner-/Minutes-Probleme blockiert; lokale Docker-Validierung auf seed host A wurde als Ersatz genutzt.

## Dokumentation

Neu bzw. stark erweitert:

- `README.md`
- `docs/epose/README.md`
- `docs/epose/DESIGN.md`
- `docs/epose/PROTOCOL.md`
- `docs/epose/SERVICE_NODE.md`
- `docs/epose/REWARDS.md`
- `docs/epose/THREAT_MODEL.md`
- `docs/epose/HARDENING_STATUS.md`
- `docs/epose/MAINNET_TESTPHASE.md`
- `docs/epose/MAINNET_VALIDATION.md`
- `docs/epose/MACOS_M1_TESTING.md`
- `docs/epose/DNS_RECORDS.md`
- `docs/epose/SERVICE_REWARD_PRIVACY.md`
- `docs/releases/v2.0.0-beta.1.md`

Bewertung:

- Die technische Dokumentation ist für Review gut.
- Vor öffentlichem Release braucht es eine deutlich kürzere Operator-Anleitung und klare Warnhinweise zu Testphase/Reset-Risiko.

## Aktueller Live-Stand aus letzter Validierung

Zuletzt dokumentierter Mainnet-Smoke:

- Height: `300`
- Top hash: `835f6ad54d1aadd026c0a7368346cb298e7ed282aed003e707f1c1b5834145d3`
- EPoSE:
  - `service_node_count: 4`
  - `qualified_count: 3`
  - `attestation_count: 7`
  - state hash: `6e07eb52425a85f6d135ebb0fa46d625639e6c64352ad0a51b8177780b50bb44`

Späteres M1-Mining zeigte, dass die frühe Difficulty niedrig war und die Chain sehr schnell Blöcke finden konnte. Danach stieg die Difficulty sprunghaft von ca. `3000` auf ca. `47043`, was zeigt, dass der Retarget grundsätzlich reagiert. Das ist kein direkter Konsensfehler, aber ein Launch-Parameter-Risiko.

## Wichtige Risiken

### 1. Transparenter Service Reward

Der aktuelle Service-Reward-Output ist consensus-sichtbar. Das macht Validierung einfach, ist aber privacy-technisch kein finaler Monero-/CryptoNote-Standard.

Empfehlung: Für Testphase akzeptabel, für Mainnet final entscheiden.

### 2. EPoSE-Parameter noch testnet-lastig

`10%`, `committee_size=5`, `min_attestations=2`, `admission_bits=8` sind Test-/Bootstrap-Parameter.

Empfehlung: Vor Mainnet final messen und härten.

### 3. Frühe Difficulty / Launch-Geschwindigkeit

Die Chain kann in der Anfangsphase bei niedriger Difficulty sehr schnell wachsen. Der Retarget reagiert, aber einzelne Miner können vorher viele Blöcke erzeugen.

Empfehlung: Early-Difficulty-/Launch-Parameter prüfen. Für echten Public Launch ggf. höhere Mindest-Difficulty oder aggressiveres Early-Retargeting einsetzen.

### 4. Rebuild statt persistenter EPoSE-Indizes

State-Rebuild aus Chain-Daten ist sauber und deterministisch, aber langfristig eventuell teuer.

Empfehlung: Erst messen, dann bei Bedarf LMDB-derived-state ergänzen.

### 5. Release-Prozess noch nicht endgültig bewiesen

Gitian ist vorbereitet, aber finaler grüner Multi-Plattform-Release mit signierten Checksums fehlt.

Empfehlung: Vor öffentlicher Download-Freigabe zwingend nachholen.

## Was Noch Fehlt

Aus meiner Sicht sind die wichtigsten offenen Punkte:

1. Finaler Mainnet-Entscheid zu EPoSE-Tokenomics:
   - Service-Reward-Prozent
   - Subsidy-only vs. Subsidy + Fees
   - Umgang mit sehr frühen Blöcken

2. Finaler Mainnet-Entscheid zu Privacy:
   - transparenter Service Reward akzeptabel oder nicht
   - falls nein: CryptoNote-kompatiblen verifizierbaren Reward-Mechanismus entwickeln

3. Finaler Sybil-/Admission-Parameter:
   - Admission-Difficulty aus Benchmarks
   - Committee Size
   - Qualification Threshold

4. Difficulty-/Launch-Hardening:
   - M1-Fall auswerten
   - prüfen, ob Mainnet-Minimum-Difficulty oder Early-Retarget nötig ist
   - verhindern, dass ein einzelner Laptop tausende frühe Blöcke in kurzer Zeit mint

5. Release-Artefakte:
   - Gitian vollständig grün
   - Linux/macOS/Windows Builds
   - SHA256SUMS
   - Signaturen
   - unabhängige Repro-Build-Bestätigung

6. Deployment-Automation:
   - reproduzierbarer Rollout auf alle Seed-/Service-Nodes
   - klare Compose/Systemd-Konvention
   - Monitoring/Restart-Policy

7. Operator-Dokumentation:
   - kurze Anleitung für normale Betreiber
   - Service-Node-Registrierung
   - Wallet-Funding
   - Logs und Troubleshooting

8. Langlauf- und externe Tests:
   - mehrere echte externe Hosts
   - längerer Mining-/Sync-Lauf
   - Reorg-/Partition-Varianten
   - Fresh Bootstrap aus unterschiedlichen Netzen

9. Vollständige CI:
   - GitHub Actions grün
   - EPoSE Unit/Fuzz/ASan
   - Release Workflow
   - Branding Smokes

## Operator Decision List

Diese Punkte sollten vor einem finalen Relaunch bewusst entschieden werden:

- Ist QWC v2 eine öffentliche Testphase mit möglichem Reset oder endgültiges Mainnet?
- Bleibt der Service Reward initial bei `10%`?
- Werden Fees in den Service-Reward-Split einbezogen?
- Akzeptieren wir transparente Service Rewards für die erste Mainnet-Version?
- Welche Mindest-Difficulty bzw. Early-Retarget-Regel wollen wir für den Launch?
- Welche Hosts sind dauerhafte Seeds?
- Welche DNS-Namen werden final veröffentlicht?
- Wann gelten Release-Artefakte als vertrauenswürdig genug?
- Wie kommunizieren wir alte Wallets/alte Chain/alte Balances?

## Empfohlene Nächste Schritte

1. Difficulty-Launch-Verhalten messen und ggf. fixen.
2. Gitian-Run und Release-Artefakte sauber grün bekommen.
3. Service-Reward-Parameter final festlegen.
4. Privacy-Entscheidung treffen.
5. Einen echten mehrstündigen Public-Seed-Smoke mit mindestens 4 externen Nodes fahren.
6. Operator-Kurzanleitung schreiben.
7. Danach erst öffentliche Testphase ankündigen.

## Technische Referenzen

Wichtige Dateien im aktuellen Stand:

- `src/epose/service_node.h`
- `src/epose/service_node.cpp`
- `src/epose/service_epoch.h`
- `src/epose/service_epoch.cpp`
- `src/epose/chain_state.h`
- `src/epose/chain_state.cpp`
- `src/epose/service_node_config.*`
- `src/epose/service_registry.*`
- `src/cryptonote_core/blockchain.cpp`
- `src/cryptonote_core/cryptonote_core.cpp`
- `src/cryptonote_core/cryptonote_tx_utils.cpp`
- `src/rpc/core_rpc_server.cpp`
- `src/rpc/core_rpc_server_commands_defs.h`
- `src/simplewallet/simplewallet.cpp`
- `src/wallet/wallet2.cpp`
- `src/p2p/net_node.inl`
- `src/cryptonote_config.h`
- `src/hardforks/hardforks.cpp`
- `tests/unit_tests/epose.cpp`
- `tests/fuzz/epose.cpp`
- `tests/epose/integration/local_epose_network.sh`
- `.github/workflows/epose.yml`
- `.github/workflows/gitian.yml`
- `deploy/mainnet/*.yml`
- `docs/epose/MAINNET_VALIDATION.md`
- `docs/epose/HARDENING_STATUS.md`

## Gesamtbewertung

Wir haben die drei großen technischen Blöcke geschafft:

1. QWC v2 als eigene neue Chain mit korrekter QWC-Basis.
2. EPoSE als deterministische, getestete Service-Node-Konsensschicht.
3. Live-Betrieb mit Bootstrap, Operator-Registration und Service-Reward-Nachweis.

Was noch fehlt, ist weniger „Feature-Code“ und mehr „Mainnet-Reife“: finale Parameter, Privacy-Entscheidung, Release-Artefakte, längere externe Tests und sauberer Operator-/Deployment-Prozess.

Pragmatische Empfehlung: Nicht mehr große neue Features einbauen, bevor die offenen Parameter entschieden sind. Als nächstes sollte der Launch-/Difficulty-Teil gehärtet werden, weil der M1-Test gezeigt hat, dass die frühe Chain sonst operativ zu schnell wachsen kann.
