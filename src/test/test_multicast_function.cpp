//
// Created by Estelle on 2024-08-27.
//
#include "../delegate/multicast_function.h"
#include "../reference_safe_delegate/reference_safe_delegate.h"
#include <gtest/gtest.h>

using namespace auto_delegate;
using namespace auto_reference;

inline uint64_t invoke_hash = 0;
inline uint64_t static_hash = rand();

struct type1
{
    int value;

    auto operator<=>(const type1&) const = default;
};

#define ARG_LIST type1 t1, type1 &t2, const type1 t1c, const type1 &t2c
#define ARG_LIST_FORWARD t1, std::ref(t2), std::forward<const type1>(t1c), std::cref(t2c)

struct
{
    type1 v1{1};
    type1 v2{2};
    type1 v3{3};
    type1 v4{4};
} params;

struct B : public generic_ref_reflector
{
    std::string name;

    B(std::string name) : name(name) {}

    uint64_t hash()
    {
        return std::hash<std::string>()(name);
    }

    void action(ARG_LIST) noexcept
    {
        invoke_hash += hash();
        assert(t1 == params.v1);
        assert(t2 == params.v2);
        assert(t1c == params.v3);
        assert(t2c == params.v4);
    }

    int function(ARG_LIST) noexcept
    {
        invoke_hash += hash();
        assert(t1 == params.v1);
        assert(t2 == params.v2);
        assert(t1c == params.v3);
        assert(t2c == params.v4);
        return 0;
    }

    virtual void virtual_action(ARG_LIST) noexcept
    {
        invoke_hash += hash();
        assert(t1 == params.v1);
        assert(t2 == params.v2);
        assert(t1c == params.v3);
        assert(t2c == params.v4);
    }

    virtual int virtual_function(ARG_LIST) noexcept
    {
        invoke_hash += hash();
        assert(t1 == params.v1);
        assert(t2 == params.v2);
        assert(t1c == params.v3);
        assert(t2c == params.v4);
        return 0;
    }

    int operator()(ARG_LIST) noexcept
    {
        invoke_hash += hash();
        assert(t1 == params.v1);
        assert(t2 == params.v2);
        assert(t1c == params.v3);
        assert(t2c == params.v4);
        return 0;
    }

    static int static_function(ARG_LIST) noexcept
    {
        invoke_hash += static_hash;
        assert(t1 == params.v1);
        assert(t2 == params.v2);
        assert(t1c == params.v3);
        assert(t2c == params.v4);
        return 0;
    }
};

#define PARAM_LIST params.v1, params.v2, params.v3, params.v4

TEST(multicast_function, auto_ref_object_action)
{
    using A = multicast_function<void(ARG_LIST)>;
    auto a = new A();
    auto b1 = new B("b1");
    auto b2 = new B("b2");

    auto& a_ref = *a;
    auto_ref_binder(b1) | bind_memfn<&B::action>;
    a_ref += auto_ref_binder(b1) | bind_memfn<&B::action>;

    a_ref += auto_ref_binder(b2) | bind_into_lambda([](auto&& b, ARG_LIST) { b.action(ARG_LIST_FORWARD); });

    invoke_hash = 0;
    a->invoke(PARAM_LIST);
    ASSERT_EQ(invoke_hash, b1->hash() + b2->hash());

    // move b1
    auto temp = new B(std::move(*b1));
    delete b1;
    b1 = temp;

    invoke_hash = 0;
    a->invoke(PARAM_LIST);
    ASSERT_EQ(invoke_hash, b1->hash() + b2->hash());

    delete b2;

    invoke_hash = 0;
    a->invoke(PARAM_LIST);
    ASSERT_EQ(invoke_hash, b1->hash());

    b2 = new B("new b2");

    a->bind(auto_ref_binder(b2) | bind_into_lambda([](auto&& b, ARG_LIST) { b.action(ARG_LIST_FORWARD); }));

    auto tempa = new A(std::move(*a));
    delete a;
    a = tempa;

    invoke_hash = 0;
    a->invoke(PARAM_LIST);
    ASSERT_EQ(invoke_hash, b1->hash() + b2->hash());
}

