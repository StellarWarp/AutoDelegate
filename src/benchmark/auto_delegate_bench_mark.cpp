#include <benchmark/benchmark.h>
#include <random>
#include <functional>

#include "../reference_safe_delegate/reference_safe_delegate.h"


using namespace auto_delegate;

struct type1
{
    int value;
};

#define ARG_LIST type1 t1, type1& t2, const type1 t1c, const type1& t2c
#define ARG_LIST_FORWARD t1, std::ref(t2), std::forward<const type1>(t1c), std::cref(t2c)

struct A
{
    multicast_delegate<void(ARG_LIST)> event_default;
    multicast_auto_delegate<void(ARG_LIST)> event;
    multicast_auto_delegate<int(ARG_LIST)> event_ret;
    multicast_auto_delegate_extern_ref<void(ARG_LIST)> event_ref;
};

template<auto I>
struct B : public generic_ref_reflector
{
    int value;

    static const uint64_t test_scale = 1 << 4;

    B() = default;

    void work_load()
    {
//        benchmark::DoNotOptimize(I);
        value += I;
    }

    void action(ARG_LIST) noexcept
    {
        work_load();
    }

    int function(ARG_LIST) noexcept
    {
        work_load();
        return value;
    }

    virtual void virtual_action(ARG_LIST) noexcept
    {
        work_load();
    }

    virtual int virtual_function(ARG_LIST) noexcept
    {
        work_load();
        return value;
    }

    int operator()(ARG_LIST) noexcept
    {
        work_load();
        return value;

    }
};

struct C
{
    std::string name;

    C() = default;

    C(std::string name) : name(name) {}

    void action(ARG_LIST) noexcept
    {

    }

    int function(ARG_LIST) noexcept
    {
        return 0;
    }

    virtual void virtual_action(ARG_LIST) noexcept
    {
    }


    virtual int virtual_function(ARG_LIST) noexcept
    {
        return 0;
    }

    static int static_function(ARG_LIST) noexcept
    {
        return 0;
    }

    int operator()(ARG_LIST) noexcept
    {
        return 0;
    }
};

#ifdef NDEBUG
static constexpr size_t class_count = 1 << 5;
#else
static constexpr size_t class_count = 1 << 2;
#endif
static constexpr size_t object_count = 1 << 12;
static constexpr size_t per_class_count = object_count / class_count;

const auto min_warm_up = 1 << 10;
#ifdef NDEBUG
const uint64_t invoke_count = 1 << 16;
#else
const uint64_t invoke_count = 1 << 2;
#endif

#define BENCHMARK_ARGS ->Iterations(invoke_count)->MeasureProcessCPUTime()->Unit(benchmark::kMillisecond)->MinWarmUpTime(min_warm_up)


template<auto>
struct index_tag {};

struct ObjectArray
{
    std::vector<void*> raw_mem;

    using typed_object_list = decltype([]
    {
        return []<size_t...I>(std::index_sequence<I...>)
        {
            return std::tuple<B<I>* ...>{};
        }(std::make_index_sequence<class_count>{});
    }());

    std::vector<typed_object_list> objects;


    ObjectArray()
    {
        raw_mem.reserve(object_count);
        std::random_device rd;
        std::mt19937 eng(rd());
        objects.reserve(per_class_count);
        for (size_t i = 0; i < per_class_count; ++i)
        {
            objects.emplace_back([&]<size_t...I>(std::index_sequence<I...>)
                                 {
                                     auto new_object = [&]<size_t Index>(index_tag<Index>)
                                     {
                                         size_t mem_size = std::uniform_int_distribution<size_t>(1024, 2048)(eng);
#ifdef _MSC_VER
                                         void* mem = _aligned_malloc(sizeof(B<Index>), alignof(B<Index>));
#else
                                         void* mem = std::aligned_alloc(alignof(B<Index>),sizeof(B<Index>));
#endif
                                         if (!mem) throw std::bad_alloc();
                                         raw_mem.push_back(mem);
                                         B<Index>* b = new(mem) B<Index>();
                                         return b;
                                     };
                                     return std::tuple{new_object(index_tag<I>{})...};
                                 }(std::make_index_sequence<class_count>{}));
        }
    }

