# Claude Code Agent System - README

## TL;DR - How to Get Parallel Agents Working on Your Todo Lists

**Simple answer**: Just give me your todo list and ask me to work on it in parallel.

```
Here's my todo list:

1. Audit IPC handlers for security vulnerabilities
2. Create fuzz targets for the IPC code
3. Update security documentation
4. Add unit tests
5. Run test suite

Please work on these in parallel where possible.
```

**What happens**: I'll analyze dependencies, spawn 2-5 autonomous agents in a **single message** to work on independent tasks simultaneously, then coordinate sequential tasks.

**Result**: Instead of 5 tasks × 10 minutes = 50 minutes, you get 3 parallel batches in ~15-20 minutes.

## Two Systems, Two Use Cases

### System 1: Specialized Slash Commands
**Purpose**: Ask focused questions with specialized context

**Commands**:
- `/architect` - Architecture design questions
- `/security` - Security analysis and threat modeling
- `/cpp-implement` - C++ implementation guidance
- `/fuzzer` - Fuzzing infrastructure help
- `/networking` - Network stack and Tor integration
- `/reviewer` - Code review with checklists
- `/tester` - Test strategy and implementation
- `/docs` - Documentation writing

**When to use**: Single questions, seeking expert advice

**Example**:
```
/architect

Should we add video decoding to ImageDecoder process or create a new VideoDecoder process?
```

### System 2: Parallel Agent Execution
**Purpose**: Execute multiple tasks simultaneously

**Command**: `/parallel-work` or just ask me to work on your todo list

**When to use**: You have 3+ tasks, want them done fast

**Example**:
```
/parallel-work

Todo:
1. Implement feature X in file_a.cpp
2. Implement feature Y in file_b.cpp
3. Write tests for feature X
4. Document both features
```

**What I do**:
1. Analyze which tasks can run in parallel
2. Group into batches (parallel vs sequential)
3. Spawn multiple Task agents in ONE message (true parallelism)
4. Monitor progress and coordinate handoffs
5. Report results

## How Parallel Execution Works

### The Key Mechanism

When I send **one message** with **multiple Task tool calls**, Claude Code runs them **concurrently**:

```
My response:
├─ Task 1: Security audit of IPC handlers
├─ Task 2: Create fuzz targets
└─ Task 3: Update documentation

Claude Code starts all 3 agents simultaneously →
Each agent works independently →
All report back when done
```

### Dependency Analysis Example

Your todo:
```
1. Design architecture
2. Implement in RequestServer
3. Add IPC messages
4. Update WebContent to use IPC
5. Add tests
```

My analysis:
```
Batch 1 (sequential): Task 1 only (design first)
Batch 2 (parallel): Tasks 2, 3 in parallel (implementation)
Batch 3 (sequential): Task 4 (needs IPC from task 3)
Batch 4 (sequential): Task 5 (needs implementation)
```

Execution:
1. Spawn 1 agent for task 1, wait for completion
2. Spawn 2 agents in parallel for tasks 2 & 3, wait for both
3. Spawn 1 agent for task 4, wait for completion
4. Spawn 1 agent for task 5, done!

## Real-World Examples

### Example 1: Security Audit

```
Please work on this in parallel:

1. Audit Services/WebContent/ for IPC vulnerabilities
2. Audit Services/ImageDecoder/ for buffer overflows
3. Audit Services/RequestServer/ for network security issues
4. Create a summary report of all findings
```

**Execution**:
- Batch 1: Tasks 1, 2, 3 run in parallel (3 security audits simultaneously)
- Batch 2: Task 4 runs (needs findings from all audits)

**Time**: 3 audits in parallel (~10 min) + 1 report (~5 min) = **15 minutes**
vs Sequential: 4 × 10 min = **40 minutes**

### Example 2: Feature Implementation

```
I need these tasks done:

1. Implement rate limiting in ConnectionFromClient.cpp
2. Add SafeMath integer overflow checks in MessageBuffer.cpp
3. Create fuzzer for IPC messages
4. Write unit tests for rate limiting
5. Write unit tests for SafeMath
6. Update IPC security documentation
```

**Execution**:
- Batch 1: Tasks 1, 2, 3, 6 in parallel (implementation + fuzzer + docs)
- Batch 2: Tasks 4, 5 in parallel (tests need implementations from batch 1)

**Time**: ~10-15 minutes vs ~60 minutes sequential

### Example 3: Code Review and Fixes

```
Please review and fix in parallel:

1. Review Services/WebContent/ConnectionFromClient.cpp for security issues
2. Review Services/Sentinel/PolicyGraph.cpp for SQL injection
3. Fix issues found in task 1
4. Fix issues found in task 2
5. Run security test suite
```

