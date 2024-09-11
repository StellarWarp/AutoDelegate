#pragma once

#include "../delegate/multicast_delegate.h"
#include "../delegate/multicast_function.h"
#include "../auto_reference/auto_reference.h"

#ifdef no_unique_address
#undef no_unique_address
#endif
#ifdef _MSC_VER
#define DELEGATE_no_unique_address msvc::no_unique_address
#else
#define DELEGATE_no_unique_address no_unique_address
#endif
namespace auto_delegate
{
    using namespace auto_reference;

    template<typename Func>
    using auto_delegate = delegate<Func, weak_reference<generic_ref_reflector, generic_ref_reflector>>;
    template<typename Func>
    using weak_delegate = delegate<Func, std::weak_ptr<void>>;
    template<typename Func>
    using shared_delegate = delegate<Func, std::shared_ptr<void>>;


#pragma region auto_delegate


    template<typename AutoRefProtocol = generic_ref_protocol,
            typename DelegateHandle = void,
            typename InverseHandle = delegate_handle_traits<DelegateHandle>::inverse_handle_type
    >
    class auto_delegate_container : public array_ref_charger<AutoRefProtocol, std::tuple<void*, InverseHandle>>
    {
        using tuple_t = std::tuple<void*, InverseHandle>;
        using super = array_ref_charger<AutoRefProtocol, std::tuple<void*, InverseHandle>>;
    public:
        using delegate_handle_t = delegate_handle_traits<DelegateHandle>::delegate_handle_type;
        using delegate_handle_t_ref = delegate_handle_traits<DelegateHandle>::delegate_handle_reference;
        using inverse_handle_t = delegate_handle_traits<DelegateHandle>::inverse_handle_type;
        using inverse_handle_t_ref = delegate_handle_traits<DelegateHandle>::inverse_handle_reference;
        static constexpr bool enable_delegate_handle = delegate_handle_traits<DelegateHandle>::enable_delegate_handle;

    private:


        delegate_handle_t bind_handle(inverse_handle_t_ref handle_ref)
        {
            if constexpr (requires { delegate_handle_t(this, &handle_ref); })
                return delegate_handle_t(this, &handle_ref);
            else if constexpr (requires { delegate_handle_t(& handle_ref); })
                return delegate_handle_t(&handle_ref);
            else
                return;
        }

        template<size_t Idx, class Tuple>
        constexpr size_t tuple_element_offset()
        {
            using ElementType = typename std::tuple_element<Idx, Tuple>::type;
            alignas(Tuple) char storage[sizeof(Tuple)];//for eliminate warning
            Tuple* t_ptr = reinterpret_cast<Tuple*>(&storage);
            ElementType* elem_ptr = &std::get<Idx>(*t_ptr);
            return reinterpret_cast<char*>(elem_ptr) - reinterpret_cast<char*>(t_ptr);
        }

    public:
        delegate_handle_t bind(const super::pointer_t& obj, void* invoker)
        {
            assert(obj);
            auto& [fn, handle_ref] = super::bind(obj);
            fn = invoker;
            return bind_handle(handle_ref);
        }

        delegate_handle_t bind(std::nullptr_t, void* invoker) requires enable_delegate_handle
        {
            auto& [fn, handle_ref] = super::bind(std::nullptr_t{});
            fn = invoker;
            return bind_handle(handle_ref);
        }

        void unbind(delegate_handle_t_ref handle) requires enable_delegate_handle
        {
            if constexpr (!enable_delegate_handle) return;
            inverse_handle_t* inv_handle = inverse_handle_t::get(&handle);
            assert(inv_handle);
            intptr_t offset = offsetof(typename super::element_t, data) + tuple_element_offset<1, tuple_t>();
            auto* h = (typename super::ref_handle_t*) (intptr_t(inv_handle) - offset);
            super::notify_reference_removed(h);
        }

        void unbind(const super::pointer_t& obj) requires (not enable_delegate_handle)
        {
            assert(obj);
            super::unbind(obj);
        }

        auto_delegate_container() = default;

        auto_delegate_container(auto_delegate_container&& other) noexcept
                : super(std::move(other))
        {
            if constexpr (enable_delegate_handle)
                if constexpr (delegate_handle_t::container_reference)
                {
                    for (auto& ref: *this)
                    {
                        ref.data.inv_handle.notify_container_moved(this);
                    }
                }
        }
    };

    template<typename Func>
    using multicast_auto_delegate = multicast_delegate<Func, auto_delegate_container<generic_ref_protocol, delegate_handle>>;