    ~ObjectArray()
    {
        for (auto ptr: raw_mem)
        {
#ifdef _MSC_VER
            _aligned_free(ptr);
#else
            std::free(ptr);
#endif
        }
    }

    static auto& Array()
    {
        static ObjectArray a;
        return a.objects;
    }

};

#define INVOKE_PARAMS PARAMS.v1, PARAMS.v2, PARAMS.v1, PARAMS.v2

struct
{
    type1 v1{1};
    type1 v2{2};
    type1 v3{3};
    type1 v4{4};
} PARAMS;


template<typename OArr = ObjectArray>
void ForEachObject(auto&& func)
{
    auto& objects = OArr::Array();
    for (auto& object_tuple: objects)
    {
        [&]<size_t...I>(std::index_sequence<I...>)
        {
            auto lambda = [&]<size_t Index>(index_tag<Index>)
            {
                auto ptr = std::get<Index>(object_tuple);
                func(ptr, index_tag<Index>());
            };
            (lambda(index_tag<I>()), ...);
        }(std::make_index_sequence<class_count>{});
    }
}


template<typename DelegateType, typename OArr = ObjectArray>
void TestTemplate(benchmark::State& state, auto&& bind, auto&& invoke)
{
    DelegateType d{};
    if constexpr (requires { typename DelegateType::delegate_handle_t; })
    {
        if constexpr (not std::is_void_v<typename DelegateType::delegate_handle_t>)
        {
            std::vector<typename DelegateType::delegate_handle_t> handles;
            handles.reserve(object_count);

            ForEachObject<OArr>([&](auto&& ptr, auto)
                                {
                                    handles.emplace_back(bind(d, ptr));
                                });

            for (auto _: state)
            {
                invoke(d);
            }
        } else
        {
            ForEachObject<OArr>([&](auto&& ptr, auto)
                                {
                                    bind(d, ptr);
                                });

            for (auto _: state)
            {
                invoke(d);
            }
        }
    }
    else
    {
        ForEachObject<OArr>([&](auto&& ptr, auto)
                            {
                                bind(d, ptr);
                            });

        for (auto _: state)
        {
            invoke(d);
        }
    }
}

inline std::vector<int> int_values(object_count, 0);

static void BM_WorkLoad(benchmark::State& state)
{
    for (auto _: state)
    {
        ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                      {
                          int_values[I] += I;
                      });
    }
}

BENCHMARK(BM_WorkLoad)BENCHMARK_ARGS;

static void BM_Inline_Function(benchmark::State& state)
{
    for (auto _: state)
    {
        ForEachObject([&](auto&& o, auto)
                      {
                          o->function(INVOKE_PARAMS);
                      });
    }
}

BENCHMARK(BM_Inline_Function)BENCHMARK_ARGS;

static void BM_Direct_Virtual_Function(benchmark::State& state)
{
    for (auto _: state)
    {
        ForEachObject([&](auto&& o, auto)
                      {
                          o->virtual_function(INVOKE_PARAMS);
                      });
    }
}
//BENCHMARK(BM_Direct_Virtual_Function)BENCHMARK_ARGS;



static void BM_Direct_FunctionPointer(benchmark::State& state)
{

    auto func_ptrs = []
    {
        return []<size_t...I>(std::index_sequence<I...>)
        {
            return std::tuple{&B<I>::function...};
        }(std::make_index_sequence<class_count>{});
    }();

    benchmark::DoNotOptimize(func_ptrs);


    for (auto _: state)
    {
        ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                      {
                          auto func_ptr = std::get<I>(func_ptrs);
                          (o->*func_ptr)(INVOKE_PARAMS);
                      });
    }
}
// BENCHMARK(BM_Direct_FunctionPointer)BENCHMARK_ARGS;


static void BM_FunctionPointer(benchmark::State& state)
{
    using func_t = decltype(&B<0>::function);
    using pair_t = std::pair<B<0>*, func_t>;
    std::vector<pair_t> funcs;
    funcs.reserve(object_count);

    ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                  {
                      funcs.emplace_back(pair_t{
                              (B<0>*) o,
                              (func_t) &B<I>::function
                      });
                  });

    for (auto& [o, func_ptr]: funcs)
    {
        benchmark::DoNotOptimize(func_ptr);
    }

    for (auto _: state)
    {
        for (auto& [o, func_ptr]: funcs)
        {
            (o->*func_ptr)(INVOKE_PARAMS);
        }
    }
}

