local build_dir = "build/" .. _ACTION

newoption {
    trigger = "cxxflags",
    description = "Additional build options",
}

newoption {
    trigger = "linkflags",
    description = "Additional linker options",
}

--------------------------------------------------------------------------------
workspace "Json"
    configurations { "release", "debug" }
    architecture "x64"

    filter {}

    location    (build_dir)
    objdir      (build_dir .. "/obj")

    warnings "Extra"

    -- exceptionhandling "Off"
    -- rtti "Off"

    flags {
        "StaticRuntime",
    }

    configuration { "debug" }
        targetdir (build_dir .. "/bin/debug")

    configuration { "release" }
        targetdir (build_dir .. "/bin/release")

    configuration { "debug" }
        defines { "_DEBUG" }
        symbols "On"

    configuration { "release" }
        defines { "NDEBUG" }
        symbols "On" -- for profiling...
        optimize "Full"
            -- On ==> -O2
            -- Full ==> -O3

    configuration { "gmake" }
        buildoptions {
            -- "-std=c++14",
            "-march=native",
            "-Wformat",
            -- "-Wsign-compare",
            -- "-Wsign-conversion",
            -- "-pedantic",
            -- "-fno-omit-frame-pointer",
            -- "-ftime-report",
        }

    configuration { "gmake", "debug", "linux" }
        buildoptions {
            -- "-fno-omit-frame-pointer",
            -- "-fsanitize=undefined",
            -- "-fsanitize=address",
            -- "-fsanitize=memory",
            -- "-fsanitize-memory-track-origins",
        }
        linkoptions {
            -- "-fsanitize=undefined",
            -- "-fsanitize=address",
            -- "-fsanitize=memory",
        }

    configuration { "vs*" }
        buildoptions {
            "/utf-8",
            -- "/std:c++latest",
            -- "/EHsc",
            -- "/arch:AVX2",
            -- "/GR-",
        }
        defines {
            -- "_CRT_SECURE_NO_WARNINGS=1",
            -- "_SCL_SECURE_NO_WARNINGS=1",
            -- "_HAS_EXCEPTIONS=0",
        }

    configuration { "windows" }
        characterset "MBCS"

    if _OPTIONS["cxxflags"] then
        configuration {}
            buildoptions {
                _OPTIONS["cxxflags"],
            }
    else
        configuration { "gmake" }
            buildoptions {
                "-std=c++14",
            }
    end

    if _OPTIONS["linkflags"] then
        configuration {}
            linkoptions {
                _OPTIONS["linkflags"],
            }
    end

--------------------------------------------------------------------------------
group "Libs"

project "json"
    language "C++"
    kind "StaticLib"
    files {
        "src/**.cc",
        "src/**.h",
    }
    configuration { "gmake" }
        buildoptions {
            "-Wsign-compare",
            "-Wsign-conversion",
            "-Wold-style-cast",
            "-Wshadow",
            "-Wconversion",
            "-pedantic",
        }

--------------------------------------------------------------------------------
group "Tests"

project "test"
    language "C++"
    kind "ConsoleApp"
    files {
        "test/catch.hpp",
        "test/catch_main.cc",
        "test/test.cc",
    }
    links {
        "json",
    }
    configuration { "gmake" }
        buildoptions {
            "-Wsign-compare",
            "-Wsign-conversion",
            "-Wold-style-cast",
            "-Wshadow",
            "-Wconversion",
            "-pedantic",
        }

project "benchmark"
    language "C++"
    kind "ConsoleApp"
    files {
        "benchmark/*.cc",
        "benchmark/*.h",
    }
    links {
        "json",
    }
    configuration { "gmake" }
        buildoptions {
            "-Wsign-compare",
            "-Wsign-conversion",
            "-Wold-style-cast",
            "-Wshadow",
            "-Wconversion",
            "-pedantic",
        }
