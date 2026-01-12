# CMake Build System Skill

Comprehensive guide for working with Ladybird's CMake-based build system.

## Overview

This skill provides expert knowledge on:
- CMake configuration and build system architecture
- Adding new libraries and executables
- Managing dependencies (internal and external)
- Build presets and compiler configuration
- Troubleshooting common build issues
- Cross-platform build patterns

## Structure

```
cmake-build-system/
├── SKILL.md                    # Main skill documentation (comprehensive guide)
├── README.md                   # This file
├── examples/                   # Practical examples
│   ├── add-library-example.txt              # Complete library CMakeLists.txt
│   ├── add-executable-example.txt           # Complete service CMakeLists.txt
│   ├── custom-preset-example.json           # Custom preset definitions
│   └── dependency-integration-example.txt   # Dependency patterns
├── scripts/                    # Utility scripts
│   ├── diagnose_build_failure.sh            # Diagnose build issues
│   ├── clean_build.sh                       # Complete clean rebuild
│   └── check_dependencies.sh                # Verify build dependencies
└── references/                 # Reference documentation
    ├── cmake-structure.md                   # CMake organization
    ├── preset-reference.md                  # All presets explained
    ├── compiler-support.md                  # Supported compilers
    └── common-errors.md                     # Error solutions
```

## Quick Start

### Using the Skill

**View main documentation:**
```bash
cat .claude/skills/cmake-build-system/SKILL.md
```

**Check build dependencies:**
```bash
./.claude/skills/cmake-build-system/scripts/check_dependencies.sh
```

**Diagnose build failure:**
```bash
./.claude/skills/cmake-build-system/scripts/diagnose_build_failure.sh
```

**Clean rebuild:**
```bash
./.claude/skills/cmake-build-system/scripts/clean_build.sh
```

### Common Tasks

**Build with specific preset:**
```bash
./Meta/ladybird.py build --preset Release     # Default
./Meta/ladybird.py build --preset Debug       # Debugging
./Meta/ladybird.py build --preset Sanitizer   # Bug hunting
```

**Add a new library:**
1. Read: `examples/add-library-example.txt`
2. Create: `Libraries/LibYourLib/CMakeLists.txt`
3. Follow the template pattern
4. Build: `./Meta/ladybird.py build LibYourLib`

**Add a new service:**
1. Read: `examples/add-executable-example.txt`
2. Create: `Services/YourService/CMakeLists.txt`
3. Update: `Services/CMakeLists.txt` (add_subdirectory)
4. Build: `./Meta/ladybird.py build YourService`

**Integrate external dependency:**
1. Read: `examples/dependency-integration-example.txt`
2. Choose appropriate pattern (vcpkg, pkg-config, manual)
3. Add to CMakeLists.txt
4. Rebuild: `./Meta/ladybird.py rebuild`

## Key Concepts

### CMake Architecture

Ladybird uses a hierarchical CMake structure:
- **Root:** `CMakeLists.txt` - Project configuration
- **Presets:** `CMakePresets.json` - Build configurations
- **Modules:** `Meta/CMake/*.cmake` - Utility functions
- **Wrapper:** `Meta/ladybird.py` - Primary build interface

See: `references/cmake-structure.md`

### Build Presets

Pre-configured build environments for different purposes:
- **Release:** Default optimized build
- **Debug:** Full debugging symbols
- **Sanitizer:** ASAN + UBSAN enabled
- **Distribution:** Static linking for release
- **And more...**

See: `references/preset-reference.md`

### Compiler Support

Ladybird requires **C++23** support:
- GCC 14+
- Clang 20+
- Apple Clang 16+ (Xcode 16)
- Clang-CL 20+ (Windows experimental)

See: `references/compiler-support.md`

## Common Workflows

### Development Workflow

```bash
# Initial setup
./Meta/ladybird.py vcpkg
./Meta/ladybird.py build

# Iterative development
./Meta/ladybird.py build <target>  # Build specific target
./Meta/ladybird.py run             # Run Ladybird
./Meta/ladybird.py test            # Run tests

# Debug
./Meta/ladybird.py build --preset Debug
./Meta/ladybird.py debug
```

### Adding New Code

```bash
# 1. Create CMakeLists.txt (see examples/)
# 2. Add sources
# 3. Configure dependencies
# 4. Build and test

./Meta/ladybird.py build YourTarget
./Build/release/bin/YourTarget
```

### Troubleshooting

```bash
# Check dependencies
./.claude/skills/cmake-build-system/scripts/check_dependencies.sh

# Diagnose failure
./.claude/skills/cmake-build-system/scripts/diagnose_build_failure.sh

# Clean rebuild
./.claude/skills/cmake-build-system/scripts/clean_build.sh

# Or manual clean
./Meta/ladybird.py clean
./Meta/ladybird.py build
```

## Examples

### Library CMakeLists.txt

See: `examples/add-library-example.txt`

**Pattern:**
```cmake
set(SOURCES file1.cpp file2.cpp)
ladybird_lib(LibName linkname)
target_link_libraries(LibName PUBLIC LibCore PRIVATE LibCrypto)
```

### Service CMakeLists.txt

