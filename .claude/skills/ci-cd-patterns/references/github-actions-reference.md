# GitHub Actions Reference for Ladybird CI

## Quick Reference

### Workflow Syntax

```yaml
name: Workflow Name
on: [push, pull_request]
jobs:
  job-id:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5
      - run: echo "Hello World"
```

### Common Triggers

```yaml
# Push to specific branches
on:
  push:
    branches: [master, main, develop]

# Pull requests
on:
  pull_request:
    types: [opened, synchronize, reopened]

# Scheduled runs (cron)
on:
  schedule:
    - cron: '0 2 * * *'  # Daily at 2 AM UTC

# Manual trigger
on: workflow_dispatch

# Release tags
on:
  push:
    tags:
      - 'v*.*.*'

# File path filters
on:
  push:
    paths:
      - 'Libraries/**'
      - 'Services/**'
      - '!**.md'
```

### Concurrency Control

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true  # Cancel outdated runs
```

### Matrix Builds

```yaml
strategy:
  fail-fast: false
  matrix:
    os: [ubuntu-24.04, macos-15, windows-latest]
    compiler: [gcc-14, clang-20]
    preset: [Release, Debug, Sanitizer]

    # Exclude combinations
    exclude:
      - os: macos-15
        compiler: gcc-14

    # Add specific combinations
    include:
      - os: ubuntu-24.04
        compiler: clang-20
        preset: Fuzzers
```

### Environment Variables

```yaml
env:
  # Global environment
  GLOBAL_VAR: value

jobs:
  build:
    env:
      # Job-level environment
      JOB_VAR: value
    steps:
      - name: Step with env
        env:
          # Step-level environment
          STEP_VAR: value
        run: echo "$STEP_VAR"
```

### Secrets and Variables

```yaml
steps:
  - name: Use secret
    env:
      API_TOKEN: ${{ secrets.API_TOKEN }}
    run: curl -H "Authorization: Bearer $API_TOKEN" https://api.example.com

  - name: Use variable
    env:
      ENV_NAME: ${{ vars.ENVIRONMENT }}
    run: echo "Deploying to $ENV_NAME"
```

### Conditional Execution

```yaml
steps:
  - name: Run on Linux only
    if: runner.os == 'Linux'
    run: echo "Linux"

  - name: Run on success
    if: success()
    run: echo "Previous steps succeeded"

  - name: Run on failure
    if: failure()
    run: echo "Something failed"

  - name: Always run
    if: always()
    run: echo "Cleanup"

  - name: Complex condition
    if: |
      github.event_name == 'push' &&
      github.ref == 'refs/heads/master' &&
      !contains(github.event.head_commit.message, '[skip ci]')
    run: echo "Complex condition met"
```

## Common GitHub Actions

### Checkout

```yaml
- uses: actions/checkout@v5
  with:
    fetch-depth: 0  # Full history
    submodules: recursive

- uses: actions/checkout@v5
  with:
    ref: refs/pull/${{ github.event.pull_request.number }}/merge
```

### Caching

```yaml
- uses: actions/cache@v4
  with:
    path: ~/.ccache
    key: ${{ runner.os }}-ccache-${{ github.sha }}
    restore-keys: |
      ${{ runner.os }}-ccache-

- uses: actions/cache@v4
  id: cache
  with:
    path: Build/vcpkg
    key: vcpkg-${{ hashFiles('vcpkg.json') }}

- name: Install dependencies
  if: steps.cache.outputs.cache-hit != 'true'
  run: vcpkg install
