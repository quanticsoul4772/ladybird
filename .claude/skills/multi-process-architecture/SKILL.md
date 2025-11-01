---
name: multi-process-architecture
description: Architecture patterns for Ladybird's multi-process browser design, including process isolation, IPC coordination, sandboxing, and failure handling
use-when: Designing new processes, implementing IPC interfaces, setting up sandboxes, or coordinating multi-process workflows
allowed-tools:
  - Read
  - Write
  - Grep
tags:
  - architecture
  - ipc
  - sandboxing
---

# Multi-Process Architecture Patterns

## Ladybird Process Model
```
┌─────────────────────────────────────────────────────┐
│                   UI Process                         │
│  (Privileged, Unsandboxed)                          │
│  - Window management                                 │
│  - Tab orchestration                                 │
│  - User input handling                               │
└──────────┬──────────────┬──────────────┬────────────┘
           │              │              │
    ┌──────▼──────┐ ┌────▼─────┐ ┌─────▼──────┐
    │ WebContent  │ │ WebContent│ │ WebContent │
    │ (Process 1) │ │ (Process 2│ │ (Process 3)│
    │  Sandboxed  │ │ Sandboxed │ │ Sandboxed  │
    └──────┬──────┘ └────┬──────┘ └─────┬──────┘
           │              │               │
    ┌──────▼───────────────▼───────────── ▼──────┐
    │         RequestServer Process               │
    │  (Network isolation + Tor integration)      │
    │  Sandboxed                                   │
    └──────────┬─────────────────────────────────┘
               │
    ┌──────────▼──────────┐
    │  ImageDecoder       │
    │  (Sandboxed)        │
    │  - Per-tab or       │
    │    shared           │
    └─────────────────────┘
```

## Design Principles

### 1. Process Boundaries Define Trust Boundaries
```cpp
// ❌ WRONG - Mixing privilege levels
class BrowserWindow {
    void handle_web_request() {
        // Direct file system access from web content!
        auto file = open("/etc/passwd");
    }
};

// ✅ CORRECT - Clear separation
class UIProcess {
    void handle_file_access_request(IPC::Message const& msg) {
        // Validate request from sandboxed process
        if (!is_authorized(msg.path))
            return error("Permission denied");

        // Privileged process handles file access
        return read_file(msg.path);
    }
};
```

### 2. Minimize IPC Surface Area
```cpp
// ❌ WRONG - Too many IPC messages
interface WebContentToUI {
    WriteFile(path, data);
    ReadFile(path);
    DeleteFile(path);
    ExecuteCommand(cmd);
    // Too much power!
}

// ✅ CORRECT - Minimal, specific messages
interface WebContentToUI {
    RequestResource(url);  // Limited to HTTP(S)
    DisplayNotification(message);
    UpdateTitle(title);
}
```

### 3. Fail-Safe Process Crashes
```cpp
class TabManager {
    void on_web_content_crash(ProcessID pid)
    {
        // 1. Identify affected tab
        auto* tab = find_tab_by_process(pid);

        // 2. Show crash page (don't bring down browser)
        tab->show_crash_page();

        // 3. Clean up IPC connections
        m_ipc_connections.remove(pid);

        // 4. Log for debugging
        dbgln("WebContent process {} crashed", pid);

        // UI process continues running!
    }
};
```

## Process Creation Patterns

### WebContent Process (Per-Tab)
```cpp
// UIProcess/TabManager.cpp
ErrorOr<NonnullRefPtr<Tab>> TabManager::create_new_tab()
{
    // 1. Spawn sandboxed WebContent process
    auto process = TRY(WebContent::Process::spawn_with_sandbox());

    // 2. Establish IPC connection
    auto connection = TRY(IPC::Connection::create(process.pid()));

    // 3. Configure sandbox parameters
    TRY(connection->send_sync(Messages::WebContent::SetSandboxParameters{
        .allow_network = false,  // Use RequestServer
        .allow_files = false,
        .max_memory_mb = 512
    }));

    // 4. Create tab object
    return Tab::create(move(process), move(connection));
}
```

