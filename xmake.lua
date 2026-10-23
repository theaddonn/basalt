set_project("basalt")
set_version("0.1.0")
set_xmakever("2.9.0")

set_languages("c++20")
-- source and execution charset are UTF-8 everywhere (/utf-8 on MSVC)
set_encodings("utf-8")
set_warnings("allextra")
add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

add_requires("cli11", "nlohmann_json", "spdlog", "tl_expected")
add_requires("catch2 3.x", {optional = true})

-- core logic, kept separate from main so tests can link against it
target("basalt-core")
    set_kind("static")
    add_files("src/basalt/**.cpp")
    add_includedirs("src", {public = true})
    add_headerfiles("src/basalt/**.hpp")
    add_packages("nlohmann_json", "spdlog", "tl_expected", {public = true})

target("basalt")
    set_kind("binary")
    add_files("src/main.cpp")
    if is_plat("windows", "mingw") then
        -- makes narrow Windows APIs (and std::filesystem) use UTF-8
        add_files("res/basalt.manifest")
        -- app icon and version info shown in Explorer
        add_files("res/basalt.rc")
    end
    if is_plat("linux") then
        -- `xmake install` puts the icon and desktop entry where launchers find them
        add_installfiles("res/basalt.desktop", {prefixdir = "share/applications"})
        add_installfiles("res/basalt.png", {prefixdir = "share/icons/hicolor/256x256/apps"})
    end
    add_deps("basalt-core")
    add_packages("cli11")

target("basalt-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/**.cpp")
    add_deps("basalt-core")
    add_packages("catch2")
    add_tests("default")
