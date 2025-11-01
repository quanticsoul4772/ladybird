/*
 * Example: Implementing a Custom Object Type in LibJS
 *
 * This example shows how to create a complete custom object with:
 * - Custom object class
 * - Constructor function
 * - Prototype with methods
 * - Property accessors
 * - GC integration
 *
 * Example: Point object with x, y coordinates
 */

// ===== Point.h =====

#pragma once

#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/NativeFunction.h>

namespace JS {

// The Point instance object
class Point final : public Object {
    JS_OBJECT(Point, Object);
    GC_DECLARE_ALLOCATOR(Point);

public:
    static GC::Ref<Point> create(Realm&, double x, double y);

    virtual ~Point() override = default;

    double x() const { return m_x; }
    double y() const { return m_y; }

    void set_x(double x) { m_x = x; }
    void set_y(double y) { m_y = y; }

protected:
    Point(Realm&, double x, double y);

    virtual void initialize(Realm&) override;

private:
    double m_x { 0.0 };
    double m_y { 0.0 };
};

// The constructor function: new Point(x, y)
class PointConstructor final : public NativeFunction {
    JS_OBJECT(PointConstructor, NativeFunction);
    GC_DECLARE_ALLOCATOR(PointConstructor);

public:
    virtual void initialize(Realm&) override;
    virtual ~PointConstructor() override = default;

    virtual ThrowCompletionOr<Value> call() override;
    virtual ThrowCompletionOr<GC::Ref<Object>> construct(FunctionObject& new_target) override;

private:
    explicit PointConstructor(Realm&);

    virtual bool has_constructor() const override { return true; }

    // Static methods: Point.distance(p1, p2)
    JS_DECLARE_NATIVE_FUNCTION(distance);
};

// The prototype object: Point.prototype
class PointPrototype final : public Object {
    JS_OBJECT(PointPrototype, Object);
    GC_DECLARE_ALLOCATOR(PointPrototype);

public:
    virtual void initialize(Realm&) override;

private:
    explicit PointPrototype(Realm&);

    // Instance methods
    JS_DECLARE_NATIVE_FUNCTION(magnitude);
    JS_DECLARE_NATIVE_FUNCTION(to_string);
    JS_DECLARE_NATIVE_FUNCTION(move_by);

    // Getters/setters
    JS_DECLARE_NATIVE_FUNCTION(x_getter);
    JS_DECLARE_NATIVE_FUNCTION(x_setter);
    JS_DECLARE_NATIVE_FUNCTION(y_getter);
    JS_DECLARE_NATIVE_FUNCTION(y_setter);
};

}

// ===== Point.cpp =====

#include <LibJS/Runtime/Point.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Realm.h>
#include <math.h>

namespace JS {

// ===== Point Object Implementation =====

GC_DEFINE_ALLOCATOR(Point);

GC::Ref<Point> Point::create(Realm& realm, double x, double y)
{
    return realm.heap().allocate<Point>(realm, x, y);
}

Point::Point(Realm& realm, double x, double y)
    : Object(ConstructWithPrototypeTag::Tag, realm.intrinsics().point_prototype())
    , m_x(x)
    , m_y(y)
{
}

void Point::initialize(Realm& realm)
{
    Base::initialize(realm);

    // Points don't need custom initialization in this example
}

// ===== Point Constructor Implementation =====

GC_DEFINE_ALLOCATOR(PointConstructor);

PointConstructor::PointConstructor(Realm& realm)
    : NativeFunction(realm.vm().names.Point.as_string(),
                     realm.intrinsics().function_prototype())
{
}

void PointConstructor::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    // Constructor.length = 2 (number of required parameters)
    define_direct_property(vm.names.length, Value(2), Attribute::Configurable);

    // Constructor.prototype = Point.prototype
    define_direct_property(vm.names.prototype,
        realm.intrinsics().point_prototype(),
        Attribute::None);  // Non-writable, non-enumerable, non-configurable

    // Static method: Point.distance(p1, p2)
    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(realm, vm.names.distance, distance, 2, attr);
}

// new Point(x, y) - Called with 'new'
ThrowCompletionOr<GC::Ref<Object>> PointConstructor::construct(FunctionObject&)
{
    auto& vm = this->vm();
    auto& realm = *vm.current_realm();

    // Get x coordinate (default to 0)
    auto x_value = vm.argument(0);
    auto x = x_value.is_undefined() ? 0.0 : TRY(x_value.to_number(vm)).as_double();

    // Get y coordinate (default to 0)
    auto y_value = vm.argument(1);
    auto y = y_value.is_undefined() ? 0.0 : TRY(y_value.to_number(vm)).as_double();

    return Point::create(realm, x, y);
}

// Point(x, y) - Called without 'new'
ThrowCompletionOr<Value> PointConstructor::call()
{
    auto& vm = this->vm();
    return vm.throw_completion<TypeError>(ErrorType::ConstructorWithoutNew, "Point");
}