    template<typename Func>
    using multicast_auto_delegate_extern_ref = multicast_delegate<Func, auto_delegate_container<reference_reflector_ref_protocol>>;

#pragma endregion

#pragma region weak_delegate

    template<typename DelegateHandle = delegate_handle>
    class weak_delegate_container
    {
    public:
        using delegate_handle_t = delegate_handle_traits<DelegateHandle>::delegate_handle_type;
        using delegate_handle_t_ref = delegate_handle_traits<DelegateHandle>::delegate_handle_reference;
        using inverse_handle_t = delegate_handle_traits<DelegateHandle>::inverse_handle_type;
        using inverse_handle_t_ref = delegate_handle_traits<DelegateHandle>::inverse_handle_reference;
        static constexpr bool enable_delegate_handle = delegate_handle_traits<DelegateHandle>::enable_delegate_handle;


    private:
        struct delegate_object
        {
            std::weak_ptr<void> ptr;
            void* invoker;
            [[DELEGATE_no_unique_address]] inverse_handle_t inv_handle;
        };

        std::vector<delegate_object> objects;

        template<typename T, typename U>
        inline bool equals(const std::weak_ptr<T>& t, const std::weak_ptr<U>& u)
        {
            return !t.owner_before(u) && !u.owner_before(t);
        }

    public:

        auto size() { return objects.size(); }

        bool empty() { return objects.empty(); }

        void clear() { objects.clear(); }

        delegate_handle_t bind(const std::weak_ptr<void>& obj, void* invoker)
        {
            assert(!obj.expired());
            auto& [ptr, fn, handle_ref] = objects.emplace_back(obj, invoker, inverse_handle_t{});
            if constexpr (requires { delegate_handle_t(this, &handle_ref); })
                return delegate_handle_t(this, &handle_ref);
            else if constexpr (requires { delegate_handle_t(& handle_ref); })
                return delegate_handle_t(&handle_ref);
            else
                return;
        }

        void unbind(delegate_handle_t_ref handle) requires enable_delegate_handle
        {
            if constexpr (!enable_delegate_handle) return;
            inverse_handle_t* inv_handle = inverse_handle_t::get(&handle);
            intptr_t handle_ref_element_offset = offsetof(delegate_object, inv_handle);
            auto* o = (delegate_object*) (intptr_t(inv_handle) - handle_ref_element_offset);
            int64_t index = o - objects.data();
            objects[index] = std::move(objects.back());
            objects.pop_back();
        }

        void unbind(const std::weak_ptr<void>& obj) requires (not enable_delegate_handle)
        {
            assert(!obj.expired());
            for (int i = 0; i < objects.size(); ++i)
            {
                if (equals(objects[i].handle.get(), obj))
                {
                    objects[i] = std::move(objects.back());
                    objects.pop_back();
                    return;
                }
            }
            assert(false);
        }


        weak_delegate_container() = default;

        weak_delegate_container(weak_delegate_container&& other) noexcept
                : objects(std::move(other.objects))
        {
            if constexpr (enable_delegate_handle)
                if constexpr (delegate_handle_t::reqiure_container)
                {
                    for (auto& ref: objects)
                    {
                        ref.inv_handle.notify_container_moved(this);
                    }
                }
        }

        class iterator
        {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = delegate_object;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;
        private:
            std::vector<delegate_object>::iterator it;
            std::vector<delegate_object>::iterator end_it;
            std::vector<delegate_object>& vec;

        public:
            iterator(std::vector<delegate_object>& vec)
                    : it(vec.begin()), end_it(vec.end()), vec(vec) {}

            void operator++() { ++it; }

            void operator++(int) { operator++(); }

            reference operator*() { return *it; }

            bool operator==(const iterator& other) { return it == other.it; }

            bool operator==(std::nullptr_t)
            {
                if (it == end_it)
                {
                    vec.resize(end_it - vec.begin());
                    return true;
                }
                while (it->ptr.expired())
                {
                    end_it--;
                    if (it == end_it)
                    {
                        vec.resize(end_it - vec.begin());
                        return true;
                    }
                    *it = std::move(*end_it);
                }
                return false;
            }
        };

        iterator begin() { return iterator(objects); }

        std::nullptr_t end() { return {}; }
    };

