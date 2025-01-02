# Auto Delegate

Auto Delegate is a C++23 header-only library that implements lightweight delegates for type-erased functions. It provides powerful yet simple tools to bind and invoke member functions, static functions, and lambdas without the need for external dependencies.

## Features

- **Type-Erased Function Binding**: Easily bind member functions, static functions, or stateless lambdas.
- **Multicast Delegates**: Support multiple bindings for a single delegate with `multicast_delegate`.
- **Function-Style Invocation**: Invoke bound functions using either callable or function-style syntax.
- **Flexible Handles**: Manage bindings using handle objects for fine-grained control.
- **Stateless and Lightweight**: Header-only implementation with minimal overhead.

## Installation

Auto Delegate is a header-only library. To use it, simply include the necessary headers in your project:

```cpp
#include "delegate/multicast_delegate.h"
#include "delegate/multicast_function.h"
```

## Build

This project is built using XMake.

Three targets are available: `DelegateTest` `DelegateBenchmark` `DelegateSample`

## Usage Examples

### Delegate Usage

```cpp
#include "delegate/multicast_delegate.h"
#include <iostream>

void delegate_sample() {
    class MyClass {
    public:
        int value = 10;
        int Func(int a, int b) { return a + b + value; }
        virtual int VFunc(int a, int b) { return a + b + value; }
        static int StaticFunc(int a, int b) { return a + b; }
        int OverLoadFunc(int a, int b) { return a + b + value; }
        int OverLoadFunc() { return value; }
    };

    using namespace auto_delegate;

    MyClass obj;

    // Bind methods - constructor
    delegate<int(int, int)> d(&obj, func_tag<&MyClass::Func>());

    // Bind/rebind methods
    d.bind<&MyClass::Func>(&obj);
    // Bind with lambda
    // Note: capture is not supported
    d.bind(&obj, [](auto&& o, int a, int b) { return o.Func(a, b) + 1; });
    // Bind static func
    d.bind<&MyClass::StaticFunc>();
    // Bind stateless callable
    // Note: capture is not supported
    d.bind([](int a, int b) { return a + b; });

    // Call methods - callable style
    auto res1 = d(1, 2);
    // Call methods - function style
    auto res2 = d.invoke(1, 2);

    // Unbind
    d.reset();
    // Bool cast
    bool b = d;
}
```

### Multicast Delegate Usage

```cpp
#include "delegate/multicast_delegate.h"
#include <iostream>

void multicast_delegate_sample() {
    class MyClass {
    public:
        int value = 10;
        void Action(int a, int b) { std::cout << a + b + value << std::endl; }
        int Func(int a, int b) { return a + b + value; }
        virtual int VFunc(int a, int b) { return a + b + value; }
        static int StaticFunc(int a, int b) { return a + b; }
        int OverLoadFunc(int a, int b) { return a + b + value; }
        int OverLoadFunc() { return value; }
    };

    using namespace auto_delegate;

    // With return
    {
        multicast_delegate<int(int, int)> d;

        MyClass obj;

        // Bind methods
        auto h1 = d.bind<&MyClass::Func>(&obj);
        // Bind overload method
        auto h2 = d.bind<MyClass, &MyClass::OverLoadFunc>(&obj);
        // Bind with callable
        // Note: capture is not supported
        auto h3 = d.bind(&obj, [](auto&& o, int a, int b) { return o.Func(a, b) + 1; });
        // Bind static func
        auto h4 = d.bind<&MyClass::StaticFunc>();
        // Bind stateless callable
        // Note: capture is not supported
        auto h5 = d.bind([](int a, int b) { return a + b; });

        // Invoke
        d.for_each_invoke(1, 2, [](auto&& res) { std::cout << res << std::endl; });

        // Unbind the related delegate
        h1.unbind();
        // Give up the ownership of the delegate
        // The delegate stays in the invoke list
        h2.release();

        {
            auto h = d.bind<&MyClass::Func>(&obj);
            // The delegate will be unbound once the handle is destructed
        }
    }

    // No return
    {
        multicast_delegate<void(int, int)> d;

        MyClass obj;

        auto h = d.bind<&MyClass::Action>(&obj);

        d.invoke(1, 2);
    }
}
```

