
#include <gtest/gtest.h>
#include <numeric>
#include "../delegate/function.h"

#if defined _WIN32 || defined_WIN64

#include <crtdbg.h>

class MemoryLeakDetector {
public:
    MemoryLeakDetector() {
        _CrtMemCheckpoint(&memState_);
    }

    ~MemoryLeakDetector() {
        _CrtMemState stateNow, stateDiff;
        _CrtMemCheckpoint(&stateNow);
        int diffResult = _CrtMemDifference(&stateDiff, &memState_, &stateNow);
        if (diffResult)
            reportFailure(stateDiff.lSizes[1]);
    }
private:
    void reportFailure(unsigned int unfreedBytes) {
        FAIL() << "Memory leak of " << unfreedBytes << " byte(s) detected.";
    }
    _CrtMemState memState_;
};
#endif


TEST(function, test_function)
{
    #if defined _WIN32 || defined_WIN64
    MemoryLeakDetector memoryLeakDetector;
    #endif
    using namespace auto_delegate;


    function<int(int,int)> empty_f;

    ASSERT_FALSE(empty_f);

    function f0 = []{ return 114514; };
    ASSERT_EQ(f0(), 114514);

    function<int(int,int)> f = [](int a, int b) { return a + b; };
    ASSERT_EQ(f(1, 2), 3);

    f = [](int a, int b) { return a - b; };
    ASSERT_EQ(f(3, 2), 1);

    struct small_buffer
    {
        int a;
        int b;
    }capture(3, 4);

    f = [=] (int a, int b) { return a + b + capture.a + capture.b; };
    ASSERT_EQ(f(1, 2), 10);

    //copy
    auto f2 = f;
    ASSERT_EQ(f2(1, 2), 10);

    //move
    auto f3 = std::move(f);
    ASSERT_EQ(f3(1, 2), 10);

    ASSERT_FALSE(f);


    struct buffer_1_t
    {
        uint64_t arr[6] = {1, 2, 3, 4, 5, 6};
    }b1;
    double exam_sum_1 = std::accumulate(std::begin(b1.arr), std::end(b1.arr), 0.0);
    function<uint64_t(uint64_t)> f4 = [=](uint64_t a)
    {
        return std::accumulate(std::begin(b1.arr), std::end(b1.arr), a);
    };
    ASSERT_EQ(f4(123), exam_sum_1 + 123);

    struct buffer_2_t
    {
        uint64_t arr[7] = {1, 2, 3, 4, 5, 6, 7};
    }b2;
    double exam_sum_2 = std::accumulate(std::begin(b2.arr), std::end(b2.arr), 0.0);
    f4 = [=](uint64_t a)
    {
        return std::accumulate(std::begin(b2.arr), std::end(b2.arr), a);
    };
    ASSERT_EQ(f4(123), exam_sum_2 + 123);



    auto& target_type = f4.target_type();


    function<uint64_t(uint64_t)> f5 = [=] (uint64_t a) mutable
    {
        return std::accumulate(std::begin(b1.arr), std::end(b1.arr), a);
    };
    ASSERT_EQ(f5(123), exam_sum_1 + 123);

    f5.swap(f4);

    ASSERT_EQ(f4(123), exam_sum_1 + 123);
    ASSERT_EQ(f5(123), exam_sum_2 + 123);

    struct buffer_12_t
    {
        uint64_t arr[6] = {1, 2, 3, 4, 5, 6};
    }b12;
    f4 = [=](uint64_t a) mutable
    {
        return std::accumulate(std::begin(b2.arr), std::end(b2.arr), a);
    };
    ASSERT_EQ(f4(123), exam_sum_2 + 123);

}

template<int IDENT>
int tmp_func(int a, int b)
{
    return a + b + IDENT;
}

template<typename T>
constexpr void* move_constructor(void* dest, void* src)
{
    return (T*) new(dest) T(std::move(*(T*) src));
}

template<typename T>
constexpr void* copy_constructor(void* dest, const void* src)
{
    return (T*) new(dest) T(*(T*) src);
}

template<typename T>
constexpr void destructor(void* addr)
{
    ((T*) addr)->~T();
}