TEST(multicast_function, auto_ref_object_function)
{
    using A = multicast_function<int(ARG_LIST)>;

    A* a = new A();
    auto b1 = new B("b1");
    auto b2 = new B("b2");

    size_t hash = 0;

    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();

    a->bind(auto_ref_binder(b2) | bind_memfn<&B::virtual_function>);
    hash += b2->hash();


    auto static_function_h = a->bind_unique_handled<&B::static_function>();

    hash += static_hash;

    // a->bind(b1);
    // hash += b1->hash();

    auto static_lambda_h = a->bind_unique_handled([](ARG_LIST)
                                                  {
                                                      invoke_hash += static_hash;
                                                      assert(t1 == params.v1);
                                                      assert(t2 == params.v2);
                                                      assert(t1c == params.v3);
                                                      assert(t2c == params.v4);
                                                      return 0;
                                                  });
    hash += static_hash;

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    hash -= b2->hash();
    hash -= b1->hash();
    delete b2;
    delete b1;

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    static_function_h.unbind();
    hash -= static_hash;
    static_lambda_h.unbind();
    hash -= static_hash;

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    b1 = new B("new b1");
    b2 = new B("new b2");
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();
    a->bind(auto_ref_binder(b1) | bind_memfn<&B::virtual_function>);
    hash += b1->hash();

    auto tempa = new A(std::move(*a));
    delete a;
    a = tempa;

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    delete b1;
    delete a;
    delete b2;
}

TEST(multicast_function, function_bind)
{
    using A = multicast_function<int(ARG_LIST)>;

    A* a = new A();
    auto b1 = new B("b1");
    auto b2 = std::make_shared<B>("b2");
    auto b3 = std::make_shared<B>("b3");

    size_t hash = 0;


    a->bind<&B::function>(b1);
    hash += b1->hash();

    // auto_ref_binder(b1) | bind_memfn<&B::function> | bind_handle;

    //inbuild bind
    a->bind_weak<&B::function>(b2);
    hash += b2->hash();
    a->bind_weak(b3, [](auto&& o, ARG_LIST) { return o.function(ARG_LIST_FORWARD); });
    hash += b3->hash();

    auto& a_ref = *a;
    // extension binding
    // bind shared
    auto memfn_handle = a_ref += shared_binder(b2)
    | bind_into_lambda([](auto&& o, ARG_LIST) { return o.function(ARG_LIST_FORWARD); })
    | bind_handle;
    hash += b2->hash();
    a_ref += weak_binder(b3) | bind_memfn<&B::function>;
    hash += b3->hash();
    a_ref += weak_binder(b3) | bind_memfn<&B::virtual_function>;
    hash += b3->hash();

    auto static_function_h = a->bind_unique_handled<&B::static_function>();

    hash += static_hash;

    auto static_lambda_h = a->bind_unique_handled([](ARG_LIST)
                                                  {
                                                      invoke_hash += static_hash;
                                                      assert(t1 == params.v1);
                                                      assert(t2 == params.v2);
                                                      assert(t1c == params.v3);
                                                      assert(t2c == params.v4);
                                                      return 0;
                                                  });
    hash += static_hash;

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    static_function_h.unbind();
    hash -= static_hash;
    static_lambda_h.unbind();
    hash -= static_hash;

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    hash -= b3->hash();
    hash -= b3->hash();
    hash -= b3->hash();
    b3.reset();



    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    auto b2_hash = b2->hash();
    b2.reset();

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    hash -= b2_hash;
    hash -= b2_hash;
    memfn_handle.unbind();

    invoke_hash = 0;
    a->for_each_invoke(PARAM_LIST,
                       [](auto&& results)
                       {
                           ASSERT_EQ(results, 0);
                       });
    ASSERT_EQ(invoke_hash, hash);

    hash -= b1->hash();
    delete b1;
    delete a;
}