BENCHMARK(BM_FunctionPointer)BENCHMARK_ARGS;

static void BM_Lambda(benchmark::State& state)
{
    using func_t = int (*)(void*, ARG_LIST);
    using pair_t = std::pair<B<0>*, func_t>;
    std::vector<pair_t> funcs;
    funcs.reserve(object_count);

    ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                  {
                      funcs.emplace_back(pair_t{
                              (B<0>*) o,
                              [](void* o, ARG_LIST) -> int
                              {
                                  return ((B<I>*) o)->function(INVOKE_PARAMS);
                              }
                      });
                  });

    for (auto& [o, func_ptr]: funcs)
    {
        benchmark::DoNotOptimize(func_ptr);
    }

    for (auto _: state)
    {
        for (auto& [o, func_ptr]: funcs)
        {
            func_ptr(o, INVOKE_PARAMS);
        }
    }
}
// BENCHMARK(BM_Lambda)BENCHMARK_ARGS;



static void BM_Virtual_FunctionPointer(benchmark::State& state)
{
    using func_t = decltype(&B<0>::virtual_function);
    using pair_t = std::pair<B<0>*, func_t>;
    std::vector<pair_t> funcs;
    funcs.reserve(object_count);

    ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                  {
                      funcs.emplace_back(pair_t{
                              (B<0>*) o,
                              (func_t) &B<I>::virtual_function
                      });
                  });

    for (auto _: state)
    {
        for (auto& [o, func_ptr]: funcs)
        {
            (o->*func_ptr)(INVOKE_PARAMS);
        }
    }
}
//BENCHMARK(BM_Virtual_FunctionPointer)BENCHMARK_ARGS;


static void BM_StdFunction(benchmark::State& state)
{
    std::vector<std::function<void(ARG_LIST)>> funcs;
    funcs.reserve(object_count);

    ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                  {
                      funcs.emplace_back([o](ARG_LIST)
                                         {
                                             o->function(ARG_LIST_FORWARD);
                                         });
                  });

//    static_assert(sizeof(std::function<void(ARG_LIST)>) == 32);

    for (auto _: state)
    {
        for (auto& f: funcs)
        {
            f(INVOKE_PARAMS);
        }
    }
}

BENCHMARK(BM_StdFunction)BENCHMARK_ARGS;

static void BM_MyFunction(benchmark::State& state)
{
    std::vector<function<void(ARG_LIST)>> funcs;
    funcs.reserve(object_count);

    ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                  {
                      funcs.emplace_back([o](ARG_LIST)
                                         {
                                             o->function(ARG_LIST_FORWARD);
                                         });
                  });

    for (auto _: state)
    {
        for (auto& f: funcs)
        {
            f(INVOKE_PARAMS);
        }
    }
}

BENCHMARK(BM_MyFunction)BENCHMARK_ARGS;

static void BM_FunctionV2(benchmark::State& state)
{
    std::vector<function_v2::function<void(ARG_LIST)>> funcs;
    funcs.reserve(object_count);

    ForEachObject([&]<size_t I>(auto&& o, index_tag<I>)
                  {
                      funcs.emplace_back([o](ARG_LIST)
                                         {
                                             o->function(ARG_LIST_FORWARD);
                                         });
                  });

    for (auto _: state)
    {
        for (auto& f: funcs)
        {
            f(INVOKE_PARAMS);
        }
    }
}

BENCHMARK(BM_FunctionV2)BENCHMARK_ARGS;


static void BM_DefaultMulticast_InvokeAction(benchmark::State& state)
{
    TestTemplate<multicast_delegate<void(ARG_LIST)>>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::action>(ptr);
            },
            [](auto&& d)
            {
                d.invoke(INVOKE_PARAMS);
            }
    );
}

//BENCHMARK(BM_DefaultMulticast_InvokeAction)BENCHMARK_ARGS;


