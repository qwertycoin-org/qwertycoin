# Qwertycoin v2 / EPoSE - PR #2 Ueberblick

> Historical snapshot - not current protocol documentation. This file describes
> an earlier PR/testnet state and intentionally preserves historical wording.
> For current `main`, use `README.md`, `PROTOCOL.md`, `REWARDS.md`,
> `SERVICE_NODE.md`, and `IMPLEMENTATION_REPORT.md`.

Stand: 2026-08-29

Branch: `feature/epose-v2`

PR: `<repository-url>/pull/2`
Dokumentierter Code-Stand vor diesem Bericht: `61aa5fc99 fix: brand daemon and wallet help text`

## Zweck dieses Berichts

Dieser Bericht beschreibt, was in PR #2 bisher prinzipiell aufgebaut wurde. Er ist kein Code-Review und keine Zeile-fuer-Zeile-Erklaerung, sondern eine fachlich-technische Zusammenfassung:

- wie QWC v2 als neue Chain startet,
- wie EPoSE v2 aufgebaut ist,
- wie Service Nodes registriert und qualifiziert werden,
- wie Rewards zwischen Miner und Service Nodes verteilt werden,
- was bereits getestet wurde,
- und was fuer eine echte Beta noch offen bleibt.

## Kurzfassung

PR #2 macht aus dem bisherigen Qwertycoin-v2-PoC eine erste EPoSE-faehige Testnet-Chain.

Die Chain startet nicht mehr wie eine historische Monero-Kette, die alte Hardforks nachtraeglich durchlaeuft. QWC v2 startet als neue Chain direkt mit QWC Protocol/HF17 ab Genesis. Dieses Protokoll erbt die aktuellen Monero-Regeln der 0.18.x-Basis und aktiviert zusaetzlich EPoSE.

RandomX/PoW bleibt weiterhin die Grundlage der Chain-Sicherheit. EPoSE ersetzt Mining nicht. EPoSE ist eine zusaetzliche, deterministische Service-Node-Schicht, die einen Teil des Block Rewards an qualifizierte Service Nodes auszahlen kann.

## QWC v2 Chain-Baseline

Die wichtigsten Chain-Entscheidungen in PR #2:

- QWC v2 ist eine neue Chain.
- Mainnet, Testnet und Stagenet starten direkt bei QWC Protocol/HF17 ab Height 0.
- Historische Monero-Hardforks v1-v15 werden nicht kuenstlich ueber spaetere Blockhoehen aktiviert.
- Die Hardfork-Infrastruktur bleibt erhalten, damit spaetere QWC-Upgrades als v18, v19 usw. sauber aktiviert werden koennen.
- Die Genesis-Initialisierung wurde auf den QWC-HF17-Start ausgerichtet.
- Wallet und Daemon verwenden dieselbe QWC-v2-Genesis-Logik.

Das Ziel ist: Ein neuer QWC-v2-Start soll von Anfang an moderne Consensus-/TX-Regeln verwenden, statt alte historische Uebergangsfenster nachzubauen.

## Was EPoSE v2 ist

EPoSE v2 ist eine Proof-of-Service-Erweiterung fuer QWC.

Die Idee:

1. Miner erzeugen weiter die Blockchain per PoW.
2. Service Nodes betreiben oeffentlich erreichbare Infrastruktur.
3. Service Nodes registrieren sich mit einem kryptografischen Service-Key und einer Reward-Adresse.
4. Andere Service Nodes koennen die Erreichbarkeit/Service-Qualitaet attestieren.
5. Nur qualifizierte Service Nodes koennen Service Rewards bekommen.
6. Die Auswahl des bezahlten Service Nodes pro Block ist deterministisch aus Chain-State ableitbar.

Wichtig: EPoSE basiert nicht auf einer zentralen Sentinel-Instanz als Reward-Autoritaet. Sentinel/Explorer koennen spaeter den Zustand anzeigen oder ueberwachen, aber die fuer Rewards relevante Wahrheit muss aus der Chain rekonstruierbar sein.

## Service-Node-Identitaet

Eine Service Node besteht im Consensus nicht primaer aus einer IP-Adresse, sondern aus einer kryptografischen Identitaet.

Eine Registrierung enthaelt im Prinzip:

- Service Public Key,
- Reward-Adresse,
- Endpoint-Commitment,
- Registration Epoch,
- Expiry Epoch,
- Admission Proof,
- Signatur.

