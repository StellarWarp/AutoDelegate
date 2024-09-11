set_encodings("utf-8")
add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    set_runtimes("MDd")
end


add_cxflags("/Zc:__cplusplus", {tools = {"msvc", "cl", "clang_cl"}})
-- add_cxflags("/JMC", {tools = {"msvc", "cl", "clang_cl"}, force = true})

add_requires("benchmark", {configs = {header_only  = true}})
add_requires("gtest", {configs = {header_only  = true, debug = true}})

add_ldflags("/PROFILE")

target("DelegateBenchmark")
    set_languages("c++23")
    set_kind("binary")

    add_files("src/benchmark/*.cpp")
    add_packages("benchmark")
    set_symbols("debug")

target("DelegateTest")
    set_languages("c++23")
    set_kind("binary")

    add_files("src/test/*.cpp")
    add_packages("gtest")
    set_symbols("debug")


target("DelegateSample")
    set_languages("c++23")
    set_kind("binary")

    add_files("src/samples/*.cpp")
    set_symbols("debug")