```

### Artifacts

```yaml
# Upload
- uses: actions/upload-artifact@v4
  with:
    name: build-artifacts
    path: |
      Build/bin/
      !Build/bin/*.log
    retention-days: 7

# Download
- uses: actions/download-artifact@v4
  with:
    name: build-artifacts
    path: artifacts/
```

### Setup Actions

```yaml
# Python
- uses: actions/setup-python@v5
  with:
    python-version: '3.11'
    cache: 'pip'

# Node.js
- uses: actions/setup-node@v4
  with:
    node-version: '20'
    cache: 'npm'

# Java
- uses: actions/setup-java@v4
  with:
    distribution: 'temurin'
    java-version: '17'
```

## Reusable Workflows

### Calling Workflow

```yaml
jobs:
  call-workflow:
    uses: ./.github/workflows/reusable.yml
    with:
      config: 'production'
      debug: false
    secrets:
      token: ${{ secrets.API_TOKEN }}
```

### Reusable Workflow Definition

```yaml
name: Reusable Workflow

on:
  workflow_call:
    inputs:
      config:
        required: true
        type: string
      debug:
        required: false
        type: boolean
        default: false
    secrets:
      token:
        required: true
    outputs:
      result:
        description: "Result of the workflow"
        value: ${{ jobs.build.outputs.result }}

jobs:
  build:
    runs-on: ubuntu-latest
    outputs:
      result: ${{ steps.build.outputs.result }}
    steps:
      - run: echo "Config: ${{ inputs.config }}"
      - id: build
        run: echo "result=success" >> $GITHUB_OUTPUT
```

## Advanced Patterns

### Job Dependencies

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - run: make build

  test:
    needs: build  # Wait for build
    runs-on: ubuntu-latest
    steps:
      - run: make test

  deploy:
    needs: [build, test]  # Wait for both
    runs-on: ubuntu-latest
    steps:
      - run: make deploy
```

### Service Containers

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:15
        env:
          POSTGRES_PASSWORD: postgres
        options: >-
          --health-cmd pg_isready
          --health-interval 10s
        ports:
          - 5432:5432

    steps:
      - run: psql -h localhost -U postgres
```

### Composite Actions

```yaml
# .github/actions/setup-build/action.yml
name: Setup Build Environment
description: Install dependencies and configure cache

inputs:
  compiler:
    description: 'Compiler to use'
    required: true

runs:
  using: composite
  steps:
    - shell: bash
      run: |
        sudo apt-get install -y ${{ inputs.compiler }}

    - uses: actions/cache@v4
      with:
        path: ~/.ccache
        key: ccache-${{ inputs.compiler }}

# Use in workflow
- uses: ./.github/actions/setup-build
  with:
    compiler: gcc-14
```

### GitHub Script

```yaml
- uses: actions/github-script@v7
  with:
    script: |
      const issue = await github.rest.issues.create({
        owner: context.repo.owner,
        repo: context.repo.repo,
        title: 'Automated issue',
        body: 'This issue was created by a workflow'
      });

      console.log('Created issue:', issue.data.number);
```

## Context Variables

```yaml
steps:
  - name: Print context
    run: |
      echo "Event: ${{ github.event_name }}"
      echo "Ref: ${{ github.ref }}"
      echo "SHA: ${{ github.sha }}"
      echo "Actor: ${{ github.actor }}"
      echo "Repo: ${{ github.repository }}"
      echo "Workflow: ${{ github.workflow }}"
      echo "Run ID: ${{ github.run_id }}"
      echo "Run Number: ${{ github.run_number }}"
      echo "Job: ${{ github.job }}"
      echo "Runner OS: ${{ runner.os }}"
      echo "Runner Arch: ${{ runner.arch }}"
```

## Expressions and Functions

```yaml
steps:
  # String operations
  - if: contains(github.event.head_commit.message, '[skip ci]')
  - if: startsWith(github.ref, 'refs/tags/')
  - if: endsWith(github.ref, '-dev')

  # JSON operations
  - run: echo '${{ toJSON(github.event) }}'

  # Success/failure checks
  - if: success()
  - if: failure()
  - if: always()
  - if: cancelled()

  # Environment
  - run: echo "Branch: ${{ github.ref_name }}"
  - run: echo "Is PR: ${{ github.event_name == 'pull_request' }}"
```

## Best Practices

1. **Use Specific Action Versions**: `actions/checkout@v5` (not `@main`)
2. **Cache Dependencies**: Use `actions/cache@v4` for faster builds
3. **Fail Fast**: Set `fail-fast: false` in matrix to see all failures
4. **Concurrency Control**: Cancel outdated workflow runs
5. **Secrets**: Never log secrets, use GitHub Secrets for sensitive data
6. **Timeouts**: Set job/step timeouts to prevent runaway workflows
7. **Conditional Steps**: Use `if:` to skip unnecessary steps
8. **Artifacts**: Clean up artifacts with appropriate retention periods
9. **Reusable Workflows**: DRY - don't repeat workflow code
10. **Documentation**: Comment complex workflows

## Debugging

```yaml
# Enable debug logging
# Set secret: ACTIONS_STEP_DEBUG = true

steps:
  # Print environment
  - run: env | sort

  # Print context
  - run: echo '${{ toJSON(github) }}'

  # Dump job context
  - run: echo '${{ toJSON(job) }}'

  # SSH debugging (for private runners)
  - uses: mxschmitt/action-tmate@v3
    if: ${{ failure() }}
```

## Security

```yaml
# Limit permissions (principle of least privilege)
permissions:
  contents: read
  pull-requests: write
  issues: write

jobs:
  secure-job:
    permissions:
      contents: read  # Read-only access to repo
    steps:
      - uses: actions/checkout@v5

      # Pin action to specific SHA (most secure)
      - uses: actions/cache@13aacd865c20de90d75de3b17ebe84f7a17d57d2  # v4.0.0
```

## Rate Limits

- **GitHub API**: 1000 requests/hour per workflow
- **Artifacts**: 10 GB total storage
- **Cache**: 10 GB per repository
- **Concurrent Jobs**: Varies by plan (20-60 for free tier)
- **Job Execution Time**: 6 hours maximum
- **Workflow File Size**: 1 MB maximum

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Workflow Syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- [Contexts](https://docs.github.com/en/actions/learn-github-actions/contexts)
- [Expressions](https://docs.github.com/en/actions/learn-github-actions/expressions)
