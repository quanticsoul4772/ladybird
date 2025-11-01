// ============================================================================
// EXAMPLE 1: Resource Leak - Missing Cleanup
// ============================================================================

// BEFORE (WRONG):
class Document {
    ResourceLoader* m_loader { nullptr };

public:
    void load_resource(URL::URL const& url) {
        // LEAK: If called multiple times, previous loader is leaked
        m_loader = new ResourceLoader(url);
        m_loader->start();
    }

    ~Document() {
        // LEAK: Forgot to delete m_loader
    }
};

// AFTER (CORRECT - OwnPtr):
class Document {
    OwnPtr<ResourceLoader> m_loader;

public:
    void load_resource(URL::URL const& url) {
        // Old loader automatically deleted when reassigned
        m_loader = make<ResourceLoader>(url);
        m_loader->start();
    }

    // Destructor automatically deletes m_loader
};


// ============================================================================
// EXAMPLE 2: Reference Cycle - Circular References
// ============================================================================

// BEFORE (WRONG):
class Node : public RefCounted<Node> {
    RefPtr<Node> m_parent;  // Parent refs child
    Vector<NonnullRefPtr<Node>> m_children;  // Child refs parent

    // CYCLE: parent->child->parent->child->...
    // Reference count never reaches zero!
};

// AFTER (CORRECT - WeakPtr):
class Node : public RefCounted<Node> {
    WeakPtr<Node> m_parent;  // Weak reference breaks cycle
    Vector<NonnullRefPtr<Node>> m_children;

    // No cycle: parent owns children, children don't own parent
};


// ============================================================================
// EXAMPLE 3: Exception Safety - Resource Leaked on Error
// ============================================================================

// BEFORE (WRONG):
ErrorOr<void> process_file(String const& path) {
    int fd = TRY(Core::System::open(path, O_RDONLY));

    auto data = TRY(read_all_data(fd));  // LEAK: If this fails, fd never closed

    TRY(Core::System::close(fd));
    return {};
}

// AFTER (CORRECT - RAII):
ErrorOr<void> process_file(String const& path) {
    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Read));

    auto data = TRY(read_all_data(file));

    // file automatically closed on scope exit (even if error thrown)
    return {};
}


// ============================================================================
// EXAMPLE 4: Conditional Leak - Leak in Error Path
// ============================================================================

// BEFORE (WRONG):
ErrorOr<NonnullRefPtr<Image>> load_image(String const& path) {
    auto* decoder = new ImageDecoder();

    auto data = TRY(read_file(path));  // LEAK: If this fails, decoder leaked

    if (data.is_empty()) {
        // LEAK: Forgot to delete decoder
        return Error::from_string_literal("Empty file");
    }

    auto image = TRY(decoder->decode(data));
    delete decoder;
    return image;
}

// AFTER (CORRECT - OwnPtr):
ErrorOr<NonnullRefPtr<Image>> load_image(String const& path) {
    auto decoder = make<ImageDecoder>();

    auto data = TRY(read_file(path));  // decoder auto-deleted on error

    if (data.is_empty()) {
        return Error::from_string_literal("Empty file");
        // decoder auto-deleted here
    }

    auto image = TRY(decoder->decode(data));
    return image;
    // decoder auto-deleted here
}


// ============================================================================
// EXAMPLE 5: Container of Pointers - Forgetting to Delete Elements
// ============================================================================

// BEFORE (WRONG):
class DocumentList {
    Vector<Document*> m_documents;

public:
    void add_document(String const& title) {
        m_documents.append(new Document(title));  // LEAK
    }

    ~DocumentList() {
        // LEAK: Vector destroyed, but Documents not deleted
    }
};

// AFTER (CORRECT - Vector of OwnPtr):
class DocumentList {
    Vector<NonnullOwnPtr<Document>> m_documents;

public:
    void add_document(String const& title) {
        m_documents.append(make<Document>(title));
    }

    // Destructor automatically deletes all Documents
};


// ============================================================================
// EXAMPLE 6: Lambda Capture - Capturing RefPtr Creates Cycle
// ============================================================================

// BEFORE (WRONG):
class EventTarget : public RefCounted<EventTarget> {
    Function<void()> m_callback;

public:
    void setup() {
        // CYCLE: this captures RefPtr to self
        m_callback = [this, strong_this = NonnullRefPtr(*this)]() {
            this->handle_event();
            // strong_this keeps 'this' alive
            // m_callback keeps strong_this alive
            // CYCLE!
        };
    }
};

// AFTER (CORRECT - WeakPtr):
class EventTarget : public RefCounted<EventTarget> {
    Function<void()> m_callback;

public:
    void setup() {
        m_callback = [weak_this = make_weak_ptr()]() {
            auto strong_this = weak_this.strong_ref();
            if (!strong_this)
                return;  // Object was deleted

            strong_this->handle_event();
            // No cycle: weak_this doesn't keep object alive
        };
    }
};


