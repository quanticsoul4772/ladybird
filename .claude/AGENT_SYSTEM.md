# Claude Code Agent System for Ladybird

This document explains how to use the multi-agent engineering team setup for your Ladybird browser fork.

## What This Is

A practical system that leverages **Claude Code's slash commands** and **MCP servers** to simulate a specialized engineering team. Instead of custom agents (which Claude Code doesn't support), this uses:

1. **Slash Commands** (`.claude/commands/*.md`) - Specialized prompts for different roles
2. **MCP Servers** (configured globally) - Enhanced capabilities for research and reasoning
3. **Structured Workflows** - Coordinated multi-step processes

## Your MCP Servers

These are already configured in your Claude Desktop and available:

- **brave-search** - Web search for research, CVE lookup, best practices
- **unified-thinking** - Advanced reasoning:
  - `decompose-problem` - Break down complex features
  - `make-decision` - Data-driven technical decisions
  - `analyze-temporal` - Short-term vs long-term analysis
  - `build-causal-graph` - Map dependencies and attack chains
  - `analyze-perspectives` - Multi-viewpoint analysis
- **memory** - Persistent memory across sessions
- **filesystem** - File system operations
- **obsidian-rest** - Your Obsidian vault integration
- **windows-cli** - Windows CLI tools

## Available "Agents" (Slash Commands)

### Specialized Roles

| Command | Role | When to Use |
|---------|------|-------------|
| `/architect` | Software Architect | Architecture design, technical decisions, system design questions |
| `/security` | Security Researcher | Security analysis, vulnerability research, threat modeling |
| `/cpp-implement` | C++ Developer | Implementing features in LibWeb, LibJS, LibCore, etc. |
| `/fuzzer` | Fuzzing Specialist | Creating fuzz targets, analyzing crashes, automated testing |
| `/networking` | Network Engineer | Network stack, Tor integration, per-tab privacy features |
| `/performance` | Performance Engineer | Profiling, optimization, benchmarking, regression detection |
| `/reviewer` | Code Reviewer | Code quality review, security audit, architectural consistency |
| `/tester` | Test Engineer | Test strategy, test implementation, QA |
| `/docs` | Documentation Specialist | API docs, architecture docs, user guides |

### Coordinators

| Command | Purpose | When to Use |
|---------|---------|-------------|
| `/orchestrator` | Master Coordinator | Complex tasks requiring multiple specialists, workflow coordination |
| `/feature-workflow` | Complete Workflow | Full feature development from design to documentation |

## How to Use

### Example 1: Architecture Question

```
/architect

We need to decide whether to add video decoding to the existing ImageDecoder process or create a new VideoDecoder process. Can you analyze the tradeoffs?
```

The architect will:
1. Use `brave-search` to research how Chromium/Firefox handle video decoding
2. Use `unified-thinking:analyze-temporal` for short-term vs long-term implications
3. Consider security (sandbox boundaries), performance, and maintainability
4. Provide a recommendation with clear rationale

### Example 2: Security Audit

```
/security

Please audit the IPC message handlers in Services/WebContent/ConnectionFromClient.cpp for security vulnerabilities. Focus on input validation and DoS prevention.
```

The security agent will:
1. Analyze the code for common vulnerability patterns
2. Use `brave-search` to research similar CVEs in other browsers
3. Use `unified-thinking:build-causal-graph` to map potential attack chains
4. Provide specific findings with severity and recommendations

### Example 3: Implement a Feature

```
/cpp-implement

Implement a rate limiter for IPC messages in Services/WebContent/ConnectionFromClient.cpp. It should:
- Track messages per second per message type
- Block clients exceeding 100 messages/second
- Use token bucket algorithm
- Follow Ladybird coding standards
```

The C++ developer will:
1. Research token bucket implementations with `brave-search`
2. Implement following Ladybird coding style (snake_case, ErrorOr<T>, smart pointers)
3. Include proper error handling
4. Add inline documentation

### Example 4: Complete Feature Workflow

```
/feature-workflow

Implement per-tab Tor circuit isolation with stream isolation to prevent cross-tab correlation attacks. Each WebContent process should get a unique Tor circuit through the RequestServer.
```

This coordinates the entire team:
1. Decomposes the feature with `unified-thinking`
2. `/architect` designs the architecture
3. `/security` performs threat modeling
4. `/cpp-implement` implements the feature
5. `/tester` creates tests
6. `/fuzzer` creates fuzz targets
7. `/reviewer` performs code review
8. `/docs` creates documentation

