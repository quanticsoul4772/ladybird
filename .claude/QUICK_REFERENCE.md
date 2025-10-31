# Quick Reference - Claude Code Agent System

## Slash Commands

```bash
/architect          # Architecture design & technical decisions
/security          # Security analysis & threat modeling
/cpp-implement     # C++ implementation across all libraries
/fuzzer            # Fuzzing infrastructure & crash analysis
/networking        # Network stack, Tor integration
/reviewer          # Code review & quality assurance
/tester            # Test strategy & implementation
/docs              # Documentation creation
/orchestrator      # Coordinate multiple agents
/feature-workflow  # Complete feature development workflow
```

## MCP Tools Available

### Brave Search
```bash
"Use brave-search to find CVEs related to browser IPC"
"Search for how Chromium handles stream isolation"
```

### Unified-Thinking
```bash
"Use unified-thinking:decompose-problem to break down this feature"
"Use unified-thinking:analyze-temporal for short vs long-term implications"
"Use unified-thinking:build-causal-graph for attack chain analysis"
"Use unified-thinking:make-decision with weighted criteria"
"Use unified-thinking:analyze-perspectives from security/performance viewpoints"
```

### Memory
```bash
"Store this architectural decision in memory"
"Retrieve previous decisions about IPC design"
```

## Common Patterns

### Quick Architecture Question
```
/architect How should we handle video decoding - new process or extend ImageDecoder?
```

### Security Audit
```
/security Audit Services/WebContent/ConnectionFromClient.cpp for IPC vulnerabilities
```

### Implement Feature
```
/cpp-implement Add rate limiting to IPC message handlers using token bucket algorithm
```

### Full Workflow
```
/feature-workflow Implement per-tab Tor circuit isolation with stream isolation
```

### Coordinated Team
```
/orchestrator Have security audit IPC while fuzzer creates fuzz targets in parallel
```

## Research-Enhanced Patterns

```
/architect Research [topic] with brave-search, then analyze with unified-thinking:analyze-temporal

/security Use unified-thinking:build-causal-graph for attack paths, then brave-search for similar CVEs

/cpp-implement Search for [algorithm] implementations, then use unified-thinking to evaluate approaches
```

## File Locations

- **Commands**: `.claude/commands/*.md`
- **MCP Config**: `C:\Users\rbsmi\AppData\Roaming\Claude\claude_desktop_config.json`
- **Documentation**: `.claude/AGENT_SYSTEM.md`

## Testing

Try this:
```
/architect Can you use brave-search to research browser process architectures and use unified-thinking:decompose-problem to break down adding a new process type?
```