static void BM_DefaultMulticast_InvokeFunction(benchmark::State& state)
{
    TestTemplate<multicast_delegate<int(ARG_LIST)>>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::function>(ptr);
            },
            [](auto&& d)
            {
                d.for_each_invoke(INVOKE_PARAMS, [](auto&& res)
                {

                });
            }
    );
}

//BENCHMARK(BM_DefaultMulticast_InvokeFunction)BENCHMARK_ARGS;


static void BM_MulticastFunc_InvokeAction(benchmark::State& state)
{
    TestTemplate<multicast_function<void(ARG_LIST)>>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d += auto_ref_binder(ptr) | bind_memfn<&T::action>;
            },
            [](auto&& d)
            {
                d.invoke(INVOKE_PARAMS);
            }
    );
}

//BENCHMARK(BM_MulticastFunc_InvokeAction)BENCHMARK_ARGS;


static void BM_MulticastFunc_InvokeFunction(benchmark::State& state)
{
    TestTemplate<multicast_function<int(ARG_LIST)>>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d += auto_ref_binder(ptr) | bind_memfn<&T::function>;
            },
            [](auto&& d)
            {
                d.for_each_invoke(INVOKE_PARAMS, [](auto&& res)
                {

                });
            }
    );
}

//BENCHMARK(BM_MulticastFunc_InvokeFunction)BENCHMARK_ARGS;

static void BM_DefaultMulticast_InvokeVirtualAction(benchmark::State& state)
{
    TestTemplate<multicast_delegate<void(ARG_LIST)>>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::virtual_action>(ptr);
            },
            [](auto&& d)
            {
                d.invoke(INVOKE_PARAMS);
            }
    );
}
//BENCHMARK(BM_DefaultMulticast_InvokeVirtualAction)BENCHMARK_ARGS;


static void BM_DefaultMulticast_InvokeVirtualFunction(benchmark::State& state)
{
    TestTemplate<multicast_delegate<int(ARG_LIST)>>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::virtual_function>(ptr);
            },
            [](auto&& d)
            {
                d.for_each_invoke(INVOKE_PARAMS, [](auto&& res)
                {

                });
            }
    );
}
//BENCHMARK(BM_DefaultMulticast_InvokeVirtualFunction)BENCHMARK_ARGS;


struct SharedObjectArray
{
    using typed_object_list = decltype([]
    {
        return []<size_t...I>(std::index_sequence<I...>)
        {
            return std::tuple<SharedPtr<B<I>> ...>{};
        }(std::make_index_sequence<class_count>{});
    }());

    std::vector<typed_object_list> objects;


    SharedObjectArray()
    {
        objects.reserve(per_class_count);
        for (size_t i = 0; i < per_class_count; ++i)
        {
            objects.emplace_back([&]<size_t...I>(std::index_sequence<I...>)
                                 {
                                     auto new_object = [&]<size_t Index>(index_tag<Index>)
                                     {
                                         auto mem = new B<Index>();
                                         return SharedPtr<B<Index>>(mem);
                                     };
                                     return std::tuple{new_object(index_tag<I>{})...};
                                 }(std::make_index_sequence<class_count>{}));
        }
    }


    static auto& Array()
    {
        static SharedObjectArray sa;
        return sa.objects;
    }

};


void ForEachWeakObject(auto&& func)
{
    auto& objects = SharedObjectArray::Array();

    for (auto& object_tuple: objects)
    {
        [&]<size_t...I>(std::index_sequence<I...>)
        {
            auto lambda = [&]<size_t Index>(index_tag<Index>)
            {
                auto ptr = std::get<Index>(object_tuple);
                func(ptr, index_tag<Index>());
            };
            (lambda(index_tag<I>()), ...);
        }(std::make_index_sequence<class_count>{});
    }
}

static void BM_LightWeakMulticast_InvokeAction(benchmark::State& state)
{
    TestTemplate<multicast_light_weak_delegate<void(ARG_LIST)>, SharedObjectArray>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::action>(ptr);
            },
            [](auto&& d)
            {
                d.invoke(INVOKE_PARAMS);
            }
    );
}

//BENCHMARK(BM_LightWeakMulticast_InvokeAction)BENCHMARK_ARGS;