See: `examples/add-executable-example.txt`

**Pattern:**
```cmake
add_library(servicelib STATIC ${SOURCES})
add_executable(Service main.cpp)
compile_ipc(Service.ipc ServiceEndpoint.h)
target_link_libraries(Service PRIVATE servicelib)
```

### Custom Preset

See: `examples/custom-preset-example.json`

**Pattern:**
```json
{
  "name": "MyCustom",
  "inherits": "unix_base",
  "binaryDir": "${fileDir}/Build/mycustom",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "MY_OPTION": "ON"
  }
}
```

## Reference Documentation

### CMake Structure
`references/cmake-structure.md`
- Build system organization
- Directory layout
- Build flow
- Code generation

### Preset Reference
`references/preset-reference.md`
- All available presets
- Preset configuration
- When to use each preset
- Performance comparison

### Compiler Support
`references/compiler-support.md`
- Required compiler versions
- Platform-specific setup
- C++23 features used
- Compiler selection

### Common Errors
`references/common-errors.md`
- CMake errors
- Compiler errors
- Linker errors
- Runtime errors
- Platform-specific issues
- Recovery procedures

## Utility Scripts

### diagnose_build_failure.sh

Diagnose common build issues automatically.

**Usage:**
```bash
./.claude/skills/cmake-build-system/scripts/diagnose_build_failure.sh
```

**Checks:**
- CMake version
- Compiler versions
- Build dependencies
- Disk space
- Memory
- vcpkg state
- Build directory state

### check_dependencies.sh

Verify all build dependencies are installed.

**Usage:**
```bash
./.claude/skills/cmake-build-system/scripts/check_dependencies.sh
```

**Checks:**
- CMake, Ninja, Git
- GCC, Clang compilers
- Python3
- Platform-specific libraries
- Optional tools (ccache, debuggers)

**Exit codes:**
- 0: All dependencies satisfied
- Non-zero: Missing required dependencies

### clean_build.sh

Perform complete clean rebuild.

**Usage:**
```bash
# Basic clean rebuild
./.claude/skills/cmake-build-system/scripts/clean_build.sh

# Options
./.claude/skills/cmake-build-system/scripts/clean_build.sh --preset Debug
./.claude/skills/cmake-build-system/scripts/clean_build.sh --all  # Clean vcpkg too
./.claude/skills/cmake-build-system/scripts/clean_build.sh -j 4   # 4 parallel jobs
```

**Features:**
- Clean build directory
- Optional vcpkg clean
- Optional cache clean
- Automatic rebuild
- Progress reporting

## Best Practices

1. **Always use ladybird.py** instead of direct CMake
2. **Use presets** for consistent builds
3. **Separate library and executable** for testability
4. **Mark generated sources** properly
5. **Use PUBLIC/PRIVATE** link visibility correctly
6. **Check dependencies** before building
7. **Clean rebuild** when in doubt
8. **Read examples** before adding new targets

## Integration with CLAUDE.md

This skill complements the main project documentation:
- **CLAUDE.md:** High-level commands and workflows
- **This skill:** Deep CMake build system knowledge

Use this skill when:
- Adding new libraries or services
- Integrating external dependencies
- Troubleshooting build issues
- Understanding build system internals
- Customizing build configuration

## Tips and Tricks

### Fast Iteration

```bash
# Build only what changed
ninja -C Build/release <target>

# Use Release for faster execution
./Meta/ladybird.py run --preset Release

# Use ccache (auto-enabled if installed)
sudo apt install ccache
```

### Debugging Builds

```bash
# Verbose output
./Meta/ladybird.py build -- -v

# Check CMake variables
cmake -L Build/release

# Inspect compilation database
cat Build/release/compile_commands.json
```

### Parallel Builds

```bash
# Auto-detect CPU cores
./Meta/ladybird.py build -j $(nproc)

# Or set environment variable
export MAKEJOBS=$(nproc)
./Meta/ladybird.py build
```

### Build Performance

```bash
# Use ccache for rebuilds
sudo apt install ccache

# Use local vcpkg binary cache
# (automatically configured in CMakePresets.json)

# Build specific targets only
ninja -C Build/release LibWeb Ladybird
```

## When to Use This Skill

- ✅ Adding new libraries or services
- ✅ Integrating external dependencies
- ✅ Troubleshooting build failures
- ✅ Understanding build system
- ✅ Customizing build configuration
- ✅ Cross-platform builds
- ✅ CI/CD setup

## Related Skills

- **ladybird-cpp-patterns:** C++ coding patterns
- **multi-process-architecture:** Process architecture
- **ipc-security:** IPC patterns

## Getting Help

1. **Check examples:** `examples/` directory
2. **Read references:** `references/` directory
3. **Run diagnostics:** `scripts/diagnose_build_failure.sh`
4. **Search common errors:** `references/common-errors.md`
5. **Main documentation:** `SKILL.md`

## Maintenance

This skill is maintained alongside the Ladybird codebase. When build system changes occur, update:
- Main skill documentation (SKILL.md)
- Examples if patterns change
- Reference docs for new presets/compilers
- Error solutions for new issues

Last updated: 2025-11-01
