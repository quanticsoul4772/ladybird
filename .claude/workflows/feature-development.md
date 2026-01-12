# Feature Development Workflow

## Purpose
Standard workflow for implementing new features in Ladybird.

## Participants
- @orchestrator (coordinator)
- @architect (design)
- @cpp-core (implementation)
- @security (review)
- @tester (testing)
- @docs (documentation)

## Steps

### 1. Requirements Analysis (@orchestrator)
```
Use unified-thinking:decompose-problem to break down feature
Identify affected components
Determine complexity and timeline
```

### 2. Architecture Design (@architect)
```
Use brave-search to research similar implementations
Use unified-thinking:analyze-temporal for impact analysis
Design IPC interfaces if cross-process
Document in Documentation/Architecture/
```

### 3. Security Review (@security)
```
Review architecture for security implications
Identify potential attack surfaces
Define security requirements
Use unified-thinking:build-causal-graph for threat modeling
```

### 4. Implementation (@cpp-core)
```
Implement feature following design
Follow coding standards
Handle all error cases
Add inline documentation
```

### 5. Testing (@tester, @fuzzer)
```
Write unit tests
Write integration tests
Create fuzz targets if applicable
Verify coverage
```

### 6. Security Audit (@security)
```
Code review for vulnerabilities
Verify input validation
Check IPC message handling
Run security tests
```

### 7. Code Review (@reviewer)
```
Full code review against checklist
Performance analysis
Architecture consistency
```

### 8. Documentation (@docs)
```
API documentation
User-facing docs
Architecture updates
Examples and tutorials
```

### 9. Integration (@orchestrator)
```
Merge coordination
Regression testing
Release notes
```

## Example Usage in Claude Code

> @orchestrator I need to implement a new IPC message type for tab-specific Tor circuit management. Can you coordinate the team to design and implement this feature?

The orchestrator will:
1. Use unified-thinking to decompose the problem
2. Engage @architect for design
3. Coordinate @security for threat analysis
4. Direct @networking and @cpp-core for implementation
5. Ensure @tester creates comprehensive tests
6. Have @docs update documentation