Der private Service-Key bleibt lokal beim Betreiber. RPCs und Explorer duerfen nur oeffentliche Informationen anzeigen.

Die Registrierung wird ueber vorhandene Transaktionsdaten transportiert, konkret ueber `tx_extra_nonce`-Payloads. Dadurch muss nicht sofort ein komplett neues Transaktionsformat eingefuehrt werden.

## Admission Proof

Damit Service-Node-Identitaeten nicht komplett kostenlos gespammt werden koennen, gibt es im aktuellen Testnet-Stand einen einfachen identity-bound Admission Proof.

Der Proof ist gebunden an:

- Netzwerk,
- Service Public Key,
- Reward-Adresse,
- Endpoint-Commitment,
- Registration Epoch,
- Previous Epoch Hash,
- Nonce.

Aktuell ist das bewusst eine einfache, isolierte Testnet-Implementierung. Fuer Mainnet ist das noch kein finales Sybil-Resistance-Modell. Dort muss entschieden werden, ob der Admission Proof RandomX-gebunden oder durch ein anderes knappes, messbares Kostenmodell ersetzt wird.

## Epochs

EPoSE arbeitet in Epochs.

Aktuelle Konstanten:

- `EPOSE_EPOCH_LENGTH = 720` Bloecke
- `EPOSE_FINALITY_DEPTH = 60` Bloecke
- `EPOSE_REGISTRATION_TTL_EPOCHS = 30`
- `EPOSE_MIN_ATTESTATIONS = 2`
- `EPOSE_SERVICE_REWARD_BPS = 1000`

Bei 120 Sekunden Blockzeit entspricht eine Epoch ungefaehr 24 Stunden.

Der Finality-Depth von 60 Bloecken sorgt dafuer, dass Committee-/Seed-Entscheidungen nicht direkt aus dem reorg-anfaelligen Chain-Tip kommen.

## Attestations und Qualification

Eine registrierte Service Node ist nicht automatisch qualifiziert.

Eine Service Node gilt fuer eine Epoch als qualifiziert, wenn:

```text
registration_epoch <= epoch < expiry_epoch
und
unique_valid_attestations >= EPOSE_MIN_ATTESTATIONS
```

Aktuell bedeutet das:

- Eine Node muss registriert und aktiv sein.
- Sie darf noch nicht abgelaufen sein.
- Sie braucht mindestens 2 eindeutige gueltige Attestations.
- Self-Attestation zaehlt nicht.
- Doppelte Votes desselben Verifiers zaehlen nur einmal.
- Attestations von unregistrierten oder nicht zugewiesenen Verifiers werden abgelehnt.
- Attestations sind an Epoch, Challenge Hash, Subject, Verifier und beobachteten Chain-Zustand gebunden.

Praktische Folge: Mit nur einer Service Node kann `qualified_count` noch nicht sinnvoll steigen. Fuer Qualification braucht das Netzwerk mehrere registrierte Service Nodes, die sich gegenseitig pruefen koennen.

## Reward-Verteilung

EPoSE splittet den Block Reward inklusive Fees.

Aktuelle Testnet-Formel:

```text
total_reward = base_reward + transaction_fees
service_reward = floor(total_reward * EPOSE_SERVICE_REWARD_BPS / 10000)
miner_reward = total_reward - service_reward
```

Aktueller Testnet-Wert:

```text
EPOSE_SERVICE_REWARD_BPS = 1000
```

Das bedeutet im Testnet:

- 10% gehen an eine qualifizierte Service Node.
- 90% bleiben beim Miner.
- Fees werden in denselben Split einbezogen.

Beispiel:

```text
base_reward + fees = 100 QWC
service_reward = 10 QWC
miner_reward = 90 QWC
```

Dieser Wert ist ein Beta-/Testnet-Parameter. Er ist keine finale Mainnet-Tokenomics-Entscheidung.

## Was bekommt eine qualifizierte Service Node?

Pro Block wird maximal eine qualifizierte Service Node bezahlt.

Wenn fuer einen Block eine qualifizierte Payee-Node existiert:

- die Service Node bekommt aktuell 10% von `base_reward + fees`,
- der Miner bekommt den Rest,
- die Coinbase muss den exakten Service-Reward-Output enthalten,
- falscher Betrag, falscher Empfaenger oder fehlender Output machen den Block ungueltig.

Wenn keine Service Node qualifiziert ist:

