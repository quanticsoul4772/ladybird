# Ladybird Browser - Privacy & Security Research Fork

A privacy-focused experimental fork of [Ladybird Browser](https://github.com/LadybirdBrowser/ladybird) implementing advanced threat detection, credential protection, and decentralized web protocols.

> **Educational Research Project**: This fork is for learning and security research. Not audited for production use.

---

## Features

### Sentinel Security System

Local threat detection daemon with zero cloud dependencies.

**Malware Detection**:
- YARA signature scanning (9 builtin rules + custom rule support)
- ML-based detection using TensorFlow Lite
- WASM + nsjail sandbox for behavioral analysis
- Quarantine system with AES-256 encryption
- VirusTotal and AlienVault OTX threat intelligence integration
- Automatic YARA rule generation from threat feeds

**Credential Protection**:
- Real-time form monitoring for cross-origin credential submissions
- PolicyGraph database for trusted relationships
- Autofill protection for untrusted forms

**Browser Fingerprinting Detection**:
- 27 API hooks across Canvas, WebGL, Audio, and Navigator APIs
- Aggressiveness scoring (0.0-1.0 scale)
- Real-time alerts for tracking attempts

**Phishing Detection**:
- Unicode homograph detection
- Typosquatting analysis
- Suspicious TLD detection
- Domain entropy analysis

**Network Behavioral Analysis**:
- C2 beaconing detection
- Data exfiltration monitoring
- DGA (Domain Generation Algorithm) detection
- DNS tunneling detection

**Network Isolation**:
- Per-process network isolation using iptables/nftables
- Process monitoring and automatic rule cleanup
- Prevents malicious processes from network access

### Network Privacy

**Tor Integration**:
- Per-tab circuit isolation
- SOCKS5H proxy with DNS leak prevention
- Stream isolation between tabs

**IPFS/IPNS Support**:
- Decentralized content delivery
- CID cryptographic verification
- Multi-gateway fallback
- Local daemon integration

**ENS Resolution**:
- Ethereum Name Service support
- Human-readable .eth domains
- Gateway integration (eth.limo, eth.link)

**DNS Privacy**:
- DNS-over-TLS (default enabled)
- DNSSEC validation
- DNS leak prevention via SOCKS5H

---

## Quick Start

```bash
# Clone
git clone https://github.com/quanticsoul4772/ladybird.git
cd ladybird

# Build
cmake --preset Release
cmake --build Build/release -j$(nproc)

# Run
./Build/release/bin/Ladybird
```

Platform Support: Linux (full), macOS/Windows (partial - Sentinel features degrade gracefully)

Detailed build instructions: [BuildInstructionsLadybird.md](Documentation/BuildInstructionsLadybird.md)

---

## Usage

### Security Features

```bash
# Sentinel starts automatically with browser
./Build/release/bin/Ladybird

# Security dashboard
Navigate to: about:security

# View quarantined files
ls ~/.local/share/Ladybird/Quarantine/

# Custom YARA rules
mkdir -p ~/.local/share/Ladybird/yara-rules
cp my-rules.yar ~/.local/share/Ladybird/yara-rules/
```

### Network Privacy

**Tor .onion sites**:
```
http://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion
```

**IPFS content**:
```
ipfs://QmXoypizjW3WknFiJnKLwHCnL72vedxjQkDDP1mXWo6uco
ipns://docs.ipfs.tech
```

**ENS domains**:
```
https://vitalik.eth
https://uniswap.eth
```

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Browser UI Process                                  │
│  ├─ SecurityAlertDialog (Threat/Credential Alerts)  │
│  └─ about:security Dashboard                        │
└────────────────┬────────────────────────────────────┘
                 │ IPC
┌────────────────┴────────────────────────────────────┐
│  WebContent Process (per tab, sandboxed)            │
│  ├─ FormMonitor (Credential Protection)             │
│  └─ FingerprintingDetector (Tracking Detection)     │
└────────────────┬────────────────────────────────────┘
                 │ IPC
┌────────────────┴────────────────────────────────────┐
│  RequestServer Process                               │
│  ├─ SecurityTap (YARA Malware Scanning)            │
│  ├─ URLSecurityAnalyzer (Phishing Detection)        │
│  ├─ C2Detector (Beaconing Detection)                │
│  └─ TrafficMonitor (Network Behavioral Analysis)    │
└────────────────┬────────────────────────────────────┘
                 │ Unix Socket
┌────────────────┴────────────────────────────────────┐
│  Sentinel Server (Daemon)                           │
│  ├─ YARA Rule Engine                                │
│  ├─ ML Malware Detector (TensorFlow Lite)          │
│  ├─ Behavioral Sandbox (WASM + nsjail)             │
│  ├─ PolicyGraph (SQLite + LRU Cache)                │
│  ├─ ThreatFeed (Bloom Filter)                       │
│  ├─ ThreatIntelligence (VirusTotal, OTX)           │
│  ├─ QuarantineManager (AES-256 encryption)          │
│  ├─ NetworkIsolation (iptables/nftables)           │
│  └─ AuditLogger                                      │
└─────────────────────────────────────────────────────┘
```

---

## Documentation

### User Guides
- [Malware Scanning](docs/SENTINEL_USER_GUIDE.md)
- [Credential Protection](docs/USER_GUIDE_CREDENTIAL_PROTECTION.md)
- [Network Monitoring](docs/USER_GUIDE_NETWORK_MONITORING.md)
- [Policy Management](docs/SENTINEL_POLICY_GUIDE.md)

### Technical Documentation
- [Sentinel Architecture](docs/SENTINEL_ARCHITECTURE.md)
- [Fingerprinting Detection](docs/FINGERPRINTING_DETECTION_ARCHITECTURE.md)
- [Phishing Detection](docs/PHISHING_DETECTION_ARCHITECTURE.md)
- [TensorFlow Lite Integration](docs/TENSORFLOW_LITE_INTEGRATION.md)
- [Sandbox Architecture](docs/SANDBOX_ARCHITECTURE.md)

### Development
- [CLAUDE.md](CLAUDE.md) - Developer guide
- [docs/FORK.md](docs/FORK.md) - Fork overview
- [docs/FEATURES.md](docs/FEATURES.md) - Feature catalog
- [Changelog](docs/CHANGELOG.md)

---

## For Developers

See [CLAUDE.md](CLAUDE.md) for:
- Build presets and debugging
- Testing procedures (Playwright + LibWeb)
- Code architecture and patterns
- Sentinel development guidelines
- IPC message catalog

Run tests:
```bash
# Playwright tests
cd Tests/Playwright
npm test

# LibWeb tests
./Meta/ladybird.py test LibWeb
```

---

## About Ladybird

This fork builds on [Ladybird](https://ladybird.org), a web browser built from scratch with its own rendering and JavaScript engines. Ladybird is pre-alpha software under active development.

### Core Libraries

- LibWeb - Web rendering engine
- LibJS - JavaScript engine (ECMAScript)
- LibWasm - WebAssembly implementation
- LibCrypto/LibTLS - Cryptography and TLS
- LibHTTP - HTTP/1.1 client
- LibGfx - 2D graphics and image decoding
- LibCore - Event loop and OS abstraction
- LibIPC - Inter-process communication

### Fork Additions

- Services/Sentinel/ - Security daemon
  - Sandbox/ - WASM + nsjail behavioral analysis, ThreatReporter
  - ThreatIntelligence/ - VirusTotal, OTX feeds, YARA generator
  - Quarantine/ - Encrypted quarantine with AES-256
  - NetworkIsolation/ - iptables/nftables process isolation
- FormMonitor - Credential protection
- FingerprintingDetector - Tracking detection
- SecurityTap - YARA integration
- PolicyGraph - Security policy database with LRU cache
- NetworkIdentity - Per-tab network isolation

---

## Upstream Participation

Join [Ladybird's Discord server](https://discord.gg/nvfjVJ4Svh) to participate in upstream development.

Contributing to upstream: Read [Getting Started Contributing](Documentation/GettingStartedContributing.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

Note: Fork-specific features (Sentinel, network privacy) are not intended for upstream contribution. This is a personal learning fork.

---

## Disclaimer

This fork is for educational and research purposes:
- Not security-audited
- May contain bugs
- Not production-ready

For production use, visit the official [Ladybird Browser](https://github.com/LadybirdBrowser/ladybird).

---

## License

Ladybird is licensed under a 2-clause BSD license. This fork maintains the same license.

See [LICENSE](LICENSE) for details.

---

## Links

- Upstream Ladybird: https://github.com/LadybirdBrowser/ladybird
- This Fork: https://github.com/quanticsoul4772/ladybird
- Discord: https://discord.gg/nvfjVJ4Svh
- Website: https://ladybird.org
