# Parallel Agent Execution for Todo Lists

This guide shows how to use Claude Code's Task tool to execute multiple agents in parallel on your todo items.

## The Key Insight

Claude Code can run **multiple agents simultaneously** if you invoke them in a **single message**. This is true parallel execution, not sequential.

## How It Works

### Single Message = Parallel Execution

When you send a message with multiple Task tool calls, Claude Code executes them **concurrently**:

```
Single message with 3 Task calls → 3 agents run in parallel
3 separate messages with 1 Task each → 3 agents run sequentially
```

## Practical Example

### Your Todo List:
1. Implement rate limiting for IPC message handlers
2. Create fuzz targets for the new IPC code
3. Update security documentation
4. Add unit tests
5. Run full test suite

### Dependency Analysis:

**Parallel Batch 1** (independent tasks):
- Task 1: Implement rate limiting
- Task 2: Create fuzz targets (can work on existing code)
- Task 3: Update documentation

**Sequential Batch 2** (depends on batch 1):
- Task 4: Add unit tests (needs implementation from task 1)

**Sequential Batch 3** (depends on batch 2):
- Task 5: Run tests (needs tests from task 4)

### Execution

#### Batch 1: Spawn 3 Agents in Parallel

I would send **one message** with **three Task calls**:

```
[Task 1: C++ Implementation Agent]
  → Implement rate limiting in ConnectionFromClient.cpp

[Task 2: Fuzzing Agent]
  → Create LibFuzzer targets for IPC handlers

[Task 3: Documentation Agent]
  → Update docs/SECURITY.md with rate limiting design
```

All three agents start working simultaneously. Each is autonomous and reports back when done.

#### Batch 2: After Batch 1 Completes

Spawn 1 agent:

```
[Task 4: Testing Agent]
  → Add unit tests for rate limiting (using code from Task 1)
```

#### Batch 3: After Batch 2 Completes

Spawn 1 agent:

```
[Task 5: Testing Agent]
  → Run full test suite and fix any failures
```

## The `/parallel-work` Command

I've created a command that automates this:

```
/parallel-work

Here's my todo list:
1. Implement rate limiting for IPC
2. Create fuzz targets
3. Update security docs
4. Add unit tests
5. Run test suite
```

This will:
1. Analyze dependencies
2. Group into batches
3. Spawn parallel agents
4. Monitor progress
5. Coordinate sequential dependencies
6. Report completion

## Real-World Usage

### Scenario 1: Independent Feature Work

```
/parallel-work

Todo:
1. Add CSS grid support to LibWeb
2. Implement WebGPU shader compilation
3. Fix memory leak in image decoder
4. Update build documentation
```

**Result**: All 4 agents run in parallel (no dependencies)

### Scenario 2: Feature with Dependencies

```
/parallel-work

Todo:
1. Design per-tab Tor circuit architecture
2. Implement circuit management in RequestServer
3. Add IPC messages for circuit control
4. Update WebContent to use new IPC
5. Add integration tests
6. Update documentation
```

**Result**:
- Batch 1: Task 1 (design) runs alone
- Batch 2: Tasks 2, 3, 6 run in parallel (implementation + docs)
- Batch 3: Task 4 runs (needs task 3)
- Batch 4: Task 5 runs (needs task 4)

### Scenario 3: Security Audit + Fixes

```
/parallel-work

Todo:
1. Audit IPC handlers in WebContent for vulnerabilities
2. Audit image decoders for buffer overflows
3. Audit network code for info leaks
4. Fix issues from audit 1
5. Fix issues from audit 2
6. Fix issues from audit 3
7. Run security test suite
```

**Result**:
- Batch 1: Tasks 1, 2, 3 in parallel (3 security audits)
- Batch 2: Tasks 4, 5, 6 in parallel (3 fix implementations)
- Batch 3: Task 7 (integration testing)

## How Agents Report Progress

Each agent is instructed to report:

1. **Task status**: Completed / Blocked / Partial
2. **Files changed**: List of modified files
3. **Summary**: What was done
4. **Blockers**: Any issues encountered
5. **Next steps**: Dependencies or follow-up needed

This allows the coordinator to:
- Track overall progress
- Identify conflicts (two agents editing same file)
- Determine when to start next batch
- Handle errors and blockers

## Benefits Over Sequential Execution

### Sequential (Old Way)
```
Task 1: 10 minutes
Task 2: 10 minutes
Task 3: 10 minutes
Total: 30 minutes
```

### Parallel (New Way)
```
Task 1, 2, 3 in parallel: max(10, 10, 10) = 10 minutes
Total: 10 minutes
```

**3x speedup for independent tasks!**

## Handling Conflicts

If two parallel agents modify the same file:

```
Agent 1: Modified ConnectionFromClient.cpp (added rate limiting)
Agent 2: Modified ConnectionFromClient.cpp (added logging)
```

The coordinator will:
1. Detect the conflict
2. Analyze both changes
3. Merge if compatible
4. Ask user if ambiguous

## Advanced: Custom Agent Roles

You can customize agent roles with specialized context:

```
/parallel-work

Todo:
1. [security] Audit Sentinel YARA integration
2. [cpp] Optimize PolicyGraph database queries
3. [fuzzer] Create YARA rule fuzzer
4. [docs] Document Sentinel architecture

Use these specialized agents:
- [security]: Security researcher with Sentinel knowledge
- [cpp]: C++ dev with SQLite and YARA experience
- [fuzzer]: Fuzzer with YARA rule parsing expertise
- [docs]: Doc specialist familiar with security tools
```

## Limitations

1. **Maximum parallel agents**: Claude Code may limit concurrent agents (typically 3-5)
2. **Shared state**: Agents can't communicate mid-execution
3. **File conflicts**: Must be resolved after agents complete
4. **Memory usage**: More parallel agents = more memory

## Best Practices

### ✅ Good: Independent, Well-Defined Tasks

```
1. Implement feature A in file_a.cpp
2. Implement feature B in file_b.cpp
3. Write tests for feature A
4. Write docs for feature B
```

Batch 1: Tasks 1, 2 in parallel
Batch 2: Tasks 3, 4 in parallel

### ❌ Bad: Vague or Highly Dependent Tasks

```
1. Fix the bugs
2. Make it better
3. Test everything
4. Update stuff
```

These are too vague and can't be parallelized effectively.

### ✅ Good: Specific with Clear Deliverables

```
1. Add rate limiting (100 msg/sec) to ConnectionFromClient::handle_message()
   Deliverable: Modified ConnectionFromClient.cpp with token bucket impl

2. Create fuzz_ipc_messages.cpp using LibFuzzer
   Deliverable: New file in Tests/Fuzzers/, integrated in CMakeLists.txt

3. Document rate limiting in docs/SECURITY.md section "IPC Security"
   Deliverable: Updated SECURITY.md with design and rationale
```

## Try It Now

Give me a todo list and let me demonstrate parallel execution:

```
/parallel-work

[Your todo list here]
```

I'll:
1. Show you the dependency analysis
2. Show you the batching strategy
3. Spawn agents in parallel (you'll see multiple Task calls in one message)
4. Coordinate their work
5. Report results

## The Bottom Line

**Slash commands** (`/architect`, `/security`, etc.) are for **specialized prompts** - they help you ask better questions.

**Parallel agents** via `/parallel-work` are for **actual concurrent execution** - they help you get work done faster.

Use `/parallel-work` when you have multiple tasks and want them done simultaneously!
