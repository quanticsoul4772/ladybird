/*
 * Example: Garbage Collection Integration in LibJS
 *
 * This example demonstrates:
 * - GC-safe object management
 * - Proper use of GC::Ptr and GC::Ref
 * - Implementing visit_edges for marking
 * - Common GC patterns and pitfalls
 */

#pragma once

#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibGC/Ptr.h>
#include <AK/Vector.h>

namespace JS {

// Example 1: Object with GC-managed members
class Node : public Object {
    JS_OBJECT(Node, Object);
    GC_DECLARE_ALLOCATOR(Node);

public:
    static GC::Ref<Node> create(Realm&, GC::Ptr<PrimitiveString> data);

    // ✅ CORRECT: Use GC::Ptr for nullable references
    GC::Ptr<Node> parent() const { return m_parent; }
    void set_parent(GC::Ptr<Node> parent) { m_parent = parent; }

    // ✅ CORRECT: Use GC::Ref for non-null references
    GC::Ref<PrimitiveString> data() const { return m_data; }

    // Add child to this node
    void add_child(GC::Ref<Node> child)
    {
        m_children.append(child);
        child->set_parent(this);
    }

protected:
    Node(Realm&, GC::Ref<PrimitiveString> data);

    // ✅ CRITICAL: Implement visit_edges to tell GC about managed members
    virtual void visit_edges(Cell::Visitor& visitor) override;

private:
    // GC-managed members
    GC::Ptr<Node> m_parent;                    // Nullable reference
    GC::Ref<PrimitiveString> m_data;           // Non-null reference
    Vector<GC::Ref<Node>> m_children;          // Collection of references

    // ❌ WRONG: Don't use raw pointers to GC objects!
    // Node* m_sibling;  // This will cause crashes!
};

GC_DEFINE_ALLOCATOR(Node);

GC::Ref<Node> Node::create(Realm& realm, GC::Ptr<PrimitiveString> data)
{
    // Allocate on GC heap
    auto data_ref = data ? *data : PrimitiveString::create(realm.vm(), String {});
    return realm.heap().allocate<Node>(realm, data_ref);
}

Node::Node(Realm& realm, GC::Ref<PrimitiveString> data)
    : Object(ConstructWithPrototypeTag::Tag, realm.intrinsics().object_prototype())
    , m_parent(nullptr)  // Initialize nullable GC::Ptr
    , m_data(data)       // Store non-null GC::Ref
{
}

// ✅ CRITICAL: Visit all GC-managed members
void Node::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);

    // Visit single GC pointers
    visitor.visit(m_parent);
    visitor.visit(m_data);

    // Visit collections of GC pointers
    for (auto& child : m_children)
        visitor.visit(child);
}

// Example 2: Object with optional GC references
class Cache : public Object {
    JS_OBJECT(Cache, Object);
    GC_DECLARE_ALLOCATOR(Cache);

public:
    static GC::Ref<Cache> create(Realm&);

    // Store value in cache
    void set(String const& key, Value value)
    {
        m_entries.set(key, value);
    }

    // Get value from cache
    Value get(String const& key) const
    {
        return m_entries.get(key).value_or(js_undefined());
    }

protected:
    explicit Cache(Realm&);

    virtual void visit_edges(Cell::Visitor& visitor) override;

private:
    // HashMap with Value entries (which may contain GC objects)
    HashMap<String, Value> m_entries;

    // Optional GC reference
    GC::Ptr<Object> m_fallback;
};

GC_DEFINE_ALLOCATOR(Cache);

GC::Ref<Cache> Cache::create(Realm& realm)
{
    return realm.heap().allocate<Cache>(realm);
}

Cache::Cache(Realm& realm)
    : Object(ConstructWithPrototypeTag::Tag, realm.intrinsics().object_prototype())
{
}

void Cache::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);

    // Visit HashMap entries that may contain GC objects
    for (auto& entry : m_entries)
        visitor.visit(entry.value);

    // Visit optional reference
    visitor.visit(m_fallback);
}

// Example 3: Allocation patterns
class AllocationExamples {
public:
    // ✅ CORRECT: Standard allocation pattern
    static ThrowCompletionOr<GC::Ref<Object>> create_object(VM& vm)
    {
        auto& realm = *vm.current_realm();

        // Direct allocation
        auto obj = realm.heap().allocate<Object>(realm, realm.intrinsics().object_prototype());

        return obj;
    }

    // ✅ CORRECT: Allocation with OOM handling
    static ThrowCompletionOr<GC::Ref<Array>> create_array(VM& vm, size_t length)
    {
        auto& realm = *vm.current_realm();

        // TRY_OR_THROW_OOM handles allocation failures gracefully
        auto array = TRY_OR_THROW_OOM(vm, Array::create(realm, length));

        return array;
    }

    // ✅ CORRECT: Temporary root for operations
    static ThrowCompletionOr<Value> create_and_populate(VM& vm)
    {
        auto& realm = *vm.current_realm();

        // Allocate array
        auto array = TRY(Array::create(realm, 0));

        // Safe to perform operations - array is reachable through return value
        for (size_t i = 0; i < 10; ++i) {
            TRY(array->create_data_property_or_throw(i, Value(i * i)));
        }

        return array;
    }

    // ❌ WRONG: Storing raw pointer
    static void wrong_pattern(VM& vm)
    {
        auto& realm = *vm.current_realm();

        // ❌ Don't store as raw pointer!
        Object* obj = realm.heap().allocate<Object>(realm, realm.intrinsics().object_prototype()).ptr();

        // GC can run here and collect obj if nothing else references it!
        // ... dangerous code ...
    }