// ============================================================================
// EXAMPLE 7: Copy Constructor - Shallow Copy of Owning Pointer
// ============================================================================

// BEFORE (WRONG):
class Buffer {
    u8* m_data { nullptr };
    size_t m_size { 0 };

public:
    Buffer(size_t size)
        : m_data(new u8[size])
        , m_size(size) {}

    ~Buffer() { delete[] m_data; }

    // PROBLEM: Compiler-generated copy constructor does shallow copy
    // Both objects point to same m_data
    // Double-free when both destroyed!
};

// AFTER (CORRECT - Deep Copy or Delete Copy):
class Buffer {
    OwnPtr<u8[]> m_data;
    size_t m_size { 0 };

public:
    Buffer(size_t size)
        : m_data(make<u8[]>(size))
        , m_size(size) {}

    // Option 1: Delete copy (if copy not needed)
    Buffer(Buffer const&) = delete;
    Buffer& operator=(Buffer const&) = delete;

    // Option 2: Deep copy (if copy needed)
    Buffer(Buffer const& other)
        : m_data(make<u8[]>(other.m_size))
        , m_size(other.m_size) {
        memcpy(m_data.ptr(), other.m_data.ptr(), m_size);
    }
};


// ============================================================================
// EXAMPLE 8: Global Variable - Never Destroyed
// ============================================================================

// BEFORE (WRONG):
HashMap<String, Value>* g_cache { nullptr };

void initialize() {
    g_cache = new HashMap<String, Value>();  // LEAK: Never deleted
}

// AFTER (CORRECT - Static Local):
HashMap<String, Value>& cache() {
    static HashMap<String, Value> s_cache;
    return s_cache;
    // Automatically destroyed at program exit
}

// Or (CORRECT - NeverDestroyed):
HashMap<String, Value>& cache() {
    static NeverDestroyed<HashMap<String, Value>> s_cache;
    return s_cache;
    // Explicitly never destroyed (for shutdown ordering)
}


// ============================================================================
// EXAMPLE 9: Async Callback - Object Destroyed Before Callback
// ============================================================================

// BEFORE (WRONG):
class RequestHandler {
    void send_request(URL::URL const& url) {
        auto callback = [this]() {
            // DANGER: 'this' might be deleted before callback runs
            this->process_response();
        };

        m_connection->async_request(url, move(callback));
    }
};

// AFTER (CORRECT - Capture NonnullRefPtr):
class RequestHandler : public RefCounted<RequestHandler> {
    void send_request(URL::URL const& url) {
        auto callback = [strong_this = NonnullRefPtr(*this)]() {
            // Safe: strong_this keeps object alive
            strong_this->process_response();
        };

        m_connection->async_request(url, move(callback));
    }
};


// ============================================================================
// EXAMPLE 10: Incomplete Cleanup - Partial Resource Release
// ============================================================================

// BEFORE (WRONG):
class Connection {
    int m_socket_fd { -1 };
    u8* m_buffer { nullptr };
    SSL* m_ssl { nullptr };

public:
    ~Connection() {
        if (m_socket_fd >= 0)
            close(m_socket_fd);

        // LEAK: Forgot to free m_buffer and m_ssl
    }
};

// AFTER (CORRECT - Complete Cleanup):
class Connection {
    int m_socket_fd { -1 };
    OwnPtr<u8[]> m_buffer;
    OwnPtr<SSL> m_ssl;  // Assuming SSL has custom deleter

public:
    ~Connection() {
        if (m_socket_fd >= 0)
            close(m_socket_fd);

        // m_buffer and m_ssl automatically cleaned up
    }
};


// ============================================================================
// DETECTION COMMANDS
// ============================================================================

// Build with ASAN (includes LeakSanitizer):
// cmake --preset Sanitizers
// cmake --build Build/sanitizers

// Run with leak detection:
// ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

// Run with Valgrind:
// valgrind --leak-check=full --show-leak-kinds=all ./Build/release/bin/Ladybird

// Suppress known leaks:
// LSAN_OPTIONS=suppressions=lsan_suppressions.txt ./Build/sanitizers/bin/Ladybird


// ============================================================================
// PREVENTION CHECKLIST
// ============================================================================

// [ ] Use OwnPtr<T> for unique ownership (never raw new/delete)
// [ ] Use RefPtr<T> for shared ownership
// [ ] Use WeakPtr<T> to break reference cycles
// [ ] Implement RAII for all resources (files, sockets, locks)
// [ ] Delete copy constructor/assignment for non-copyable resources
// [ ] Use Vector<NonnullOwnPtr<T>> instead of Vector<T*>
// [ ] Capture NonnullRefPtr in async callbacks
// [ ] Clean up all resources in destructor
// [ ] Use static locals instead of global pointers
// [ ] Run ASAN/Valgrind regularly during development