**Execution**:
- Batch 1: Tasks 1, 2 in parallel (2 code reviews)
- Batch 2: Tasks 3, 4 in parallel (2 implementations)
- Batch 3: Task 5 (testing)

## What Each Agent Does

When I spawn an agent, I give it:
- **Role**: Security researcher, C++ dev, fuzzer, tester, etc.
- **Task**: Specific, actionable work
- **Requirements**: What to do and how
- **Deliverables**: What to report back
- **Context**: Why it matters, constraints

Each agent:
- Works **autonomously** (makes decisions)
- Uses **all available tools** (Read, Write, Edit, Grep, Bash, MCP servers)
- Reports **status, files changed, blockers**
- Completes **independently** of other agents

## MCP Integration

Agents can use your MCP servers:
- **brave-search**: Research, CVE lookup, best practices
- **unified-thinking**: Problem decomposition, decision-making, causal analysis
- **memory**: Store findings across sessions
- **filesystem**: File operations
- **obsidian-rest**: Your knowledge base

Example agent task:
```
Security Agent #1:
- Use brave-search to find Chromium IPC CVEs
- Use unified-thinking to build attack graph
- Audit ConnectionFromClient.cpp
- Report vulnerabilities found
```

## Usage Tips

### ✅ Good Todo Lists

**Specific tasks with clear deliverables**:
```
1. Add token bucket rate limiter to ConnectionFromClient::handle_message()
   Output: Modified ConnectionFromClient.cpp

2. Create Tests/Fuzzers/fuzz_ipc.cpp using LibFuzzer
   Output: New file + CMakeLists.txt integration

3. Document rate limiting in docs/SECURITY.md section "IPC Security"
   Output: Updated SECURITY.md
```

### ❌ Bad Todo Lists

**Vague or overly dependent**:
```
1. Make it more secure
2. Fix bugs
3. Test everything
4. Improve performance
```

### Pro Tips

1. **Be specific**: "Audit X for Y" not "Check security"
2. **Include file paths**: Helps agents find the right code
3. **Specify deliverables**: What file/output do you want?
4. **Mark dependencies**: Note if task X needs task Y done first
5. **Group related work**: Put similar tasks together

## Limitations

1. **Max ~3-5 parallel agents** (Claude Code limit)
2. **Agents can't communicate** mid-execution
3. **File conflicts** must be resolved after (if two agents edit same file)
4. **Costs more tokens** than sequential work

## Getting Started

### Try It Now - Simple Test

```
Please work on these 3 tasks in parallel:

1. Search the codebase for all IPC message handlers (use Grep)
2. List all .ipc files in Services/ (use Glob)
3. Count lines of code in Services/WebContent/ (use Bash)

Just to test parallel execution.
```

I'll spawn 3 simple agents that complete quickly so you can see how it works.

### Real Work - Todo List

```
Here's my actual todo list:

[paste your list]

Please analyze dependencies and work on this in parallel where possible.
```

I'll handle the coordination automatically.

## Files in This System

```
.claude/
├── commands/
│   ├── architect.md          # Architecture questions
│   ├── security.md           # Security analysis
│   ├── cpp-implement.md      # C++ implementation
│   ├── fuzzer.md             # Fuzzing help
│   ├── networking.md         # Network/Tor features
│   ├── reviewer.md           # Code review
│   ├── tester.md             # Testing strategy
│   ├── docs.md               # Documentation
│   ├── orchestrator.md       # Multi-agent coordination
│   └── parallel-work.md      # Parallel execution (main one)
│
├── PARALLEL_AGENTS.md        # Detailed parallel execution guide
├── AGENT_SYSTEM.md           # Full system documentation
├── QUICK_REFERENCE.md        # Quick command reference
└── README.md                 # This file
```

## Summary

**Specialized commands** (`/architect`, `/security`) = Single expert advice
**Parallel execution** (give me a todo) = Multiple agents working simultaneously

**Just give me your todo list and I'll coordinate everything.**

## 🎯 How to Trigger Parallel Work

You have **3 options**:

### Option 1: Just Ask (Easiest)
```
I need to implement rate limiting for IPC handlers
```
I'll automatically:
1. Create a todo list
2. Show you the plan
3. Work on it in parallel after approval

### Option 2: Use `/plan` First
```
/plan

I'm getting this error: [error]
Help me troubleshoot it.
```
I'll create a detailed plan before executing.

### Option 3: Direct Execution
```
/parallel-work

Fix the failing tests in Services/WebContent/
```
I'll create a plan and start working immediately.

**See `.claude/HOW_TO_TRIGGER_PARALLEL_WORK.md` for detailed examples!**

Ready to try it? Give me a task! 🚀