    // ✅ CORRECT: Use GC::Ref or GC::Ptr
    static void correct_pattern(VM& vm)
    {
        auto& realm = *vm.current_realm();

        // ✅ Store as GC::Ref
        GC::Ref<Object> obj = realm.heap().allocate<Object>(realm, realm.intrinsics().object_prototype());

        // obj is now protected from GC
        // ... safe code ...
    }
};

// Example 4: Common patterns with GC types
class GCPatterns {
public:
    // Pattern: Nullable reference that may be set later
    void nullable_reference_example(Realm& realm)
    {
        GC::Ptr<Object> maybe_object;  // Initialized to nullptr

        if (some_condition()) {
            maybe_object = Object::create(realm, nullptr);
        }

        // Check before use
        if (maybe_object) {
            // Use *maybe_object or maybe_object.ptr()
        }
    }

    // Pattern: Converting between GC::Ref and GC::Ptr
    void reference_conversion(GC::Ref<Object> obj_ref)
    {
        // GC::Ref -> GC::Ptr (implicit)
        GC::Ptr<Object> obj_ptr = obj_ref;

        // GC::Ptr -> GC::Ref (explicit, check first!)
        if (obj_ptr) {
            GC::Ref<Object> back_to_ref = *obj_ptr;
        }

        // Get raw pointer (use sparingly!)
        Object* raw = obj_ref.ptr();  // Valid as long as obj_ref is reachable
    }

    // Pattern: Collections of GC objects
    void collection_patterns(Realm& realm)
    {
        // Vector of GC references
        Vector<GC::Ref<Object>> objects;
        objects.append(Object::create(realm, nullptr));
        objects.append(Object::create(realm, nullptr));

        // HashMap with GC values
        HashMap<String, GC::Ptr<Object>> object_map;
        object_map.set("key"_string, Object::create(realm, nullptr));

        // Don't forget to visit in visit_edges!
    }

    // Pattern: Return values from functions
    static GC::Ref<Object> returns_reference(Realm& realm)
    {
        // Return GC::Ref for non-null returns
        return Object::create(realm, nullptr);
    }

    static GC::Ptr<Object> returns_nullable(Realm& realm, bool create)
    {
        // Return GC::Ptr for nullable returns
        if (create)
            return Object::create(realm, nullptr);
        return nullptr;
    }
};

// Example 5: visit_edges patterns for different data structures
class ComplexObject : public Object {
    JS_OBJECT(ComplexObject, Object);
    GC_DECLARE_ALLOCATOR(ComplexObject);

protected:
    virtual void visit_edges(Cell::Visitor& visitor) override
    {
        Base::visit_edges(visitor);

        // Visit single GC pointer
        visitor.visit(m_single_object);

        // Visit Vector of GC objects
        for (auto& obj : m_object_vector)
            visitor.visit(obj);

        // Visit HashMap with GC keys or values
        for (auto& entry : m_object_map)
            visitor.visit(entry.value);

        // Visit Optional<GC::Ref<T>>
        if (m_optional_object.has_value())
            visitor.visit(m_optional_object.value());

        // Visit Variant with GC types
        m_variant.visit(
            [&](GC::Ref<Object>& obj) { visitor.visit(obj); },
            [&](GC::Ptr<PrimitiveString>& str) { visitor.visit(str); },
            [&](Empty) { /* Nothing to visit */ }
        );

        // Visit nested struct/class with GC members
        m_nested_data.visit_edges(visitor);
    }

private:
    struct NestedData {
        GC::Ptr<Object> object;

        void visit_edges(Cell::Visitor& visitor) const
        {
            visitor.visit(object);
        }
    };

    GC::Ptr<Object> m_single_object;
    Vector<GC::Ref<Object>> m_object_vector;
    HashMap<String, GC::Ptr<Object>> m_object_map;
    Optional<GC::Ref<PrimitiveString>> m_optional_object;
    Variant<GC::Ref<Object>, GC::Ptr<PrimitiveString>, Empty> m_variant;
    NestedData m_nested_data;
};

}

/*
 * Key GC Patterns Summary:
 *
 * 1. Use GC::Ref<T> for non-null references
 *    - Cannot be null
 *    - Implicit conversion from allocation
 *    - Guarantees object exists
 *
 * 2. Use GC::Ptr<T> for nullable references
 *    - Can be null
 *    - Check before dereferencing: if (ptr) { *ptr }
 *    - Implicit conversion from GC::Ref
 *
 * 3. ALWAYS implement visit_edges for GC-managed members
 *    - Call Base::visit_edges(visitor) first
 *    - Visit every GC::Ptr and GC::Ref member
 *    - Visit collections, optionals, variants
 *
 * 4. Use heap().allocate<T>() for allocation
 *    - Returns GC::Ref<T>
 *    - Automatically tracked by GC
 *    - Use TRY_OR_THROW_OOM for OOM handling
 *
 * 5. NEVER use raw pointers for GC objects
 *    - Object* ptr - dangling after GC!
 *    - Use GC::Ptr<Object> instead
 *
 * Common Mistakes:
 * ❌ Object* m_member;               // Will crash!
 * ❌ new MyObject()                  // Memory leak!
 * ❌ Forgetting visit_edges()        // Objects disappear!
 * ❌ Not visiting collection items   // Objects disappear!
 *
 * Correct Patterns:
 * ✅ GC::Ptr<Object> m_member;
 * ✅ heap().allocate<MyObject>(...)
 * ✅ virtual void visit_edges(Cell::Visitor& visitor) override
 * ✅ for (auto& item : m_items) visitor.visit(item);
 */