- es gibt keinen Service-Reward-Output,
- der Miner bekommt den normalen vollen Block Reward nach bisherigem Miner-only-Pfad.

## Was bekommt der Miner?

Der Miner bekommt:

- 100% des Rewards, wenn keine qualifizierte Service Node fuer den Block existiert.
- 90% des Rewards, wenn eine qualifizierte Service Node fuer den Block existiert.

Der Miner bleibt also immer der Block-Produzent und bekommt immer den groessten Anteil. EPoSE fuegt nur einen deterministischen Service-Reward-Anteil hinzu.

## Wie wird bei mehreren Service Nodes aufgeteilt?

EPoSE zahlt nicht alle Service Nodes in jedem Block aus.

Stattdessen:

1. Fuer eine Epoch wird aus allen qualifizierten Service Nodes ein deterministisches Ranking gebildet.
2. Das Ranking basiert auf Epoch Seed und Service Public Key.
3. Pro Block wird daraus genau eine qualifizierte Service Node ausgewaehlt.
4. Die ausgewaehlte Node bekommt fuer diesen Block den Service-Reward-Anteil.
5. Die anderen qualifizierten Nodes bekommen in diesem Block nichts, bleiben aber fuer spaetere Bloecke im Rotationsset.

Prinzip:

```text
qualified_nodes = sort_by_hash(epoch_seed, service_public_key)
selected_node = qualified_nodes[height % qualified_nodes.size]
```

Dadurch bleibt die Coinbase klein und vorhersehbar. Bei vielen Service Nodes waechst die Zahl der Coinbase-Outputs nicht linear mit.

Ueber viele Bloecke verteilt sich der Service-Reward deterministisch ueber die qualifizierten Nodes. Es ist kein Zufall zur Laufzeit und keine zentrale Auswahl.

## Coinbase und Transparenz im aktuellen Testnet

Der aktuelle Service-Reward-Output ist bewusst consensus-sichtbar.

Grund: Klassische CryptoNote-Stealth-Outputs koennen von fremden Validatoren nicht einfach gegen eine konkrete Zieladresse verifiziert werden. Fuer die Testnet-Beta ist deshalb ein transparenter Service-Reward-Output eingebaut, damit jeder Node pruefen kann:

- stimmt der Betrag,
- stimmt der Empfaenger,
- stimmt die ausgewaehlte Service Node.

Das ist ein bewusster Testnet-Kompromiss und noch keine finale Mainnet-Privacy-/Tokenomics-Entscheidung.

## Operator-Flow

Der aktuelle Betreiber-Flow sieht so aus:

1. QWC v2 Daemon als Service Node starten:
   - `--service-node`
   - `--service-node-key`
   - `--service-reward-address`
   - `--service-node-advertise-address`
2. Daemon erzeugt oder laedt den lokalen Service-Key.
3. Wallet wird gegen diesen Daemon verbunden.
4. Wallet hat eine Testnet-Adresse und unlocked Balance.
5. Wallet fuehrt `register_service_node` aus.
6. Registration-TX wird gemined.
7. Node erscheint in `get_service_nodes`.
8. Nach gueltigen Attestations kann sie qualifiziert werden.
9. Sobald sie qualifiziert und fuer einen Block ausgewaehlt ist, bekommt sie den Service-Reward-Anteil.

Auf einem dritten Seed-Host ist dieser Flow bereits praktisch vorbereitet:

- nativer systemd-Service laeuft,
- Service-Key ist geladen,
- Testnet-Reward-Wallet wurde erzeugt,
- Node ist lokal als Service Node aktiv,
- On-chain-Registration braucht geminte/unlocked Testnet-Coins.

## Daemon, Wallet und RPC

PR #2 hat die fuer Beta-Tester sichtbaren Namen bereinigt:

- `qwertycoind`
- `qwertycoin-wallet-cli`
- `qwertycoin-wallet-rpc`

Rueckwaertskompatible Symlinks fuer alte Monero-Namen bleiben im Docker-Image noch vorhanden, damit bestehende Skripte nicht sofort brechen.

Neue bzw. relevante EPoSE-RPCs:

- `get_epose_info`
- `get_service_nodes`
- `get_service_node_status`
- `get_epose_epoch`
- `get_service_rewards`
- `get_service_node_registration_payload`

Diese RPCs liefern oeffentlichen Zustand wie Epoch, Service-Node-Liste, Qualification, Reward-Payee und deterministischen State Hash. Private Service Keys werden nicht ausgegeben.