TEST(function, test_tag_function_ptr)
{
    using namespace auto_delegate;

    {
        function<int(int, int)> f1 = [](int a, int b) { return a + b; };
        ASSERT_EQ(f1(1, 2), 3);
        function<int(int, int)> f2 = [](int a, int b) { return a + b; };
        ASSERT_EQ(f2(1, 2), 3);
        function<int(int, int)> f3 = [](int a, int b) { return a + b; };
        ASSERT_EQ(f3(1, 2), 3);
        function<int(int, int)> f4 = [](int a, int b) { return a + b; };
        ASSERT_EQ(f4(1, 2), 3);
        function<int(int, int)> f5 = [](int a, int b) { return a + b; };
        ASSERT_EQ(f5(1, 2), 3);
        function<int(int, int)> f6 = tmp_func<114514>;
        ASSERT_EQ(f6(1, 2), 3 + 114514);
        function<int(int, int)> f7 = tmp_func<1234>;
        ASSERT_EQ(f7(1, 2), 3 + 1234);
    }

    function<void*(void*, void*)> f1 = move_constructor<int>;
    function<void*(void*, const void*)> f2 = copy_constructor<float>;
    function<void(void*)> f3 = destructor<int>;


}

TEST(function, test_function_validate)
{
#if defined _WIN32 || defined_WIN64
    MemoryLeakDetector memoryLeakDetector;
#endif
    using namespace auto_delegate::function_v2;


    function<int(int,int)> empty_f;

    ASSERT_FALSE(empty_f);

    function f0 = []{ return 114514; };
    ASSERT_EQ(f0(), 114514);

    function<int(int,int)> f = [](int a, int b) { return a + b; };
    ASSERT_EQ(f(1, 2), 3);

    f = [](int a, int b) { return a - b; };
    ASSERT_EQ(f(3, 2), 1);

    struct small_buffer
    {
        int a;
        int b;
    }capture(3, 4);

    f = [=] (int a, int b) { return a + b + capture.a + capture.b; };
    ASSERT_EQ(f(1, 2), 10);

    //copy
    auto f2 = f;
    ASSERT_EQ(f2(1, 2), 10);

    //move
    auto f3 = std::move(f);
    ASSERT_EQ(f3(1, 2), 10);

    ASSERT_FALSE(f);


    struct buffer_1_t
    {
        uint64_t arr[7] = {1, 2, 3, 4, 5, 6, 7};
    }b1;
    double exam_sum_1 = std::accumulate(std::begin(b1.arr), std::end(b1.arr), 0.0);
    function<uint64_t(uint64_t)> f4 = [=](uint64_t a)
    {
        return std::accumulate(std::begin(b1.arr), std::end(b1.arr), a);
    };
    ASSERT_EQ(f4(123), exam_sum_1 + 123);

    struct buffer_2_t
    {
        uint64_t arr[6] = {1, 2, 3, 4, 5, 6};
    }b2;
    double exam_sum_2 = std::accumulate(std::begin(b2.arr), std::end(b2.arr), 0.0);
    f4 = [=](uint64_t a)
    {
        return std::accumulate(std::begin(b2.arr), std::end(b2.arr), a);
    };
    ASSERT_EQ(f4(123), exam_sum_2 + 123);

    auto& target_type = f4.target_type();


    function<uint64_t(uint64_t)> f5 = [=] (uint64_t a) mutable
    {
        return std::accumulate(std::begin(b1.arr), std::end(b1.arr), a);
    };
    ASSERT_EQ(f5(123), exam_sum_1 + 123);

    struct buffer_12_t
    {
        uint64_t arr[6] = {1, 2, 3, 4, 5, 6};
    }b12;
    f4 = [=](uint64_t a) mutable
    {
        return std::accumulate(std::begin(b2.arr), std::end(b2.arr), a);
    };
    ASSERT_EQ(f4(123), exam_sum_2 + 123);

    static bool validate_res = true;

    struct callable1
    {
        bool validate(function_validate_tag) const
        {
            return validate_res;
        }

        int operator()(int a, int b)
        {
            return a + b;
        }
    };

    callable1 c1;

    function<int(int, int)> f6 = c1;
    ASSERT_EQ(f6.validate(), true);
    ASSERT_EQ(f6(1, 2), 3);


    validate_res = false;
    ASSERT_EQ(f6.validate(), false);
    ASSERT_EQ(f6.try_invoke(1,2).has_value(), false);

}