    template<typename Func>
    using multicast_weak_delegate = multicast_delegate<Func, weak_delegate_container<>>;

#pragma endregion

#pragma region multicast_function_extendtion

//    template<typename Ret, typename... Args>
//    struct auto_ref_binder_base
//    {
//        typed_ref_handle<void> ref;
//        void* container;
//
//        Ret (* on_expired)(auto_ref_binder_base* self, Args... args);
//
//        using pointer_t = typed_ref_handle<void>::other_object_ptr_t;
//
//        auto_ref_binder_base(const pointer_t& obj) : ref()
//        {
//            auto charger = ref_charger_convert_trait<generic_ref_protocol, false>::cast_ref_charger(obj);
//            auto handle = charger->template new_bind_handle<generic_ref_protocol>();
//            ref = typed_ref_handle<void>{no_ref_charger::ptr(), charger, handle};
//        }
//
//        auto_ref_binder_base(const auto_ref_binder_base&) { assert(false); }
//
//        auto_ref_binder_base(auto_ref_binder_base&&) = default;
//
//        template<typename Container>
//        void on_bind(Container* container_ptr)
//        {
//            container = container_ptr;
//            on_expired = [](auto_ref_binder_base* self, Args... args)
//            {
//                return static_cast<Container*>(self->container)->remove_on_call(self, std::forward<Args>(args)...);
//            };
//        }
//
//        Ret _invoke(Args... args, auto&& invoker)
//        {
//            if (!ref) return on_expired(this, args...);
//            return invoker(args...);
//        }
//    };
//
//    template<typename T, auto MemFunc, typename FuncT>
//    struct auto_ref_binder_impl;
//
//    template<typename T,
//            auto MemFunc,
//            typename Ret,
//            typename... Args>
//    struct auto_ref_binder_impl<T, MemFunc, Ret(Args...)> : auto_ref_binder_base<Ret, Args...>
//    {
//        using super = auto_ref_binder_base<Ret, Args...>;
//
//        explicit auto_ref_binder_impl(super::pointer_t obj) : super(obj) {}
//
//        Ret operator()(Args... args) noexcept
//        {
//            return super::_invoke(std::forward<Args>(args)...,
//                                  [&](Args... args)
//                                  {
//                                      return (static_cast<T*>(this->ref.get())->*MemFunc)(std::forward<Args>(args)...);
//                                  });
//        }
//    };
//
//    template<auto MemFunc, typename T_Ptr>
//    auto auto_ref_binder(const T_Ptr& obj)
//    {
//        using binder = auto_ref_binder_impl<
//                typename std::pointer_traits<std::decay_t<T_Ptr>>::element_type,
//                MemFunc,
//                typename function_traits<decltype(MemFunc)>::function_type>;
//        return binder(obj);
//    }
//
//
//    template<typename T, typename Lambda, typename FuncT>
//    struct auto_ref_binder_lambda;
//
//    template<typename T, typename Lambda, typename Ret, typename... Args, typename _>
//    struct auto_ref_binder_lambda<T, Lambda, Ret(_&&, Args...)>
//            : auto_ref_binder_base<Ret, Args...>
//    {
//        using super = auto_ref_binder_base<Ret, Args...>;
//        using super::super;
//        Lambda lambda;
//
//        template<typename OtherLambda>
//        explicit auto_ref_binder_lambda(super::pointer_t obj, OtherLambda&& other)
//                : super(obj), lambda(std::forward<OtherLambda>(other)) {}
//
//        Ret operator()(Args... args) noexcept
//        {
//            return super::_invoke(std::forward<Args>(args)...,
//                                  [&](Args... args)
//                                  {
//                                      return lambda(*static_cast<T*>(this->ref.get()), std::forward<Args>(args)...);
//                                  });
//        }
//    };
//
//    template<typename T_Ptr, typename Lambda>
//    auto_ref_binder_lambda(const T_Ptr& obj, Lambda&& lambda) ->
//    auto_ref_binder_lambda<
//            typename std::pointer_traits<T_Ptr>::element_type,
//            std::decay_t<Lambda>,
//            typename function_traits<decltype(
//            &std::decay_t<Lambda>::template operator()<typename std::pointer_traits<T_Ptr>::element_type>
//            )>::function_type
//    >;

    template<typename FuncT>
    struct auto_ref_binder_impl;

    template<typename Ret, typename... Args>
    struct auto_ref_binder_impl<Ret(Args...)>
    {
        typed_ref_handle<void> ref;
        void* container;

        Ret (* on_expired)(auto_ref_binder_impl* self, Args... args);

        using pointer_t = typed_ref_handle<void>::other_object_ptr_t;

        auto_ref_binder_impl(const pointer_t& obj) : ref()
        {
            auto charger = ref_charger_convert_trait<generic_ref_protocol, false>::cast_ref_charger(obj);
            auto handle = charger->template new_bind_handle<generic_ref_protocol>();
            ref = typed_ref_handle<void>{no_ref_charger::ptr(), charger, handle};
        }

        auto_ref_binder_impl(const auto_ref_binder_impl&) { assert(false); }

