
add_rules("mode.debug", "mode.release")

target("main")
    set_kind("binary")
    add_files("src/*.cpp")
    add_cxflags("-std=c++20")
    add_links("raylib")
    set_rundir(".")

