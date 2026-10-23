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

-- `xmake f -m dist`: shipping build tuned for peak performance and small,
-- self-contained binaries. Add `--native=y` to also tune for this machine's CPU
-- (the result may not run on other CPUs).
option("native")
    set_default(false)
    set_showmenu(true)
    set_description("Optimize dist builds for the host CPU (-march=native)")
option_end()

if is_mode("dist") then
    set_optimize("fastest")  -- -O3 / /Ox
    set_symbols("hidden")
    set_strip("all")
    add_defines("NDEBUG")
    set_policy("build.optimization.lto", true)

    if is_plat("windows") then
        set_runtimes("MT")  -- static CRT, no VC++ redistributable needed
        -- /Gy /Gw: per-function/data sections, /Ob3: aggressive inlining
        add_cxflags("/Gy", "/Gw", "/Ob3", "/GS-")
        add_ldflags("/OPT:REF", "/OPT:ICF")
        if has_config("native") then
            add_cxflags("/arch:AVX2")
        end
    else
        add_cxflags("-ffunction-sections", "-fdata-sections",
                    "-fno-plt", "-fno-semantic-interposition")
        if is_plat("macosx") then
            add_ldflags("-Wl,-dead_strip")
        else
            add_ldflags("-Wl,--gc-sections", "-Wl,-O2", "-Wl,--as-needed",
                        "-static-libstdc++", "-static-libgcc")
        end
        if has_config("native") then
            add_cxflags("-march=native", "-mtune=native")
        end
    end
end

add_requires("cli11", "nlohmann_json", "spdlog", "tl_expected")
add_requires("catch2 3.x", {optional = true})

if is_mode("dist") then
    -- build dependencies with LTO too so they can be inlined across libraries
    local configs = {lto = true}
    if is_plat("windows") then
        configs.runtimes = "MT"
    end
    if has_config("native") then
        configs.cxflags = is_plat("windows") and "/arch:AVX2" or "-march=native"
    end
    add_requireconfs("*", {configs = configs})
end

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

-- `xmake icons` regenerates the logo and app icons. The generator is its own
-- xmake project in tools/icons, so its dependencies never touch normal builds.
task("icons")
    set_category("action")
    set_menu {usage = "xmake icons", description = "Regenerate the logo and app icons", options = {}}
    on_run(function ()
        local root = os.projectdir()
        os.cd(path.join(root, "tools", "icons"))
        os.exec("xmake f -P . -m release -y")
        os.exec("xmake build -P . -y")
        os.execv("xmake", {"run", "-P", ".", "basalt-icons", root})
    end)