        auto_ref_binder_impl(auto_ref_binder_impl&&) = default;

        template<typename Container>
        void on_bind(Container* container_ptr)
        {
            container = container_ptr;
            on_expired = [](auto_ref_binder_impl* self, Args... args)
            {
                return static_cast<Container*>(self->container)->remove_on_call(self, std::forward<Args>(args)...);
            };
        }

        Ret invoke(auto&& invoker, Args... args)
        {
            if (!ref) return on_expired(this, std::forward<Args>(args)...);
            return invoker(ref.get() ,std::forward<Args>(args)...);
        }
    };

    template<typename T_Ptr>
    struct auto_ref_binder : object_binder_tag
    {
        using type = std::pointer_traits<std::decay_t<T_Ptr>>::element_type;
        T_Ptr ptr;
        auto_ref_binder(T_Ptr ptr) : ptr(ptr) {}

        template<typename FuncT>
        decltype(auto) convert()
        {
            return auto_ref_binder_impl<FuncT>{std::move(ptr)};
        }
    };



#pragma endregion

#pragma region benchmark_lightweight_shared_ptr

    class RefCount {
    public:
        using deleter_t = void (*)(void*);
        RefCount(int ref_count, int weak_count, deleter_t deleter) :
        ref_count_(ref_count),
        weak_count_(weak_count),
        deleter_(deleter){}

        int ref_count() const { return ref_count_; }

        int weak_count() const { return weak_count_; }

        void increment_ref_count() { ++ref_count_; }

        void decrement_ref_count() { --ref_count_; }

        void increment_weak_count() { ++weak_count_; }

        void decrement_weak_count() { --weak_count_; }

        void delete_object(void* ptr) { deleter_(ptr); }

    private:
        int ref_count_;
        int weak_count_;
        deleter_t deleter_;
    };

    template <typename T>
    class SharedPtr {
        template <typename>
        friend class WeakPtr;

        SharedPtr(T* ptr, RefCount* ref_count) : ptr_(ptr), ref_count_(ref_count) {
            ref_count_->increment_ref_count();
        }

    public:
        SharedPtr() : ptr_(nullptr), ref_count_(nullptr) {}
        SharedPtr(T* ptr)
        : ptr_(ptr),
          ref_count_(new RefCount(1, 0, [](void* ptr) { delete static_cast<T*>(ptr); }))
        {
            assert(ptr != nullptr);
        }

        SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ref_count_(other.ref_count_) {
            ref_count_->increment_ref_count();
        }

        SharedPtr(SharedPtr&& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }

        ~SharedPtr() {
            if(ref_count_)
            {
                ref_count_->decrement_ref_count();
                if(ref_count_->ref_count() == 0)
                {
                    ref_count_->delete_object(ptr_);
                }
            }
        }

        add_reference_possible_void<T> operator*() const { if constexpr (requires { *ptr_; }) return *ptr_; else return {}; }

        T* operator->() const { return ptr_; }

        T* get() const { return ptr_; }

        bool unique() const { return ref_count_->ref_count() == 1; }

    private:
        T* ptr_;
        RefCount* ref_count_;
    };

    template <typename T>
    class WeakPtr {
    public:
        WeakPtr() : ptr_(nullptr), ref_count_(nullptr) {}

        template<typename U> requires std::convertible_to<U*, T*>
        WeakPtr(const SharedPtr<U>& shared_ptr) : ptr_(shared_ptr.get()), ref_count_(shared_ptr.ref_count_) {
            ref_count_->increment_weak_count();
        }

        WeakPtr(const WeakPtr& other) : ptr_(other.ptr_), ref_count_(other.ref_count_) {
            if (ref_count_) ref_count_->increment_weak_count();
        }

        WeakPtr(WeakPtr&& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }
        WeakPtr& operator=(const WeakPtr& other) {
            if (this != &other) {
                if (ref_count_) ref_count_->decrement_weak_count();
                ptr_ = other.ptr_;
                ref_count_ = other.ref_count_;
                if (ref_count_) ref_count_->increment_weak_count();
            }
            return *this;
        }

        WeakPtr& operator=(WeakPtr&& other) noexcept {
            if (this != &other) {
                if (ref_count_) ref_count_->decrement_weak_count();
                ptr_ = other.ptr_;
                ref_count_ = other.ref_count_;
                other.ptr_ = nullptr;
                other.ref_count_ = nullptr;
            }
            return *this;
        }


        ~WeakPtr() {
            if (ref_count_)
            {
                ref_count_->decrement_weak_count();
                if (ref_count_->ref_count() == 0 && ref_count_->weak_count() == 0 ) {
                    delete ref_count_;
                }
            }
        }

