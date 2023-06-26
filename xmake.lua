
set_languages("cxx20")
add_includedirs("$(env BOOST_LATEST)", "$(env SIMPLE_CPP)")
add_linkdirs("$(env BOOST_LATEST)\\lib64-msvc-14.2")
add_requires("spdlog", "fmt")
add_packages("spdlog", "fmt")
add_defines("FMT_HEADER_ONLY", "SPDLOG_FMT_EXTERNAL", "SPDLOG_FMT_EXTERNAL_HO")

target("rtrrecv")
    set_kind("binary")
    add_files("rtrrecv.cpp")


target("simsti")
    set_kind("binary")
    add_files("simsti.cpp")
