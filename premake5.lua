local function mkdir_if_not_exist(path)
    if not os.isdir(path) then
        os.mkdir(path)
    end
end

local function internal_simple_win32_cpp(name, static_libs, clang_format_path, proj_kind)
    clang_format_path = clang_format_path or '.clang-format'

    local build_root = 'build/'
    mkdir_if_not_exist(build_root)

    local temp_root = 'temp/'
    mkdir_if_not_exist(temp_root)

    newaction {
        trigger = 'clean',
        description = 'clean generated project files',
        execute = function ()
            print 'clean the build...'
            os.rmdir(build_root)
            os.rmdir(temp_root)
            os.rmdir('.vs')
            os.execute('rm ' .. '*.sln')
            os.execute('rm ' .. '*.vcxproj')
            os.execute('rm ' .. '*.vcxproj.filters')
            os.execute('rm ' .. '*.vcxproj.user')
            print 'done.'
        end
    }

    workspace(name)
        configurations {'Debug', 'Release'}
        files(clang_format_path)

    project(name .. '_win32')
        kind(proj_kind)
        language 'C++'
        architecture 'x86_64'
        buildoptions '/std:c++latest'
        characterset 'MBCS'
        warnings 'Extra'
        flags {'FatalCompileWarnings'}
        disablewarnings {'4189', '4505', '4201', '4100', '4204', '4706', '4324'}
        defines {'_CRT_SECURE_NO_WARNINGS'}

        targetdir(build_root .. "%{cfg.buildcfg}")

        objdir(temp_root)

        local src_root = 'src/'
        if not os.isdir(src_root) then
            local f = io.open(src_root .. 'main.cpp', 'w')
            f:close()
        end
        mkdir_if_not_exist(src_root)

        local dep_root = 'dep/'
        includedirs(dep_root)
        mkdir_if_not_exist(dep_root)

        local wd_root = 'data/'
        debugdir(wd_root)
        mkdir_if_not_exist(wd_root)

        files {
            dep_root .. '**.h',
            dep_root .. '**.c',
            dep_root .. '**.cpp',
            src_root .. '**.h',
            -- src_root .. '*.hpp',
            src_root .. '**.c',
            src_root .. '**.cpp',
            src_root .. '**.inl',
        }

        links(static_libs)

        filter 'configurations:Debug'
            defines(string.upper(name) .. '_DEBUG')
            symbols 'On'

        filter 'configurations:Release'
            disablewarnings '4101'
            optimize 'On'
end

function simple_win32_windowed_cpp(name, static_libs, clang_format_path)
    internal_simple_win32_cpp(name, static_libs, clang_format_path, 'WindowedApp')
end

function simple_win32_console_cpp(name, static_libs, clang_format_path)
    internal_simple_win32_cpp(name, static_libs, clang_format_path, 'ConsoleApp')
end

simple_win32_windowed_cpp('zen', {'user32', 'winmm', 'opengl32'})