### Multicast Function Usage

```cpp
#include "delegate/multicast_function.h"
#include <iostream>

void multicast_function_sample() {
    class MyClass {
    public:
        int value = 10;
        void Action(int a, int b) { std::cout << a + b + value << std::endl; }
        int Func(int a, int b) { return a + b + value; }
        static int StaticFunc(int a, int b) { return a + b; }
        int OverLoadFunc(int a, int b) { return a + b + value; }
        int OverLoadFunc() { return value; }
    };

    using namespace auto_delegate;

    multicast_function<int(int, int)> mf;

    MyClass obj;
    int capture = 10;

    // Bind lambda
    mf += [&obj](int a, int b) { return obj.Func(a, b); };
    // Bind function pointer
    mf += &MyClass::StaticFunc;

    // Bind member function
    mf += binder(&obj) | bind_memfn<&MyClass::Func>;
    // Bind with lambda
    mf += binder(&obj) |
          bind_into_lambda([capture](auto&& o, int a, int b) { return o.OverLoadFunc(a, b) + capture; });

    // Bind with handle
    auto h1 = mf +=
            binder(&obj) |
            bind_memfn<&MyClass::Func> |
            bind_handle;

    auto h2 = mf +=
            binder(&obj) |
            bind_into_lambda([capture](auto&& o, int a, int b) { return o.OverLoadFunc(a, b) + capture; }) |
            bind_handle;

    // Shared/weak ptr
    // Weak ptr binding will auto-remove from the invoke list
    auto shared = std::make_shared<MyClass>();

    auto shared_h = mf += shared_binder(shared) |
                          bind_memfn<&MyClass::Func> |
                          bind_handle;

    mf += weak_binder(shared) |
          bind_memfn<&MyClass::Func>;

    mf.for_each_invoke(1, 2, [&](auto&& res) { std::cout << res << std::endl; });
}
```

## Naming and Functional Differences

### `function` vs. `delegate`

- **`delegate`**: Similar to `function_ref`. It does not perform memory allocation, making it a lightweight and efficient binding solution for callable objects without state.
- **`multicast_function`**: Designed to handle stateful closures or lambdas. It provides more flexibility for dynamic bindings but incurs higher memory usage due to its ability to manage stateful objects.

### Highlights of `multicast_delegate`

One of the standout features of `auto_delegate` is the rich functionality provided by `multicast_delegate`. For instance, `multicast_delegate` supports handle-based bindings:

```cpp
auto h1 = mf +=
        binder(&obj) |
        bind_memfn<&MyClass::Func> |
        bind_handle;
```

This binding mechanism provides a unique handle, which automatically removes the binding when the handle is destructed. Additionally:

- Handles do not require their lifespan to be shorter than that of the `multicast_delegate`.
- If the closure or bound object is destroyed, its associated handle becomes invalid automatically, ensuring robust and predictable behavior.
- Both handle operations and closure invalidation are **O(1)** in complexity.

Furthermore, `shared_binder` and `weak_binder` are specifically designed for bindings involving smart pointers:

- **`shared_binder`**: Binds a closure with a `shared_ptr`. The closure is automatically invalidated and removed from the invoke list if the `shared_ptr` is reset.
- **`weak_binder`**: Binds a closure with a `weak_ptr`. It prevents invocation if the `weak_ptr` is no longer valid and removes the closure from the invoke list.

These features ensure efficient and error-resistant management of closures, particularly in complex, dynamic environments.

## API Overview

### Classes

- `delegate<R(Args...)>`: A single-cast delegate for binding and invoking one callable.
- `multicast_delegate<R(Args...)>`: A multicast delegate for binding and invoking multiple callables.
- `multicast_function<R(Args...)>`: A more dynamic version of `multicast_delegate` with similar functionality.

### Functions

- `bind`: Binds a callable to a delegate.
- `reset`: Clears all bindings.
- `invoke`: Invokes the bound callable(s).
- `for_each_invoke`: Iterates over all callables and invokes them.

## License

This library is open-source and licensed under the MIT License.

## Contributing

Contributions, bug reports, and feature requests are welcome! Feel free to submit a pull request or open an issue on the repository.