### RequestServer Process (Shared)
```cpp
// UIProcess/RequestServerManager.cpp
class RequestServerManager {
public:
    static RequestServerManager& the()
    {
        static RequestServerManager instance;
        return instance;
    }

    ErrorOr<Response> make_request(URL const& url, ProcessID requestor)
    {
        // 1. Validate request
        if (!is_safe_url(url))
            return Error::from_string_literal("Unsafe URL");

        // 2. Check Tor circuit for tab
        auto circuit = get_or_create_tor_circuit(requestor);

        // 3. Forward to RequestServer process
        return m_connection->send_sync(Messages::RequestServer::FetchResource{
            .url = url,
            .circuit_id = circuit->id(),
            .requestor = requestor
        });
    }

private:
    NonnullRefPtr<IPC::Connection> m_connection;
};
```

## Sandboxing

### Sandbox Configuration
```cpp
struct SandboxPolicy {
    // File system
    Vector<String> allowed_read_paths;
    Vector<String> allowed_write_paths;

    // Network
    bool allow_network { false };
    Vector<String> allowed_domains;  // If allow_network

    // System calls
    Vector<int> allowed_syscalls;

    // Resources
    size_t max_memory_bytes { 512 * 1024 * 1024 };  // 512 MiB
    size_t max_cpu_percent { 100 };
};

ErrorOr<void> apply_sandbox(SandboxPolicy const& policy)
{
#ifdef __linux__
    // Use seccomp-bpf
    TRY(setup_seccomp(policy.allowed_syscalls));

    // Use namespaces
    TRY(setup_namespaces(policy));

    // Resource limits
    TRY(setrlimit(RLIMIT_AS, policy.max_memory_bytes));
#endif

    return {};
}
```

### Platform-Specific Sandboxing
```cpp
// Linux: seccomp + namespaces
#ifdef __linux__
    TRY(unshare(CLONE_NEWNET | CLONE_NEWPID));
    TRY(apply_seccomp_filter());
#endif

// macOS: sandbox-init
#ifdef __APPLE__
    TRY(sandbox_init(kSBXProfilePureComputation, SANDBOX_NAMED, nullptr));
#endif

// OpenBSD: pledge/unveil
#ifdef __OpenBSD__
    TRY(unveil("/tmp", "rw"));
    TRY(pledge("stdio rpath wpath", nullptr));
#endif
```

## IPC Coordination

### Request-Response Pattern
```cpp
// WebContent sends request
auto response = TRY(connection->send_sync(
    Messages::RequestServer::FetchURL { url }
));

// RequestServer responds
connection->send(Messages::WebContent::DidFetchURL {
    .request_id = response.id,
    .data = response.data,
    .status = 200
});
```

### Event Stream Pattern
```cpp
// WebContent subscribes to events
connection->send(Messages::RequestServer::SubscribeToProgress {
    .request_id = id
});

// RequestServer sends updates
connection->send(Messages::WebContent::DownloadProgress {
    .request_id = id,
    .bytes_received = 1024,
    .total_bytes = 4096
});
```

### Buffered Message Pattern
```cpp
// For high-frequency updates (e.g., scroll)
class MessageBuffer {
    void queue_message(IPC::Message&& msg)
    {
        m_buffer.append(move(msg));

        if (!m_flush_timer->is_active())
            m_flush_timer->start(Duration::from_milliseconds(16));  // ~60 FPS
    }

    void flush()
    {
        auto messages = move(m_buffer);
        m_connection->send_batch(move(messages));
    }

private:
    Vector<IPC::Message> m_buffer;
    RefPtr<Core::Timer> m_flush_timer;
    NonnullRefPtr<IPC::Connection> m_connection;
};
```

