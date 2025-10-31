# Feature Development Workflow

This command executes the complete feature development workflow, coordinating multiple specialized slash commands.

## Workflow Overview

This orchestrates a complete feature development cycle:
1. Requirements analysis and decomposition
2. Architecture design with research
3. Security threat modeling
4. Parallel implementation and test planning
5. Security audit
6. Code review
7. Documentation
8. Integration

## Execution Steps

The workflow will automatically:

### Phase 1: Planning & Design
1. **Use unified-thinking** to decompose the feature into subtasks
2. **/architect** researches and designs the architecture
   - Uses brave-search for similar browser implementations
   - Uses unified-thinking:analyze-temporal for design implications
   - Produces architecture document
3. **/security** performs initial threat modeling
   - Uses unified-thinking:build-causal-graph for attack chains
   - Identifies security requirements

### Phase 2: Implementation
4. **Parallel execution**:
   - **/cpp-implement** implements core functionality
   - **/tester** designs test strategy and creates test plan
   - **/fuzzer** designs fuzzing infrastructure

### Phase 3: Validation
5. **/security** performs security audit on implementation
   - Reviews code for vulnerabilities
   - Validates security requirements met
6. **/reviewer** performs comprehensive code review
   - Quality, performance, architecture consistency
7. **/tester** implements and runs tests
   - Unit tests, integration tests
   - Validates all requirements met

### Phase 4: Documentation & Integration
8. **/docs** creates comprehensive documentation
   - API documentation
   - Architecture documentation
   - User-facing documentation
9. **Integration validation**
   - Ensure all pieces work together
   - Run full test suite
   - Prepare for merge

## Usage

Simply invoke this command with your feature description:

```
/feature-workflow Implement per-tab Tor circuit isolation with stream isolation to prevent cross-tab correlation attacks
```

The orchestrator will coordinate all agents and guide the feature through all phases.

## What Gets Created

By the end of this workflow, you will have:
- ✅ Architecture design document
- ✅ Security threat model
- ✅ Complete implementation (C++ code)
- ✅ Comprehensive test suite
- ✅ Fuzzing infrastructure
- ✅ Security audit results
- ✅ Code review with feedback addressed
- ✅ API and architecture documentation
- ✅ Integration validation

## Current Feature Request

Please describe the feature you want to implement, and I'll coordinate the team through all phases:
