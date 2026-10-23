-- Logo and icon generator. Run it from the repo root with `xmake icons`.
set_project("basalt-icons")
set_languages("c++20")
set_encodings("utf-8")
add_rules("mode.debug", "mode.release")

add_requires("lunasvg")

target("basalt-icons")
    set_kind("binary")
    add_files("main.cpp")
    add_packages("lunasvg")