## Process Lifecycle
```cpp
class ProcessLifecycle {
    // 1. Creation
    static ErrorOr<Process> spawn(ProcessType type)
    {
        auto process = TRY(fork_and_exec(type));
        TRY(apply_sandbox(type));
        TRY(establish_ipc(process));
        return process;
    }

    // 2. Health monitoring
    void monitor()
    {
        if (m_heartbeat_timer->is_expired()) {
            dbgln("Process {} unresponsive", m_pid);
            terminate();
        }
    }

    // 3. Graceful shutdown
    void shutdown()
    {
        m_connection->send(Messages::Process::Shutdown {});
        m_shutdown_timer->start(Duration::from_seconds(5));
    }

    // 4. Force termination
    void terminate()
    {
        kill(m_pid, SIGKILL);
        m_on_terminated();
    }
};
```

## Error Handling

### Process Crash Recovery
```cpp
void on_process_crash(ProcessID pid, int signal)
{
    dbgln("Process {} crashed with signal {}", pid, signal);

    // Collect crash dump
    auto dump = collect_crash_dump(pid);
    save_crash_dump(dump);

    // Notify user
    show_error_dialog("A tab has crashed");

    // Restart if critical process
    if (is_critical_process(pid))
        restart_process(pid);
}
```

### IPC Timeout Handling
```cpp
auto result = connection->send_sync(
    Messages::WebContent::EvaluateJS { code },
    Duration::from_seconds(5)  // Timeout
);

if (!result.has_value()) {
    dbgln("IPC timeout, process may be hung");
    // Option 1: Terminate process
    // Option 2: Show "Unresponsive tab" dialog
}
```

## Performance Considerations

### Process Pool
```cpp
class WebContentProcessPool {
    ErrorOr<NonnullRefPtr<Process>> acquire()
    {
        // Reuse idle process
        if (!m_idle_processes.is_empty())
            return m_idle_processes.take_first();

        // Spawn new if under limit
        if (m_active_count < MAX_PROCESSES)
            return Process::spawn();

        // Wait for available process
        return wait_for_available_process();
    }

    void release(NonnullRefPtr<Process> process)
    {
        process->reset_state();
        m_idle_processes.append(move(process));
    }

private:
    Vector<NonnullRefPtr<Process>> m_idle_processes;
    size_t m_active_count { 0 };
    static constexpr size_t MAX_PROCESSES = 10;
};
```

### Shared Memory for Large Transfers
```cpp
// Instead of copying large buffers over IPC
ErrorOr<void> transfer_large_data(ByteBuffer const& data)
{
    // 1. Create shared memory
    auto shm = TRY(SharedMemory::create(data.size()));

    // 2. Copy data once
    memcpy(shm->data(), data.data(), data.size());

    // 3. Send only handle over IPC
    TRY(connection->send(Messages::WebContent::SharedData {
        .handle = shm->handle(),
        .size = data.size()
    }));

    return {};
}
```

## Checklist

- [ ] Each process has single, well-defined responsibility
- [ ] Trust boundaries align with process boundaries
- [ ] Sandboxing applied to all untrusted processes
- [ ] IPC surface area minimized
- [ ] All IPC messages validated
- [ ] Process crashes don't affect other tabs
- [ ] Resource limits enforced per process
- [ ] Health monitoring for critical processes
- [ ] Graceful degradation on process failure
- [ ] Performance optimized (process pools, shared memory)

## Related Skills

### Complementary Skills
- **[ipc-security](../ipc-security/SKILL.md)**: Implement secure IPC communication between processes. Understanding process boundaries is prerequisite to securing IPC channels. Reference validation patterns and rate limiting when designing cross-process communication.
- **[tor-integration](../tor-integration/SKILL.md)**: Per-tab Tor circuit isolation leverages multi-process architecture. Each WebContent process can have independent Tor circuits. Understand process isolation before implementing network privacy features.

### Implementation Skills
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Use C++ patterns for process lifecycle management, IPC messaging, and error handling across process boundaries.
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Build process binaries and link IPC endpoints. Each process (WebContent, RequestServer, etc.) has its own CMake target.

### Testing and Debugging
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug crashes in multi-process context. Use process attach techniques to debug WebContent, RequestServer, or ImageDecoder processes.
- **[browser-performance](../browser-performance/SKILL.md)**: Profile multi-process overhead and IPC latency. Optimize process spawn times and message passing performance.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Fuzz IPC boundaries and process isolation. Test process crash recovery and sandbox escapes.
