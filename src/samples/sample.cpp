#include <iostream>
#include <any>

#include "../delegate/multicast_delegate.h"
#include "../delegate/multicast_function.h"

void delegate_sample()
{
    class MyClass
    {
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

    // bind methods - constructor
    delegate<int(int, int)> d(&obj, func_tag<&MyClass::Func>());

    // bind/rebind methods
    d.bind<&MyClass::Func>(&obj);
    // bind with lambda
    // note : capture is not supported
    d.bind(&obj, [](auto&& o, int a, int b) { return o.Func(a, b) + 1; });
    // bind static func
    d.bind<&MyClass::StaticFunc>();
    // bind stateless callable
    // note : capture is not supported
    d.bind([](int a, int b) { return a + b; });

    // call methods - callable style
    auto res1 = d(1, 2) ;
    // call methods - function style
    auto res2 = d.invoke(1, 2);

    // unbind
    d.reset();
    // bool cast
    bool b = d;
}


void multicast_delegate_sample()
{
    class MyClass
    {
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

    //with return
    {
        multicast_delegate<int(int, int)> d;

        MyClass obj;

        // bind methods
        auto h1 = d.bind<&MyClass::Func>(&obj);
        // bind overload method
        auto h2 = d.bind<MyClass, &MyClass::OverLoadFunc>(&obj);
        // bind with callable
        // note : capture is not supported
        auto h3 = d.bind(&obj, [](auto&& o, int a, int b) { return o.Func(a, b) + 1; });
        // bind static func
        auto h4 = d.bind<&MyClass::StaticFunc>();
        // bind stateless callable
        // note : capture is not supported
        auto h5 = d.bind([](int a, int b) { return a + b; });

        //invoke
        d.for_each_invoke(1, 2, [](auto&& res) { std::cout << res << std::endl; });

        // unbind the related delegate
        h1.unbind();
        // give up the ownership of the delegate
        // the delegate stay in the invoke list
        h2.release();

        {
            auto h = d.bind<&MyClass::Func>(&obj);
            //the delegate will be unbind once the handle destructed
        }
    }

    //no return
    {
        multicast_delegate<void(int, int)> d;

        MyClass obj;

        auto h = d.bind<&MyClass::Action>(&obj);

        d.invoke(1, 2);
    }
}


void multicast_function_sample()
{
    class MyClass
    {
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

    // bind lambda
    mf += [&obj](int a, int b) { return obj.Func(a, b); };
    // bind function pointer
    mf += &MyClass::StaticFunc;

    // bind member function
    mf += binder(&obj) | bind_memfn<&MyClass::Func>;
    // bind with lambda
    mf += binder(&obj) |
          bind_into_lambda([capture](auto&& o, int a, int b) { return o.OverLoadFunc(a, b) + capture; });

    // bind with handle
    auto h1 = mf +=
            binder(&obj) |
            bind_memfn<&MyClass::Func> |
            bind_handle;

    auto h2 = mf +=
            binder(&obj) |
            bind_into_lambda([capture](auto&& o, int a, int b) { return o.OverLoadFunc(a, b) + capture; }) |
            bind_handle;

    // shared/weak ptr
    // weak ptr binding will auto remove from the invoke list
    auto shared = std::make_shared<MyClass>();

    auto shared_h = mf += shared_binder(shared) |
                          bind_memfn<&MyClass::Func> |
                          bind_handle;

    mf += weak_binder(shared) |
          bind_memfn<&MyClass::Func>;


    mf.for_each_invoke(1, 2, [&](auto&& res) { std::cout << res << std::endl; });
}


int main() { delegate_sample(); }