static void BM_LightWeakMulticast_InvokeFunction(benchmark::State& state)
{
    TestTemplate<multicast_light_weak_delegate<int(ARG_LIST)>, SharedObjectArray>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::function>(ptr);
            },
            [](auto&& d)
            {
                d.for_each_invoke(INVOKE_PARAMS, [](auto&& res)
                {
                });
            }
    );
}

//BENCHMARK(BM_LightWeakMulticast_InvokeFunction)BENCHMARK_ARGS;


struct StdSharedObjectArray
{
    using typed_object_list = decltype([]
    {
        return []<size_t...I>(std::index_sequence<I...>)
        {
            return std::tuple<std::shared_ptr<B<I>> ...>{};
        }(std::make_index_sequence<class_count>{});
    }());

    std::vector<typed_object_list> objects;


    StdSharedObjectArray()
    {
        objects.reserve(per_class_count);
        for (size_t i = 0; i < per_class_count; ++i)
        {
            objects.emplace_back([&]<size_t...I>(std::index_sequence<I...>)
                                 {
                                     auto new_object = [&]<size_t Index>(index_tag<Index>)
                                     {
                                         auto mem = new B<Index>();
                                         return std::shared_ptr<B<Index>>(mem);
                                     };
                                     return std::tuple{new_object(index_tag<I>{})...};
                                 }(std::make_index_sequence<class_count>{}));
        }
    }


    static auto& Array()
    {
        static StdSharedObjectArray ssa;
        return ssa.objects;
    }

};

static void BM_WeakMulticast_InvokeAction(benchmark::State& state)
{
    TestTemplate<multicast_weak_delegate<void(ARG_LIST)>, StdSharedObjectArray>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::action>(ptr);
            },
            [](auto&& d)
            {
                d.invoke(INVOKE_PARAMS);
            }
    );
}

//BENCHMARK(BM_WeakMulticast_InvokeAction)BENCHMARK_ARGS;


static void BM_WeakMulticast_InvokeFunction(benchmark::State& state)
{
    TestTemplate<multicast_weak_delegate<int(ARG_LIST)>, StdSharedObjectArray>(
            state,
            [](auto&& d, auto&& ptr)
            {
                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
                return d.template bind<&T::function>(ptr);
            },
            [](auto&& d)
            {
                d.for_each_invoke(INVOKE_PARAMS, [](auto&& res)
                {
                });
            }
    );
}

//BENCHMARK(BM_WeakMulticast_InvokeFunction)BENCHMARK_ARGS;

//
//static void BM_WeakMulticast_InvokeVirtualAction(benchmark::State& state)
//{
//    BM_WeakMulticast<multicast_weak_delegate<void(ARG_LIST)>>(
//            state,
//            [](auto&& d, auto&& ptr)
//            {
//                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
//                return d.template bind<&T::virtual_action>(ptr);
//            },
//            [](auto&& d)
//            {
//                d.invoke(INVOKE_PARAMS);
//            }
//    );
//}
//
//static void BM_WeakMulticast_VirtualFunction(benchmark::State& state)
//{
//    BM_WeakMulticast<multicast_weak_delegate<int(ARG_LIST)>>(
//            state,
//            [](auto&& d, auto&& ptr)
//            {
//                using T = std::pointer_traits<std::decay_t<decltype(ptr)>>::element_type;
//                return d.template bind<&T::virtual_function>(ptr);
//            },
//            [](auto&& d)
//            {
//                d.invoke(INVOKE_PARAMS, [](auto&& iterable)
//                {
//                    for (auto res: iterable)
//                    {
//                    }
//                });
//            }
//    );
//}

//BENCHMARK(BM_WeakMulticast_InvokeAction)
//        ->MinWarmUpTime(min_warm_up)
//        ->Iterations(invoke_count);
//BENCHMARK(BM_WeakMulticast_InvokeFunction)
//        ->MinWarmUpTime(min_warm_up)
//        ->Iterations(invoke_count);
//BENCHMARK(BM_WeakMulticast_InvokeVirtualAction)
//        ->MinWarmUpTime(min_warm_up)
//        ->Iterations(invoke_count);
//BENCHMARK(BM_WeakMulticast_VirtualFunction)
//        ->MinWarmUpTime(min_warm_up)
//        ->Iterations(invoke_count);


#undef ARG_LIST
#undef ARG_LIST_FORWARD