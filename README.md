# Ladybird Browser - Privacy & Security Research Fork

[![Milestones](https://img.shields.io/badge/Milestones-4%2F4%20Complete-success)](docs/MILESTONE_0.5_PLAN.md) [![Tests](https://img.shields.io/badge/Tests-4046%20passing-brightgreen)](#testing-and-quality-assurance) [![Documentation](https://img.shields.io/badge/Documentation-127%20files-blue)](docs/)

A privacy-focused experimental fork of [Ladybird Browser](https://github.com/LadybirdBrowser/ladybird) implementing advanced threat detection, credential protection, and decentralized web protocols.

> **Educational Research Project**: This fork is for learning and security research. Not audited for production use. See [Fork Disclaimer](#fork-disclaimer).

---

## What Makes This Fork Different

This fork implements ~32,000 lines of custom C++ code for security research:

- Multi-layered Malware Detection - YARA signatures + ML heuristics + behavioral sandboxing
- Credential Exfiltration Protection - Real-time form monitoring with PolicyGraph
- Browser Fingerprinting Detection - Tracks 6 techniques across 27 API hooks
- Phishing URL Analysis - Unicode homographs, typosquatting, domain entropy
- Network Privacy - Tor integration, IPFS/IPNS, ENS resolution
- Behavioral Analysis - WASM + nsjail sandbox for unknown threats
- Network Monitoring - C2 beaconing, data exfiltration, DGA detection

Development Status: 4 major milestones complete (0.1-0.4), Milestone 0.5 Phase 1 operational.

---

## Quick Start

```bash
# Clone
git clone https://github.com/quanticsoul4772/ladybird.git
cd ladybird

# Build (Release mode)
cmake --preset Release
cmake --build Build/release -j$(nproc)

# Run
./Build/release/bin/Ladybird
```

Platform Support: Linux (full), macOS/Windows (partial - Sentinel features degrade gracefully)

Detailed platform-specific instructions: [BuildInstructionsLadybird.md](Documentation/BuildInstructionsLadybird.md)

### For Developers

See [CLAUDE.md](CLAUDE.md) for comprehensive developer guide including:
- Build presets and debugging
- Testing procedures (Playwright + LibWeb)
- Code architecture and patterns
- Sentinel development guidelines
- IPC message catalog

---

## Sentinel Security System

Sentinel is a standalone security daemon providing local threat detection with zero cloud dependencies.

### Core Architecture

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
│  ├─ YARA Rule Engine (40% weight)                   │
│  ├─ ML Malware Detector - TFLite (35% weight)       │
│  ├─ Behavioral Sandbox - WASM + nsjail (25% weight) │
│  ├─ PolicyGraph (SQLite + LRU Cache)                │
│  ├─ ThreatFeed (Bloom Filter - 100M hashes)         │
│  └─ AuditLogger (Forensics & Compliance)            │
└─────────────────────────────────────────────────────┘
```

Key Characteristics:
- Privacy-First: All processing local (no cloud APIs)
- Multi-Process: Sandboxed components for isolation
- Graceful Degradation: Browser continues if security fails
- Zero-Copy: Shared memory for performance
- Async Operations: Thread pool for parallel scanning

### Features by Milestone

#### Milestone 0.1 - Malware Scanning

- YARA-based Detection: 9 builtin rules + custom rule support
- Quarantine System: Secure file isolation (`~/.local/share/Ladybird/Quarantine/`)
- PolicyGraph Database: SQLite persistence for user decisions
- SecurityAlertDialog: User action workflow (Block/Allow/Quarantine)
- about:security Dashboard: Policy management UI

Components: `Services/Sentinel/`, `Services/RequestServer/SecurityTap.cpp`

#### Milestone 0.2 - Credential Protection

- FormMonitor: Real-time cross-origin credential detection
- Trusted Relationships: Persistent allow/block lists
- Autofill Protection: Blocks password autofill for untrusted forms
- One-Time Override: "Allow Once" without creating policy
- Alert Types: Exfiltration (critical), Insecure HTTP (high), Tracking (medium)

Components: `Services/WebContent/FormMonitor.{h,cpp}`

#### Milestone 0.3 - Enhanced Protection

- Database Schema v3: credential_relationships, policy_templates tables
- Import/Export: JSON backup/restore for trust lists
- Policy Templates: 5 builtin templates (banking, social, email, etc.)
- Form Anomaly Detection: 4-factor scoring (hidden fields, field count, domain rep, frequency)

Components: Extended PolicyGraph with 21 new API methods

#### Milestone 0.4 - Advanced Detection

**Phase 1: ML Infrastructure**
- Feature extraction: 6-dimensional vectors (entropy, PE headers, imports, strings, code ratio)
- TensorFlow Lite integration with heuristic fallback
- Performance: ~1ms inference time

**Phase 2: ML Malware Detection**
- Shannon entropy for packed malware (>7.5 suspicious)
- PE structure analysis (header anomalies, import table size)
- Suspicious string detection (URLs, IPs, registry keys)
- Confidence scoring with explainability

**Phase 3: Federated Threat Intelligence**
- Bloom Filter: 100M threat hashes (143 MB, 0.1% false positive rate)
- IPFS threat feed sync (decentralized P2P sharing)
- Federated Learning: Privacy-preserving ML updates (ε=0.1 differential privacy)

**Phase 4: Browser Fingerprinting Detection**
- 27 API Hooks:
  - Canvas: toDataURL(), getImageData()
  - WebGL: 12 hooks (getParameter, getSupportedExtensions, etc.)
  - Audio: 4 hooks (OscillatorNode, AnalyserNode)
  - Navigator: 6 hooks (userAgent, platform, plugins, etc.)
- Aggressiveness Scoring: 0.0-1.0 scale with multi-factor weighting
- Real-time Alerts: Threshold > 0.6 triggers user notification

**Phase 5: Phishing URL Analysis**
- Unicode homograph detection (ICU spoofchecker)
- Levenshtein distance to 100 popular domains
- Suspicious TLD detection (free/unverified TLDs)
- Domain entropy analysis (random string detection)
- Human-readable explanations

**Phase 6: Network Behavioral Analysis**
- DGA Detection: Entropy + consonant ratio + n-gram analysis
- C2 Beaconing: Coefficient of Variation (CV < 0.2 = beaconing)
- Data Exfiltration: Upload ratio analysis (>80% suspicious)
- DNS Tunneling: Query depth + base64 pattern detection
- Performance: 33,333 requests/second throughput

#### Milestone 0.5 - Active Defense (Phase 1 Complete)

**Phase 1: Real-time Sandboxing**
- Tier 1 - WASM Sandbox: Wasmtime executor (<100ms analysis)
  - Host imports: logging, timestamps
  - Timeout enforcement (epoch interruption)
  - Resource limits: fuel + memory + CPU time
  - Real WASM module execution (2.7 KB Rust-compiled binary)
- Tier 2 - Native Sandbox: nsjail behavioral analyzer (planned)
- Orchestrator: Multi-tier analysis coordination
- VerdictEngine: Weighted score aggregation (YARA 40%, ML 35%, Behavioral 25%)
- Verdict Caching: PolicyGraph persistence for performance

Status: RequestServer integration complete, all 10/10 tests passing

**Remaining Phases** (Planned):
- Phase 2: Production ML Models (TensorFlow Lite training pipeline)
- Phase 3: Active Fingerprinting Mitigation (canvas noise, WebGL spoofing)
- Phase 4: SIEM Integration (Syslog/CEF export, threat correlation)

### Detection Techniques (15 Total)

**Static Analysis** (3):
1. YARA Signature Matching
2. ML Feature Extraction (6 features)
3. URL Analysis (homographs, typosquatting, TLD, entropy)

**Dynamic Analysis** (2):
4. WASM Sandbox (JavaScript/binary pre-analysis)
5. Native Sandbox (OS-level syscall monitoring - planned)

**Behavioral Analysis** (4):
6. Form Submission Monitoring
7. Browser Fingerprinting Detection (6 techniques)
8. C2 Beaconing Detection
9. DNS Analysis (DGA, tunneling)

**Network Analysis** (3):
10. Data Exfiltration Detection
11. DNS Tunneling Detection
12. Phishing URL Detection

**Intelligence Integration** (3):
13. Bloom Filter (100M threat hashes)
14. IPFS Threat Sync (decentralized)
15. Federated Learning (privacy-preserving)

### Usage

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

User Workflow:
1. Download triggers: YARA + ML + Sandbox scan
2. Threat detected: SecurityAlertDialog appears
3. User chooses: Block / Allow Once / Always Allow / Quarantine
4. Decision stored in PolicyGraph (future auto-enforcement)
5. View history at `about:security`

### Documentation

**User Guides**:
- [Malware Scanning User Guide](docs/SENTINEL_USER_GUIDE.md) - End-user documentation
- [Credential Protection Guide](docs/USER_GUIDE_CREDENTIAL_PROTECTION.md) - Form monitoring
- [Network Monitoring Guide](docs/USER_GUIDE_NETWORK_MONITORING.md) - Behavioral analysis
- [Policy Management](docs/SENTINEL_POLICY_GUIDE.md) - Trust/block list management
- [YARA Rules Guide](docs/SENTINEL_YARA_RULES.md) - Custom rule authoring

**Technical Documentation**:
- [Sentinel Architecture](docs/SENTINEL_ARCHITECTURE.md) (2,270 lines) - System design
- [Fingerprinting Detection](docs/FINGERPRINTING_DETECTION_ARCHITECTURE.md) - Technical spec
- [Phishing Detection](docs/PHISHING_DETECTION_ARCHITECTURE.md) - Algorithm details
- [TensorFlow Lite Integration](docs/TENSORFLOW_LITE_INTEGRATION.md) - ML setup
- [Sandbox Architecture](docs/SANDBOX_ARCHITECTURE.md) - WASM + nsjail design

**Milestone Plans**:
- [Milestone 0.5 Plan](docs/MILESTONE_0.5_PLAN.md) - Current roadmap
- [Milestone 0.4 Technical Specs](docs/MILESTONE_0.4_TECHNICAL_SPECS.md) - Advanced detection
- [Changelog](docs/CHANGELOG.md) - Development history

---

## Network Privacy Features

### Tor Integration

Per-tab Tor support with DNS leak prevention and circuit isolation.

```javascript
// Each tab gets unique Tor circuit
Tab 1: circuit-12345-a3f8c2@localhost:9050
Tab 2: circuit-67890-b4e9d2@localhost:9050
// Complete traffic isolation between tabs
```

Features:
- SOCKS5H Proxy: DNS resolution via Tor (prevents ISP tracking)
- Stream Isolation: Unique circuit per tab prevents correlation
- Circuit Rotation: Fresh exit node on demand
- Automatic Detection: Checks if Tor daemon is running

Integration: `Libraries/LibIPC/NetworkIdentity.{h,cpp}`, `Services/RequestServer/Request.cpp:272-286`

Testing:
```
# DuckDuckGo .onion address
http://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion
```

### IPFS/IPNS Support

Decentralized content delivery with cryptographic verification.

Protocol Handlers:
- `ipfs://QmXoypizjW3WknFiJnKLwHCnL72vedxjQkDDP1mXWo6uco` - Content-addressed
- `ipns://docs.ipfs.tech` - Mutable pointers (DNSLink)

Features:
- CID Verification: Cryptographic content integrity (hash validation)
- Multi-Gateway Fallback: 4 IPFS gateways (local + 3 public)
- Local Daemon Integration: Direct connection to local IPFS node
- Gateway Transformation: Automatic HTTP translation

Integration: `Services/RequestServer/ConnectionFromClient.cpp:1140-1475`

Testing:
```
ipfs://QmXoypizjW3WknFiJnKLwHCnL72vedxjQkDDP1mXWo6uco
ipns://docs.ipfs.tech
```

### ENS Resolution

Ethereum Name Service for human-readable `.eth` domains.

Features:
- ENS gateway integration (eth.limo, eth.link)
- Blockchain-based domain resolution
- IPFS content hosting via ENS
- Automatic fallback between gateways

Testing:
```
https://vitalik.eth
https://uniswap.eth
```

### DNS Privacy

- DNS-over-TLS: Encrypted DNS queries (default enabled)
- DNSSEC: Local cryptographic validation
- DNS Leak Prevention: SOCKS5H routes DNS through Tor

Implementation: `Services/RequestServer/Resolver.h:24`

### Audit Trail

Every network request is logged per NetworkIdentity:
- Request URL and method
- Response status code
- Bytes sent/received
- Timestamp
- Full history (max 1000 entries per tab)

Stats Available: Total requests, data usage, session age

---

## Testing and Quality Assurance

### Automated Test Suite

Total: 4,046 tests across multiple frameworks

| Framework | Tests | Status |
|-----------|-------|--------|
| Playwright E2E | 338 | Passing |
| LibWeb Unit | 3,708 | Passing |

### Test Categories

Playwright Tests (338 total):
- Security (75 tests) - FormMonitor, fingerprinting, malware, phishing, network monitoring
- Rendering (80 tests) - HTML, CSS, JavaScript, responsive design
- JavaScript (62 tests) - ES6, async, DOM, events, Web APIs
- Core Browser (45 tests) - Navigation, tabs, history, bookmarks
- Forms (42 tests) - Input, validation, submission, advanced handling
- Accessibility (22 tests) - ARIA, semantic HTML, keyboard navigation
- Performance (15 tests) - Benchmarks, overhead analysis
- Multimedia (14 tests) - Audio, video, canvas
- DOM (12 tests) - MutationObserver, IntersectionObserver, CustomElements
- Web APIs (8 tests) - LocalStorage, SessionStorage
- Network (6 tests) - Headers, Fetch API
- Edge Cases (18 tests) - Boundary conditions, error handling

Security Test Highlights:
- `tests/security/form-monitor.spec.ts` - 12 tests (credential protection)
- `tests/security/fingerprinting.spec.ts` - 12 tests (tracking detection)
- `tests/security/malware-detection.spec.ts` - 10 tests (YARA + ML)
- `tests/security/phishing-detection.spec.ts` - 10 tests (URL analysis)
- `tests/security/policy-graph.spec.ts` - 15 tests (database operations)
- `tests/security/network-monitoring.spec.ts` - 10 tests (C2, exfiltration, DGA)

### Test Infrastructure

HTTP Test Server (`test-server.js`):
- 3 ports: 9080 (HTTP), 9081 (cross-origin), 9443 (HTTPS)
- 20+ endpoints: forms, OAuth, malware, navigation, cookies
- CORS enabled for cross-origin testing
- Graceful shutdown handling

Test Fixtures (36 files):
- Malware samples (EICAR, ML-suspicious binaries)
- Phishing pages (homographs, typosquatting)
- Form tests (legitimate, phishing, OAuth)
- YARA rules (9 detection signatures)

Helper Libraries:
- `form-monitor-helpers.ts` (524 lines) - Credential testing utilities
- `phishing-test-utils.ts` - URL analysis helpers
- `accessibility-test-utils.ts` - A11Y validation

### Running Tests

```bash
# Playwright E2E tests
cd Tests/Playwright
npm install
npm test                          # All tests
npm run test:p0                   # Critical tests (182)
npx playwright test --ui          # Interactive mode
npx playwright test --headed      # See browser

# Run specific category
npx playwright test tests/security/  # Security tests
npx playwright test tests/forms/     # Form tests

# Run single test
npx playwright test -g "FMON-001"    # By test ID
npx playwright test -g "Cross-origin password submission"  # By name

# LibWeb tests
./Meta/ladybird.py test LibWeb
./Meta/ladybird.py run test-web -- -f Text/input/your-test.html

# Start test server
cd Tests/Playwright
npm run servers  # Starts all 3 origins
```

### Test Documentation

264 documentation files (~15,000 lines):
- Test catalogs and indices
- Quick start guides
- Implementation summaries
- Troubleshooting guides
- Manual test checklists

See: [Tests/Playwright/](Tests/Playwright/) for complete test documentation.

---

## Development Status

### Milestones

| Milestone | Status | Features | Lines of Code |
|-----------|--------|----------|---------------|
| 0.1 - Malware Scanning | Complete | YARA, PolicyGraph, Quarantine | ~5,000 |
| 0.2 - Credential Protection | Complete | FormMonitor, Autofill Protection | ~2,500 |
| 0.3 - Enhanced Protection | Complete | Import/Export, Templates, Anomaly Detection | ~3,000 |
| 0.4 - Advanced Detection | Complete | ML, Fingerprinting, Phishing, Network Monitoring | ~10,000 |
| 0.5 - Active Defense | Phase 1 Complete | WASM Sandbox, nsjail (planned) | ~8,000 |
| 0.6 - Production Ready | Planned | SIEM, Active Mitigation, Zero-Day Detection | TBD |

Total Custom Code: ~32,000 lines of C++ (fork-specific)

### Feature Completion

Production-Ready:
- YARA malware scanning
- Credential exfiltration detection
- Browser fingerprinting detection (27 hooks)
- Phishing URL analysis
- Network behavioral analysis
- PolicyGraph database (v4 schema)
- WASM sandbox (Tier 1)
- Import/export workflows
- Security dashboard

In Progress:
- Native sandbox (Tier 2 - nsjail)
- Production ML models
- Active fingerprinting mitigation

Planned:
- SIEM integration
- Zero-day exploit detection
- Enhanced privacy dashboard
- Mobile platform support

### Code Statistics

- Sentinel Core: 19,048 lines
- Modified Services: 10,285 lines
- LibWeb Hooks: ~200 lines (minimal)
- UI/Qt: ~1,500 lines
- Sandbox WASM: ~800 lines (Rust)
- Tests: 10,418 lines (Playwright) + 3,708 files (LibWeb)
- Documentation: 127 files (~15,000 lines)

---

## Documentation

### Quick References
- [CLAUDE.md](CLAUDE.md) - Developer guide (509 lines)
- [docs/FORK.md](docs/FORK.md) - Fork overview
- [docs/FEATURES.md](docs/FEATURES.md) - Feature catalog
- [Tests/Playwright/](Tests/Playwright/) - Test documentation (264 files)

### Architecture
- [Sentinel Architecture](docs/SENTINEL_ARCHITECTURE.md) (2,270 lines)
- [Behavioral Analyzer Design](docs/BEHAVIORAL_ANALYZER_DESIGN.md) (1,628 lines)
- [Sandbox Architecture](docs/SANDBOX_ARCHITECTURE.md)
- [Network Privacy Architecture](docs/) - See IPFS/Tor docs

### User Guides
- [Malware Scanning](docs/SENTINEL_USER_GUIDE.md)
- [Credential Protection](docs/USER_GUIDE_CREDENTIAL_PROTECTION.md)
- [Network Monitoring](docs/USER_GUIDE_NETWORK_MONITORING.md)
- [Policy Management](docs/SENTINEL_POLICY_GUIDE.md)

### Development
- [Milestone 0.5 Plan](docs/MILESTONE_0.5_PLAN.md) (612 lines)
- [Milestone 0.4 Technical Specs](docs/MILESTONE_0.4_TECHNICAL_SPECS.md)
- [Testing Guide](docs/TESTING.md)
- [Changelog](docs/CHANGELOG.md)

---

## About Ladybird

[Ladybird](https://ladybird.org) is a truly independent web browser using a novel engine based on web standards. Ladybird is in pre-alpha and suitable only for developers.

### Core Libraries (Inherited from SerenityOS)

- LibWeb - Web rendering engine
- LibJS - JavaScript engine (ECMAScript)
- LibWasm - WebAssembly implementation
- LibCrypto/LibTLS - Cryptography and TLS
- LibHTTP - HTTP/1.1 client
- LibGfx - 2D graphics and image decoding
- LibUnicode - Unicode and locale support
- LibMedia - Audio and video playback
- LibCore - Event loop and OS abstraction
- LibIPC - Inter-process communication

### Fork Enhancements

- Services/Sentinel/ - Security daemon (69 files, 19,048 lines)
- FormMonitor - Credential protection
- FingerprintingDetector - Tracking detection
- SecurityTap - YARA integration
- PolicyGraph - Security policy database
- NetworkIdentity - Per-tab network isolation
- Sandbox/ - WASM + nsjail behavioral analysis

---

## Upstream Participation

Join [Ladybird's Discord server](https://discord.gg/nvfjVJ4Svh) to participate in upstream development.

Contributing to upstream: Read [Getting Started Contributing](Documentation/GettingStartedContributing.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

Reporting issues: See [issue policy](CONTRIBUTING.md#issue-policy) and [issue guidelines](ISSUES.md).

Note: Fork-specific features (Sentinel, network privacy) are not intended for upstream contribution. This is a personal learning fork.

---

## Fork Disclaimer

This fork is for educational and research purposes:

Limitations:
- Not security-audited - Use at your own risk
- May contain bugs - Experimental implementations
- Not production-ready - Testing and learning environment

Privacy characteristics:
- All processing local (no cloud dependencies)
- Graceful degradation (browser continues working if security features fail)
- 4,046 automated tests

For production use, visit the official [Ladybird Browser](https://github.com/LadybirdBrowser/ladybird).

Threat Model: This fork provides defense-in-depth against common web threats (malware, credential theft, fingerprinting, phishing). It does not protect against:
- Zero-day browser exploits
- Nation-state adversaries
- Advanced persistent threats (APTs)
- Physical access attacks
- Social engineering

Privacy Guarantees:
- All threat detection is local (no external APIs)
- No telemetry or data collection
- Optional federated learning uses differential privacy (ε=0.1)
- Audit logs are local-only

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

---

Last Updated: 2025-11-02
Fork Version: Milestone 0.5 (Phase 1 Complete)
Upstream Sync: Current with master (as of 2025-11-02)