## Explorer

Der Explorer wurde separat ergaenzt, damit er EPoSE sichtbar macht:

- QWC/EPoSE-Dashboard,
- Service-Node-Anzahl,
- Qualified Count,
- Reward-Basis,
- State Hash,
- Service-Node-Tabelle.

Das ist ein eigener Explorer-PR und nicht Teil von PR #2 im Core-Repo, aber operativ wichtig fuer die Beta.

## Tests und Harness

PR #2 enthaelt deutlich mehr als nur Codepfade. Es wurden auch fokussierte Tests und ein lokaler Multi-Node-Harness aufgebaut.

Abgedeckt sind unter anderem:

- EPoSE-Serialisierung und Parser,
- Registration-Signaturen,
- Admission-Proof-Binding,
- Attestation-Signaturen,
- Duplicate-Attestation-Rejection,
- unregistrierte oder falsch zugewiesene Verifier,
- State Hash,
- Restart-/Rebuild-Verhalten,
- Reorg-Verhalten fuer Registrierungen,
- Reward-Split,
- Payee-Rotation,
- Genesis/HF17-Start,
- Wallet-Registration-Flow,
- Daemon-CLI-Kommandos,
- Docker-/Harness-Generierung fuer mehrere Nodes.

Der Harness kann lokale 3/5/10/25-Node-Konfigurationen erzeugen und prueft unter anderem:

- gleiche Genesis,
- gleiche Chain-Hoehe,
- gleiche Top-Hashes,
- gleiche EPoSE-State-Hashes,
- Restart-Persistenz,
- Partition/Heal-Szenarien,
- mined registration flows.

## Live-Infrastruktur

Im Verlauf von PR #2 wurden reale Testnet-Hosts genutzt:

- `seed host A`
- `seed host B`
- `seed host C`

Wichtige Ports:

- P2P: `8196`
- Daemon RPC: `8197`
- Wallet RPC: `8198`
- ZMQ RPC: `8199`

RPC bleibt fuer echte Nodes grundsaetzlich lokal gebunden. Von aussen soll primaer P2P erreichbar sein. Fuer Explorer wurden nur eng begrenzte read-only EPoSE-Endpunkte geproxyt.

## Was PR #2 bewusst noch nicht final loest

PR #2 ist noch keine Mainnet-Freigabe.

Bekannte offene Punkte:

- finaler Mainnet-Admission-Proof gegen Sybil-Angriffe,
- finale Mainnet-Reward-Parameter,
- vollstaendige lange Multi-Node-Qualification mit ausreichend echten Service Nodes,
- 25+ Node-Test auf geeigneter Hardware,
- laengere Fuzz-/Chaos-/Security-Laeufe,
- CI/GitHub-Actions muessen wieder echte Logs und gruene Checks liefern,
- finale Genesis-Artefakte fuer Beta/Mainnet,
- sauberer Release-/Operator-Flow,
- Entscheidung, ob transparente Service-Rewards fuer Mainnet akzeptabel sind oder anders geloest werden muessen.

## Beta-Einschaetzung

Der aktuelle Stand ist geeignet fuer eine fruehe Testnet-Beta-Vorbereitung:

- Chain startet,
- Daemon und Wallet bauen,
- Service-Node-Modus startet,
- Registrierung ist als Flow vorhanden,
- EPoSE-Zustand ist per RPC sichtbar,
- Reward-Logik ist deterministisch modelliert,
- Explorer zeigt EPoSE-Daten.

Nicht geeignet ist der Stand als Mainnet-Beta. Dafuer fehlen noch wirtschaftliche Finalisierung, Sybil-Hardening, lange Netzwerklaeufe und eine saubere CI-/Release-Kette.

## Entscheidende Kurzantwort zur Reward-Verteilung

Im aktuellen PR-Stand gilt fuer das Testnet:

```text
Keine qualifizierte Service Node:
  Miner bekommt 100%

Mindestens eine qualifizierte Service Node:
  ausgewaehlte Service Node bekommt 10%
  Miner bekommt 90%

Mehrere qualifizierte Service Nodes:
  pro Block bekommt genau eine Node die 10%
  Auswahl erfolgt deterministisch per epoch-seeded Rotation
  die anderen qualifizierten Nodes warten auf ihre naechsten Slots
```

Das ist der aktuelle technische Beta-Mechanismus, nicht die finale Mainnet-Tokenomics.