### Example 5: Use MCP Tools Directly

You can also use MCP tools directly without slash commands:

```
Can you use brave-search to find recent CVEs related to browser IPC security, then use unified-thinking to build a causal graph of potential attack chains in Ladybird's multi-process architecture?
```

## Advanced Patterns

### Pattern 1: Research-Driven Development

```
/architect

Use brave-search to research how Tor Browser implements stream isolation, then use unified-thinking:analyze-temporal to evaluate different approaches for Ladybird. Consider our multi-process architecture and existing RequestServer design.
```

### Pattern 2: Security Analysis with Reasoning

```
/security

Analyze the Sentinel service Unix socket IPC for security issues. Use unified-thinking:build-causal-graph to map potential attack paths, then use brave-search to find similar vulnerabilities in other Unix socket services.
```

### Pattern 3: Coordinated Parallel Work

```
/orchestrator

Please coordinate the team to:
1. /security audits IPC handlers in Services/WebContent/
2. /fuzzer creates fuzz targets for the same handlers
3. /docs updates the security documentation

All three should work in parallel and report findings.
```

## Tips for Best Results

### 1. Be Specific
```
❌ "Fix the security issue"
✅ "Audit Services/WebContent/ConnectionFromClient.cpp for integer overflow vulnerabilities in IPC message size validation"
```

### 2. Reference MCP Tools
```
✅ "Use brave-search to find how Chromium handles this, then use unified-thinking:make-decision to choose the best approach"
```

### 3. Provide Context
```
✅ "This is for the RequestServer process which handles all network requests from WebContent. It needs to support per-tab Tor circuits."
```

### 4. Use Workflows for Complex Tasks
```
❌ Asking each role separately in sequence
✅ Use /orchestrator or /feature-workflow to coordinate automatically
```

## File Organization

```
.claude/
├── commands/              # Slash command definitions
│   ├── architect.md
│   ├── security.md
│   ├── cpp-implement.md
│   ├── fuzzer.md
│   ├── networking.md
│   ├── reviewer.md
│   ├── tester.md
│   ├── docs.md
│   ├── orchestrator.md
│   └── feature-workflow.md
├── agents/               # Original agent definitions (reference only)
├── workflows/            # Workflow definitions
├── hooks/                # Git hooks
├── mcp.json             # MCP config (not used - see global config)
└── config.json          # Project config
```

## Global MCP Configuration

Your MCP servers are configured in:
```
C:\Users\rbsmi\AppData\Roaming\Claude\claude_desktop_config.json
```

To add or modify MCP servers, edit that file and restart Claude Desktop.

## Testing the System

Try this to verify everything works:

```
/architect

Can you use brave-search to find information about browser sandbox architectures, then use unified-thinking:decompose-problem to break down the task of adding a new sandboxed process type to Ladybird?
```

You should see:
1. The architect role activates with its specialized context
2. Brave search is used for research
3. Unified-thinking decomposes the problem
4. A structured analysis is provided

## Troubleshooting

### "I don't see slash commands"

Slash commands should auto-complete when you type `/` in Claude Code. If not:
- Restart Claude Code
- Check that `.claude/commands/*.md` files exist
- Try typing the full command like `/architect`

### "MCP servers not working"

- Verify they're in `claude_desktop_config.json`
- Restart Claude Desktop
- Check server paths are correct
- Test by asking to use a specific tool: "Use brave-search to look up X"

### "Agents not coordinating"

Use `/orchestrator` or `/feature-workflow` to explicitly coordinate multiple agents.

## What's Different from Original Plan

The original plan suggested:
- Custom agents loaded from `.claude/agents/*.md` ❌ (not supported)
- Project-level `mcp.json` ❌ (not supported)
- Automatic agent loading ❌ (not supported)

This system provides:
- Slash commands for role-based prompts ✅ (fully supported)
- Global MCP server integration ✅ (configured and working)
- Manual but structured coordination ✅ (via /orchestrator)

## Next Steps

1. **Try a simple command**: `/architect What are the security implications of adding WebGPU support?`

2. **Test MCP integration**: Ask to use `brave-search` or `unified-thinking` within a command

3. **Run a workflow**: Use `/feature-workflow` for a small feature

4. **Customize**: Edit `.claude/commands/*.md` to adjust prompts for your needs

## Questions?

For Claude Code documentation: https://docs.claude.com/claude-code
For Ladybird docs: See `Documentation/` and `docs/` in this repo
