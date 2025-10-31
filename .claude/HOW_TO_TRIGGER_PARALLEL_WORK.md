# How to Trigger Parallel Work - Quick Guide

## 🎯 Three Ways to Get Parallel Execution

### Method 1: Just Ask (Recommended)

**You don't need a special command!** Just describe what you want:

```
I need to implement rate limiting for IPC message handlers
```

**What I do:**
1. ✅ Automatically recognize this is complex
2. ✅ Create a todo list using TodoWrite
3. ✅ Show you the plan with dependencies
4. ✅ Wait for your approval
5. ✅ Execute tasks in parallel

**Example interaction:**
```
You: I need to add fuzzing for the Sentinel service

Me: I've created a todo list with 5 tasks:

    Batch 1 (Parallel):
    1. Identify fuzz targets in Sentinel code
    2. Research YARA rule fuzzing techniques
    3. Check existing fuzzing infrastructure

    Batch 2 (Sequential):
    4. Create fuzz harnesses
    5. Integrate with build system

    Proceed with parallel execution?

You: Yes

Me: [Starts working on batch 1 tasks in parallel...]
```

---

### Method 2: Use `/plan` Command

For complex requests where you want to see the plan first:

```
/plan

I'm getting this compilation error: [error message]
Help me troubleshoot and fix it.
```

**What I do:**
1. ✅ Create detailed todo list
2. ✅ Show execution plan with batches
3. ✅ Wait for approval
4. ✅ Execute in parallel

**Best for:**
- Bug troubleshooting
- Feature implementation
- Multi-step investigations

---

### Method 3: Use `/parallel-work` with Request

For explicit parallel execution:

```
/parallel-work

Troubleshoot this error: [error]
```

**What I do:**
1. ✅ Create todo list automatically
2. ✅ Execute immediately (less approval step)
3. ✅ Report progress as I work

**Best for:**
- When you trust me to plan well
- Urgent tasks
- When you've used me before and know the pattern

---

## 📋 What Makes a Good Request?

### ✅ Good Requests (I can plan and parallelize)

```
"Implement rate limiting for IPC handlers"
→ I create: audit, design, implement, test, document

"This test is failing: [error]. Fix it"
→ I create: reproduce, debug, identify cause, fix, verify

"Add security validation to the new proxy feature"
→ I create: audit code, identify gaps, implement validation, test, document

"Analyze performance of the YARA scanning"
→ I create: profile code, identify bottlenecks, research solutions, implement optimizations
```

### ❌ Too Simple (no planning needed)

```
"What does this function do?"
→ I just answer directly

"Read Services/WebContent/ConnectionFromClient.cpp"
→ I just read it

"Explain how IPC works"
→ I just explain
```

### 🤔 Borderline (I'll ask if you want a plan)

```
"Review this code for security issues"
→ Could be simple or complex depending on scope

"Help me understand this error"
→ Might just need explanation, or might need investigation
```

---

## 🚀 Example Workflows

### Workflow 1: Feature Implementation

**Your request:**
```
Add input validation to the enable_tor IPC handler
```

**My response:**
```
I've created a todo list:

Batch 1 (Parallel):
1. Read enable_tor handler implementation
2. Check what ValidatedDecoder provides
3. Find similar validated handlers for reference

Batch 2 (Sequential):
4. Implement validation using ValidatedDecoder
5. Add error handling

Batch 3 (Sequential):
6. Write unit tests for validation
7. Test with invalid inputs

Proceed?
```

**After approval:** I work on batch 1 in parallel, then batches 2 and 3.

---

### Workflow 2: Bug Investigation

**Your request:**
```
Getting segfault in Sentinel when processing large YARA rules.
Stack trace: [trace]
```

**My response:**
```
I've created investigation plan:

Batch 1 (Parallel):
1. Analyze stack trace for exact crash location
2. Search for similar issues in codebase
3. Check YARA rule size limits

Batch 2 (Sequential):
4. Reproduce with minimal test case
5. Debug with GDB/LLDB
6. Identify root cause

Batch 3 (Sequential):
7. Implement fix
8. Verify fix with original crash case
9. Add regression test

Proceed?
```

---

### Workflow 3: Troubleshooting

**Your request:**
```
/parallel-work

Tests are failing after my changes:
- TestCircuitIsolation failed
- TestProxyValidation passed
- TestFormMonitor crashed

Help fix these failures.
```

**My response:**
```
Creating todo list...

Batch 1 (Parallel - independent investigations):
1. Investigate TestCircuitIsolation failure
2. Investigate TestFormMonitor crash
3. Verify TestProxyValidation still covers what it should

Batch 2 (Sequential - fixes):
4. Fix TestCircuitIsolation
5. Fix TestFormMonitor
6. Re-run all tests

Starting batch 1 in parallel...
[Works on all 3 investigations simultaneously]
```

---

## 💡 Pro Tips

### Tip 1: Be Specific

```
❌ "Fix the security issues"
✅ "Audit Services/WebContent/ for input validation issues and fix any found"
```

### Tip 2: Provide Context

```
❌ "This doesn't work"
✅ "WebContent crashes when loading evil.com with error: [trace]"
```

### Tip 3: Specify Scope

```
❌ "Make it faster"
✅ "Profile and optimize YARA rule scanning in Sentinel service"
```

### Tip 4: Include Constraints

```
✅ "Implement X but don't change the IPC protocol"
✅ "Fix Y without breaking existing tests"
✅ "Add Z using only upstream Ladybird patterns"
```

---

## 🎭 When I Won't Create a Todo List

I skip the todo list for:

1. **Simple questions**
   - "What does this do?"
   - "How does X work?"
   - "Explain Y"

2. **Single-step tasks**
   - "Read this file"
   - "Search for this pattern"
   - "Show me the definition of X"

3. **Clarification requests**
   - "What did you mean by X?"
   - "Can you explain that differently?"

For these, I just do the task immediately.

---

## 🔄 Changing the Plan Mid-Execution

If you want to adjust the plan:

```
You: Actually, skip task 3 and add a new task to check performance

Me: Updated todo list:
    ✅ Task 1 (completed)
    ✅ Task 2 (completed)
    ❌ Task 3 (removed)
    ⭐ Task 4 (new): Check performance impact
    ⏸️  Task 5 (pending)

    Continuing with task 4...
```

I'll adjust the plan and keep working!

---

## Summary

**Easiest way:** Just tell me what you want
**Explicit planning:** Use `/plan` first
**Direct execution:** Use `/parallel-work`

**I'll automatically:**
- Create todos for complex work
- Work in parallel when possible
- Keep you updated on progress
- Handle dependencies correctly

**Try it now!** Give me a task and watch me plan and execute in parallel! 🚀