// Point.distance(p1, p2) - Static method
JS_DEFINE_NATIVE_FUNCTION(PointConstructor::distance)
{
    auto& realm = *vm.current_realm();

    // Validate arguments are Point objects
    auto p1_value = vm.argument(0);
    if (!p1_value.is_object() || !is<Point>(p1_value.as_object()))
        return vm.throw_completion<TypeError>(ErrorType::NotAnObjectOfType, "Point");

    auto p2_value = vm.argument(1);
    if (!p2_value.is_object() || !is<Point>(p2_value.as_object()))
        return vm.throw_completion<TypeError>(ErrorType::NotAnObjectOfType, "Point");

    auto& p1 = static_cast<Point&>(p1_value.as_object());
    auto& p2 = static_cast<Point&>(p2_value.as_object());

    // Calculate distance: sqrt((x2-x1)^2 + (y2-y1)^2)
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double distance = sqrt(dx * dx + dy * dy);

    return Value(distance);
}

// ===== Point Prototype Implementation =====

GC_DEFINE_ALLOCATOR(PointPrototype);

PointPrototype::PointPrototype(Realm& realm)
    : Object(ConstructWithPrototypeTag::Tag, realm.intrinsics().object_prototype())
{
}

void PointPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    u8 attr = Attribute::Writable | Attribute::Configurable;

    // Instance methods
    define_native_function(realm, vm.names.magnitude, magnitude, 0, attr);
    define_native_function(realm, vm.names.toString, to_string, 0, attr);
    define_native_function(realm, vm.names.moveBy, move_by, 2, attr);

    // Property accessors: get x, set x, get y, set y
    define_native_accessor(realm, vm.names.x, x_getter, x_setter, attr);
    define_native_accessor(realm, vm.names.y, y_getter, y_setter, attr);
}

// Helper: Get 'this' as Point
static ThrowCompletionOr<Point*> typed_this_point(VM& vm)
{
    auto this_value = vm.this_value();
    if (!this_value.is_object() || !is<Point>(this_value.as_object()))
        return vm.throw_completion<TypeError>(ErrorType::NotAnObjectOfType, "Point");
    return static_cast<Point*>(&this_value.as_object());
}

// point.magnitude() - Returns distance from origin
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::magnitude)
{
    auto* point = TRY(typed_this_point(vm));

    double mag = sqrt(point->x() * point->x() + point->y() * point->y());
    return Value(mag);
}

// point.toString() - Returns "[Point: x, y]"
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::to_string)
{
    auto* point = TRY(typed_this_point(vm));

    auto string = MUST(String::formatted("[Point: {}, {}]", point->x(), point->y()));
    return PrimitiveString::create(vm, string);
}

// point.moveBy(dx, dy) - Mutates point coordinates
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::move_by)
{
    auto* point = TRY(typed_this_point(vm));

    auto dx = TRY(vm.argument(0).to_number(vm)).as_double();
    auto dy = TRY(vm.argument(1).to_number(vm)).as_double();

    point->set_x(point->x() + dx);
    point->set_y(point->y() + dy);

    return js_undefined();
}

// get point.x
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::x_getter)
{
    auto* point = TRY(typed_this_point(vm));
    return Value(point->x());
}

// set point.x = value
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::x_setter)
{
    auto* point = TRY(typed_this_point(vm));
    auto value = TRY(vm.argument(0).to_number(vm)).as_double();
    point->set_x(value);
    return js_undefined();
}

// get point.y
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::y_getter)
{
    auto* point = TRY(typed_this_point(vm));
    return Value(point->y());
}

// set point.y = value
JS_DEFINE_NATIVE_FUNCTION(PointPrototype::y_setter)
{
    auto* point = TRY(typed_this_point(vm));
    auto value = TRY(vm.argument(0).to_number(vm)).as_double();
    point->set_y(value);
    return js_undefined();
}

}

/*
 * Example JavaScript Usage:
 *
 * // Create points
 * const p1 = new Point(3, 4);
 * const p2 = new Point(0, 0);
 *
 * // Access properties
 * console.log(p1.x);           // 3
 * console.log(p1.y);           // 4
 *
 * // Set properties
 * p1.x = 5;
 * p1.y = 12;
 *
 * // Call methods
 * console.log(p1.magnitude()); // 13
 * console.log(p1.toString());  // "[Point: 5, 12]"
 *
 * // Mutate
 * p1.moveBy(1, 1);
 * console.log(p1.x);           // 6
 *
 * // Static method
 * const dist = Point.distance(p1, p2);
 * console.log(dist);           // ~13.038
 */

/*
 * Key Patterns Demonstrated:
 *
 * 1. Object Hierarchy:
 *    - Instance (Point)
 *    - Constructor (PointConstructor)
 *    - Prototype (PointPrototype)
 *
 * 2. GC Integration:
 *    - JS_OBJECT macro
 *    - GC_DECLARE_ALLOCATOR / GC_DEFINE_ALLOCATOR
 *    - heap().allocate<T>()
 *
 * 3. Constructor Pattern:
 *    - construct() for 'new Point()'
 *    - call() for 'Point()' (usually throws)
 *    - has_constructor() returns true
 *
 * 4. Property Accessors:
 *    - define_native_accessor(name, getter, setter, attr)
 *    - Getter/setter are separate native functions
 *
 * 5. Type Checking:
 *    - typed_this_point() helper validates 'this'
 *    - is<Point>(object) checks type
 *    - static_cast<Point&> after validation
 */