        bool expired() const { return !ref_count_ || ref_count_->ref_count() == 0; }

        SharedPtr<T> lock() const {
            if(!ref_count_) return SharedPtr<T>();
            return SharedPtr<T>(ptr_, ref_count_);
        }

    private:
        T* ptr_;
        RefCount* ref_count_;
    };

    template<typename DelegateHandle = delegate_handle>
    class light_weak_delegate_container
    {
    public:
        using delegate_handle_t = delegate_handle_traits<DelegateHandle>::delegate_handle_type;
        using delegate_handle_t_ref = delegate_handle_traits<DelegateHandle>::delegate_handle_reference;
        using inverse_handle_t = delegate_handle_traits<DelegateHandle>::inverse_handle_type;
        using inverse_handle_t_ref = delegate_handle_traits<DelegateHandle>::inverse_handle_reference;
        static constexpr bool enable_delegate_handle = delegate_handle_traits<DelegateHandle>::enable_delegate_handle;


    private:
        struct delegate_object
        {
            WeakPtr<void> ptr;
            void* invoker;
            [[DELEGATE_no_unique_address]] inverse_handle_t inv_handle;
        };

        std::vector<delegate_object> objects;

        template<typename T, typename U>
        inline bool equals(const WeakPtr<T>& t, const WeakPtr<U>& u)
        {
            return !t.owner_before(u) && !u.owner_before(t);
        }

    public:

        auto size() { return objects.size(); }

        bool empty() { return objects.empty(); }

        void clear() { objects.clear(); }

        delegate_handle_t bind(const WeakPtr<void>& obj, void* invoker)
        {
            assert(!obj.expired());
            auto& [ptr, fn, handle_ref] = objects.emplace_back(obj, invoker, inverse_handle_t{});
            if constexpr (requires { delegate_handle_t(this, &handle_ref); })
                return delegate_handle_t(this, &handle_ref);
            else if constexpr (requires { delegate_handle_t(& handle_ref); })
                return delegate_handle_t(&handle_ref);
            else
                return;
        }

        void unbind(delegate_handle_t_ref handle) requires enable_delegate_handle
        {
            if constexpr (!enable_delegate_handle) return;
            inverse_handle_t* inv_handle = inverse_handle_t::get(&handle);
            intptr_t handle_ref_element_offset = offsetof(delegate_object, inv_handle);
            auto* o = (delegate_object*) (intptr_t(inv_handle) - handle_ref_element_offset);
            int64_t index = o - objects.data();
            objects[index] = std::move(objects.back());
            objects.pop_back();
        }

        void unbind(const WeakPtr<void>& obj) requires (not enable_delegate_handle)
        {
            assert(!obj.expired());
            for (int i = 0; i < objects.size(); ++i)
            {
                if (equals(objects[i].handle.get(), obj))
                {
                    objects[i] = std::move(objects.back());
                    objects.pop_back();
                    return;
                }
            }
            assert(false);
        }


        light_weak_delegate_container() = default;

        light_weak_delegate_container(light_weak_delegate_container&& other) noexcept
                : objects(std::move(other.objects))
        {
            if constexpr (enable_delegate_handle)
                if constexpr (delegate_handle_t::reqiure_container)
                {
                    for (auto& ref: objects)
                    {
                        ref.inv_handle.notify_container_moved(this);
                    }
                }
        }

        class iterator
        {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = delegate_object;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;
        private:
            std::vector<delegate_object>::iterator it;
            std::vector<delegate_object>::iterator end_it;
            std::vector<delegate_object>& vec;

        public:
            iterator(std::vector<delegate_object>& vec)
                    : it(vec.begin()), end_it(vec.end()), vec(vec) {}

            void operator++() { ++it; }

            void operator++(int) { operator++(); }

            reference operator*() { return *it; }

            bool operator==(const iterator& other) { return it == other.it; }

            bool operator==(std::nullptr_t)
            {
                if (it == end_it)
                {
                    vec.resize(end_it - vec.begin());
                    return true;
                }
                while (it->ptr.expired())
                {
                    end_it--;
                    if (it == end_it)
                    {
                        vec.resize(end_it - vec.begin());
                        return true;
                    }
                    *it = std::move(*end_it);
                }
                return false;
            }
        };

        iterator begin() { return iterator(objects); }

        std::nullptr_t end() { return {}; }
    };

    template<typename Func>
    using multicast_light_weak_delegate = multicast_delegate<Func, light_weak_delegate_container<>>;

#pragma endregion
}


#undef DELEGATE_no_unique_address