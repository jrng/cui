// C_MAKE_COMPILER_FLAGS = "-std=c99 -Wall -Wextra -pedantic"
#define C_MAKE_IMPLEMENTATION
#include "src/c_make.h"

static bool
cui_c_make_configuration_is_valid(CMakeLogLevel log_level)
{
    bool result = true;

    bool cui_backend_x11_enabled     = c_make_config_is_enabled("cui_backend_x11"    , true);
    bool cui_backend_wayland_enabled = c_make_config_is_enabled("cui_backend_wayland", true);

    bool cui_renderer_software_enabled   = c_make_config_is_enabled("cui_renderer_software"  , true);
    bool cui_renderer_opengles2_enabled  = c_make_config_is_enabled("cui_renderer_opengles2" , true);
    bool cui_renderer_metal_enabled      = c_make_config_is_enabled("cui_renderer_metal"     , true);
    bool cui_renderer_direct3d11_enabled = c_make_config_is_enabled("cui_renderer_direct3d11", true);

    switch (c_make_get_target_platform())
    {
        case CMakePlatformAndroid:
        {
            if (!cui_renderer_opengles2_enabled)
            {
                c_make_log(log_level, "The OpenGL ES 2 renderer needs to be enabled.\n"
                                      "   Set cui_renderer_opengles2 in 'c_make.txt' to \"on\"\n");
                result = false;
            }
        } break;

        case CMakePlatformFreeBsd:
        {
            c_make_log(log_level, "Platform FreeBSD is not supported.\n");
            result = false;
        } break;

        case CMakePlatformWindows:
        {
            if (!cui_renderer_software_enabled && !cui_renderer_direct3d11_enabled)
            {
                c_make_log(log_level, "Please provide at least one renderer for cui. Software and/or Direct3D 11.\n"
                                      "   Set cui_renderer_software or cui_renderer_direct3d11 in 'c_make.txt' to \"on\"\n");
                result = false;
            }
        } break;

        case CMakePlatformLinux:
        {
            if (!cui_backend_x11_enabled && !cui_backend_wayland_enabled)
            {
                c_make_log(log_level, "Please provide at least one backend for cui. X11 and/or Wayland.\n"
                                      "   Set cui_backend_x11 or cui_backend_wayland in 'c_make.txt' to \"on\"\n");
                result = false;
            }

            if (!cui_renderer_software_enabled && !cui_renderer_opengles2_enabled)
            {
                c_make_log(log_level, "Please provide at least one renderer for cui. Software and/or OpenGL ES 2.\n"
                                      "   Set cui_renderer_software or cui_renderer_opengles2 in 'c_make.txt' to \"on\"\n");
                result = false;
            }
        } break;

        case CMakePlatformMacOs:
        {
            if (!cui_renderer_software_enabled && !cui_renderer_metal_enabled)
            {
                c_make_log(log_level, "Please provide at least one renderer for cui. Software and/or Metal.\n"
                                      "   Set cui_renderer_software or cui_renderer_metal in 'c_make.txt' to \"on\"\n");
                result = false;
            }
        } break;

        case CMakePlatformWeb:
        {
            c_make_log(log_level, "Platform Web is not supported.\n");
            result = false;
        } break;
    }

    return result;
}

static void
cui_c_make_append_default_compiler_flags(CMakeCommand *command, CMakeArchitecture target_architecture)
{
    const char *target_c_compiler = c_make_get_target_c_compiler();

    c_make_command_append(command, target_c_compiler);
    c_make_command_append_command_line(command, c_make_get_target_c_flags());

    switch (c_make_get_target_platform())
    {
        case CMakePlatformAndroid:
        {
            c_make_command_append(command, "-std=c99");
        } break;

        case CMakePlatformFreeBsd:
        {
        } break;

        case CMakePlatformWindows:
        {
            if (c_make_compiler_is_msvc(target_c_compiler))
            {
                c_make_command_append_msvc_compiler_flags(command);
                c_make_command_append(command, "-nologo");
                c_make_command_append(command, "-MT"); // static linkage of c standard library
            }
            else
            {
                c_make_command_append(command, "-std=c99", "-municode");
                c_make_command_append(command, "-Wno-missing-field-initializers", "-Wno-cast-function-type", "-Wno-unused-parameter");
            }
        } break;

        case CMakePlatformLinux:
        {
            c_make_command_append(command, "-std=gnu99");
        } break;

        case CMakePlatformMacOs:
        {
            c_make_command_append(command, "-ObjC", "-std=c99", "-Wno-nullability-completeness");
        } break;

        case CMakePlatformWeb:
        {
        } break;
    }

    switch (c_make_get_build_type())
    {
        case CMakeBuildTypeDebug:
        {
            if (c_make_compiler_is_msvc(target_c_compiler))
            {
                c_make_command_append(command, "-Od", "-Z7");
                c_make_command_append(command, "-DCUI_DEBUG_BUILD=1");
            }
            else
            {
                c_make_command_append(command, "-pedantic", "-Wall", "-Wextra", "-g", "-O0");
                c_make_command_append(command, "-DCUI_DEBUG_BUILD=1");
            }
        } break;

        case CMakeBuildTypeRelDebug:
        {
            if (c_make_compiler_is_msvc(target_c_compiler))
            {
                c_make_command_append(command, "-O2", "-Z7");
                c_make_command_append(command, "-DCUI_DEBUG_BUILD=1");
            }
            else
            {
                c_make_command_append(command, "-g", "-O2");
                c_make_command_append(command, "-DCUI_DEBUG_BUILD=1");
            }
        } break;

        case CMakeBuildTypeRelease:
        {
            c_make_command_append(command, "-O2", "-DNDEBUG");
        } break;
    }

    switch (c_make_get_target_platform())
    {
        case CMakePlatformAndroid:
        {
            switch (target_architecture)
            {
                case CMakeArchitectureUnknown: break;
                case CMakeArchitectureAmd64:   c_make_command_append(command, "-target", "x86_64-linux-android24" /* TODO: add minSdkVersion */);  break;
                case CMakeArchitectureAarch64: c_make_command_append(command, "-target", "aarch64-linux-android24" /* TODO: add minSdkVersion */); break;
                case CMakeArchitectureRiscv64: break;
                case CMakeArchitectureWasm32:  break;
                case CMakeArchitectureWasm64:  break;
            }
        } break;

        case CMakePlatformFreeBsd: break;
        case CMakePlatformWindows: break;
        case CMakePlatformLinux:   break;

        case CMakePlatformMacOs:
        {
            switch (target_architecture)
            {
                case CMakeArchitectureUnknown: break;
                case CMakeArchitectureAmd64:   c_make_command_append(command, "-target", "x86_64-apple-macos10.14"); break;
                case CMakeArchitectureAarch64: c_make_command_append(command, "-target", "arm64-apple-macos11");     break;
                case CMakeArchitectureRiscv64: break;
                case CMakeArchitectureWasm32:  break;
                case CMakeArchitectureWasm64:  break;
            }
        } break;

        case CMakePlatformWeb: break;
    }
}

static void
cui_c_make_append_defines(CMakeCommand *command)
{
    bool cui_backend_x11_enabled     = c_make_config_is_enabled("cui_backend_x11"    , true);
    bool cui_backend_wayland_enabled = c_make_config_is_enabled("cui_backend_wayland", true);

    bool cui_renderer_software_enabled   = c_make_config_is_enabled("cui_renderer_software"  , true);
    bool cui_renderer_opengles2_enabled  = c_make_config_is_enabled("cui_renderer_opengles2" , true);
    bool cui_renderer_metal_enabled      = c_make_config_is_enabled("cui_renderer_metal"     , true);
    bool cui_renderer_direct3d11_enabled = c_make_config_is_enabled("cui_renderer_direct3d11", true);

    bool cui_framebuffer_screenshot_enabled = c_make_config_is_enabled("cui_framebuffer_screenshot", false);
    bool cui_renderer_opengles2_render_times_enabled = c_make_config_is_enabled("cui_renderer_opengles2_render_times", false);

    switch (c_make_get_target_platform())
    {
        case CMakePlatformAndroid:
        {
            if (cui_renderer_opengles2_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_OPENGLES2_ENABLED=1");

                if (cui_renderer_opengles2_render_times_enabled)
                {
                    c_make_command_append(command, "-DCUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED=1");
                }
            }
        } break;

        case CMakePlatformFreeBsd:
        {
        } break;

        case CMakePlatformWindows:
        {
            if (cui_renderer_software_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_SOFTWARE_ENABLED=1");
            }

            if (cui_renderer_direct3d11_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_DIRECT3D11_ENABLED=1");
            }
        } break;

        case CMakePlatformLinux:
        {
            if (cui_backend_x11_enabled)
            {
                c_make_command_append(command, "-DCUI_BACKEND_X11_ENABLED=1");
            }

            if (cui_backend_wayland_enabled)
            {
                c_make_command_append(command, "-DCUI_BACKEND_WAYLAND_ENABLED=1");
            }

            if (cui_renderer_software_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_SOFTWARE_ENABLED=1");
            }

            if (cui_renderer_opengles2_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_OPENGLES2_ENABLED=1");

                if (cui_renderer_opengles2_render_times_enabled)
                {
                    c_make_command_append(command, "-DCUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED=1");
                }
            }
        } break;

        case CMakePlatformMacOs:
        {
            if (cui_renderer_software_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_SOFTWARE_ENABLED=1");
            }

            if (cui_renderer_metal_enabled)
            {
                c_make_command_append(command, "-DCUI_RENDERER_METAL_ENABLED=1");
            }
        } break;

        case CMakePlatformWeb:
        {
        } break;
    }

    if (cui_framebuffer_screenshot_enabled)
    {
        c_make_command_append(command, "-DCUI_FRAMEBUFFER_SCREENSHOT_ENABLED=1");
    }
}

static void
cui_c_make_append_linker_flags(CMakeCommand *command, CMakeArchitecture target_architecture)
{
    bool cui_backend_x11_enabled     = c_make_config_is_enabled("cui_backend_x11"    , true);
    bool cui_backend_wayland_enabled = c_make_config_is_enabled("cui_backend_wayland", true);

    bool cui_renderer_opengles2_enabled  = c_make_config_is_enabled("cui_renderer_opengles2" , true);
    bool cui_renderer_metal_enabled      = c_make_config_is_enabled("cui_renderer_metal"     , true);
    bool cui_renderer_direct3d11_enabled = c_make_config_is_enabled("cui_renderer_direct3d11", true);

    const char *target_c_compiler = c_make_get_target_c_compiler();

    switch (c_make_get_target_platform())
    {
        case CMakePlatformAndroid:
        {
            c_make_command_append(command, "-static-libgcc", "-lm", "-llog", "-landroid");

            if (cui_renderer_opengles2_enabled)
            {
                c_make_command_append(command, "-lEGL", "-lGLESv2");
            }
        } break;

        case CMakePlatformFreeBsd:
        {
        } break;

        case CMakePlatformWindows:
        {
            if (c_make_compiler_is_msvc(target_c_compiler))
            {
                c_make_command_append(command, "-link");
                c_make_command_append_msvc_linker_flags(command, target_architecture);
                c_make_command_append(command, "user32.lib", "gdi32.lib", "shell32.lib", "uxtheme.lib", "comdlg32.lib");

                if (cui_renderer_direct3d11_enabled)
                {
                    c_make_command_append(command, "d3d11.lib", "dxguid.lib", "d3dcompiler.lib");
                }
            }
            else
            {
                c_make_command_append(command, "-luser32", "-lgdi32", "-lshell32", "-luxtheme", "-lcomdlg32");

                if (cui_renderer_direct3d11_enabled)
                {
                    c_make_command_append(command, "-ld3d11", "-ldxguid", "-ld3dcompiler");
                }
            }
        } break;

        case CMakePlatformLinux:
        {
            c_make_command_append(command, "-lm", "-pthread", "-ldl");

            if (cui_backend_x11_enabled)
            {
                c_make_command_append(command, "-lX11", "-lXext", "-lXrandr");
            }

            if (cui_backend_wayland_enabled)
            {
                c_make_command_append(command, "-lwayland-client", "-lwayland-cursor", "-lxkbcommon");

                if (cui_renderer_opengles2_enabled)
                {
                    c_make_command_append(command, "-lwayland-egl");
                }
            }

            if (cui_renderer_opengles2_enabled)
            {
                c_make_command_append(command, "-lEGL", "-lGLESv2");
            }
        } break;

        case CMakePlatformMacOs:
        {
            c_make_command_append(command, "-framework", "AppKit");

            if (cui_renderer_metal_enabled)
            {
                c_make_command_append(command, "-framework", "Quartz", "-framework", "Metal");
            }
        } break;

        case CMakePlatformWeb:
        {
        } break;
    }
}

static void
cui_c_make_build_shared_library(const char *library_name, CMakeArchitecture target_architecture)
{
    // TODO: abstract from platform
    const char *library_filename = c_make_c_string_concat("lib", library_name, ".so");

    CMakeCommand command = { 0 };

    cui_c_make_append_default_compiler_flags(&command, target_architecture);
    cui_c_make_append_defines(&command);

    // TODO: platform dependent
    c_make_command_append(&command, "-fPIC", "-shared");

    c_make_command_append(&command, c_make_c_string_concat("-I", c_make_c_string_path_concat(c_make_get_source_path(), "include")));

    // TODO: abstract from platform
    c_make_command_append(&command, "-o", c_make_c_string_path_concat(c_make_get_build_path(), library_filename));
    c_make_command_append(&command, c_make_c_string_path_concat(c_make_get_source_path(), "src", "cui.c"));

    cui_c_make_append_linker_flags(&command, target_architecture);

    c_make_log(CMakeLogLevelInfo, "compile '%s'\n", library_filename);
    c_make_command_run(command);
}

static void
cui_c_make_build_static_library(const char *library_name, CMakeArchitecture target_architecture)
{
    const char *object_filename;
    const char *library_filename;

    if (c_make_get_target_platform() == CMakePlatformWindows)
    {
        object_filename = c_make_c_string_concat(library_name, ".obj");
        library_filename = c_make_c_string_concat(library_name, ".lib");
    }
    else
    {
        object_filename = c_make_c_string_concat(library_name, ".o");
        library_filename = c_make_c_string_concat("lib", library_name, ".a");
    }

    CMakeCommand command = { 0 };

    cui_c_make_append_default_compiler_flags(&command, target_architecture);
    cui_c_make_append_defines(&command);

    c_make_command_append(&command, c_make_c_string_concat("-I", c_make_c_string_path_concat(c_make_get_source_path(), "include")));

    c_make_command_append(&command, "-c");
    c_make_command_append_output_object(&command, c_make_c_string_path_concat(c_make_get_build_path(), library_name), c_make_get_target_platform());

    c_make_command_append(&command, c_make_c_string_path_concat(c_make_get_source_path(), "src", "cui.c"));

    c_make_log(CMakeLogLevelInfo, "compile '%s'\n", object_filename);
    c_make_command_run_and_reset_and_wait(&command);

    const char *ar_cmd = c_make_get_target_ar();

    c_make_command_append(&command, ar_cmd);

    if (c_make_is_msvc_library_manager(ar_cmd))
    {
        c_make_command_append(&command, "-nologo", c_make_c_string_concat("-out:", c_make_c_string_path_concat(c_make_get_build_path(), library_filename)),
                                                   c_make_c_string_path_concat(c_make_get_build_path(), object_filename));
    }
    else
    {
        c_make_command_append(&command, "-rc", c_make_c_string_path_concat(c_make_get_build_path(), library_filename),
                                               c_make_c_string_path_concat(c_make_get_build_path(), object_filename));
    }

    c_make_log(CMakeLogLevelInfo, "generate '%s'\n", library_filename);
    c_make_command_run(command);
}

static void
cui_c_make_build(const char *output_name, const char *input_name, CMakeArchitecture target_architecture)
{
    CMakeCommand command = { 0 };

    cui_c_make_append_default_compiler_flags(&command, target_architecture);

    c_make_command_append(&command, c_make_c_string_concat("-I", c_make_c_string_path_concat(c_make_get_source_path(), "include")));

    if (c_make_get_target_platform() == CMakePlatformAndroid)
    {
        c_make_command_append(&command, "-shared");
        const char *library_filename = c_make_c_string_concat("lib", output_name, ".so");
        c_make_command_append(&command, "-o", c_make_c_string_path_concat(c_make_get_build_path(), library_filename));
    }
    else
    {
        c_make_command_append_output_executable(&command, c_make_c_string_path_concat(c_make_get_build_path(), output_name), c_make_get_target_platform());
    }

    c_make_command_append(&command, c_make_c_string_path_concat(c_make_get_source_path(), "examples", input_name));
    c_make_command_append_input_static_library(&command, c_make_c_string_path_concat(c_make_get_build_path(), "cui"), c_make_get_target_platform());

    cui_c_make_append_linker_flags(&command, target_architecture);

    c_make_log(CMakeLogLevelInfo, "compile '%s'\n", output_name);
    c_make_command_run(command);
}

static void
cui_c_make_build_example(const char *example_name, const char *executable_name)
{
    if ((c_make_get_target_platform() == CMakePlatformMacOs) && (c_make_get_target_architecture() == CMakeArchitectureAarch64))
    {
        cui_c_make_build(c_make_c_string_concat(executable_name, "-arm64") , c_make_c_string_concat(executable_name, ".c"), CMakeArchitectureAarch64);
        cui_c_make_build(c_make_c_string_concat(executable_name, "-x86_64"), c_make_c_string_concat(executable_name, ".c"), CMakeArchitectureAmd64);

        c_make_process_wait_for_all();

        CMakeCommand command = { 0 };

        c_make_command_append(&command, "lipo", "-create", "-output", 
                              c_make_c_string_path_concat(c_make_get_build_path(), executable_name),
                              c_make_c_string_path_concat(c_make_get_build_path(), c_make_c_string_concat(executable_name, "-arm64")),
                              c_make_c_string_path_concat(c_make_get_build_path(), c_make_c_string_concat(executable_name, "-x86_64")));

        c_make_log(CMakeLogLevelInfo, "generate '%s'\n", executable_name);
        c_make_command_run(command);
    }
    else if (c_make_get_target_platform() == CMakePlatformAndroid)
    {
        const char *arch_folder = "";

        switch (c_make_get_target_architecture())
        {
            case CMakeArchitectureUnknown: break;
            case CMakeArchitectureAmd64:   arch_folder = "x86_64";    break;
            case CMakeArchitectureAarch64: arch_folder = "arm64-v8a"; break;
            case CMakeArchitectureRiscv64: break;
            case CMakeArchitectureWasm32:  break;
            case CMakeArchitectureWasm64:  break;
        }

        const char *library_path = c_make_c_string_path_concat(c_make_get_build_path(), example_name, "apk_build", "lib", arch_folder);

        c_make_create_directory_recursively(library_path);
        c_make_create_directory_recursively(c_make_c_string_path_concat(c_make_get_build_path(), example_name, "res", "values"));

        cui_c_make_build(executable_name, c_make_c_string_concat(executable_name, ".c"), c_make_get_target_architecture());

        c_make_process_wait_for_all();

        const char *library_filename = c_make_c_string_concat("lib", executable_name, ".so");
        const char *manifest_filename = c_make_c_string_path_concat(c_make_get_build_path(), example_name, "AndroidManifest.xml");
        const char *key_filename = c_make_c_string_path_concat(c_make_get_build_path(), example_name, "debug.keystore");
        // TODO: use example_name, but without whitespaces
        const char *apk_filename = c_make_c_string_path_concat(c_make_get_build_path(), example_name, c_make_c_string_concat(executable_name, ".apk"));
        const char *apk_signed_filename = c_make_c_string_path_concat(c_make_get_build_path(), example_name, c_make_c_string_concat(executable_name, ".signed.apk"));
        const char *apk_unaligned_filename = c_make_c_string_path_concat(c_make_get_build_path(), example_name, c_make_c_string_concat(executable_name, ".unaligned.apk"));

        c_make_log(CMakeLogLevelInfo, "package '%s.apk'\n", executable_name);

        // TODO: build this file directly in the right location
        c_make_copy_file(c_make_c_string_path_concat(c_make_get_build_path(), library_filename), c_make_c_string_path_concat(library_path, library_filename));

        c_make_write_entire_file(manifest_filename,
                                 c_make_string_replace_all(CMakeStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                                                              "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                                                                              "          package=\"com.{{executable_name}}\"\n"
                                                                              "          android:versionCode=\"1\"\n"
                                                                              "          android:versionName=\"1.0\"\n"
                                                                              "          android:debuggable=\"true\">\n"
                                                                              "  <uses-sdk android:minSdkVersion=\"24\" />\n"
                                                                              "  <application android:label=\"@string/app_name\"\n"
                                                                              "               android:theme=\"@style/CustomStyle\"\n"
                                                                              "               android:hasCode=\"false\">\n"
                                                                              "    <activity android:name=\"android.app.NativeActivity\"\n"
                                                                              "              android:label=\"@string/app_name\"\n"
                                                                              "              android:configChanges=\"orientation|screenSize|screenLayout|keyboardHidden|density\">\n"
                                                                              "      <meta-data android:name=\"android.app.lib_name\"\n"
                                                                              "                 android:value=\"{{executable_name}}\" />\n"
                                                                              "      <intent-filter>\n"
                                                                              "        <action android:name=\"android.intent.action.MAIN\" />\n"
                                                                              "        <category android:name=\"android.intent.category.LAUNCHER\" />\n"
                                                                              "      </intent-filter>\n"
                                                                              "    </activity>\n"
                                                                              "  </application>\n"
                                                                              "</manifest>\n"),
                                                           CMakeStringLiteral("{{executable_name}}"),
                                                           CMakeCString(executable_name)));

        c_make_write_entire_file(c_make_c_string_path_concat(c_make_get_build_path(), example_name, "res", "values", "strings.xml"),
                                 c_make_string_replace_all(CMakeStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                                                              "<resources>\n"
                                                                              "  <string name=\"app_name\">{{app_name}}</string>\n"
                                                                              "</resources>\n"),
                                                           CMakeStringLiteral("{{app_name}}"),
                                                           CMakeCString(example_name)));

        c_make_write_entire_file(c_make_c_string_path_concat(c_make_get_build_path(), example_name, "res", "values", "styles.xml"),
                                 CMakeStringLiteral("<resources>\n"
                                                    "  <style name=\"CustomStyle\">\n"
                                                    "    <item name=\"android:windowActionBar\">false</item>\n"
                                                    "    <item name=\"android:windowNoTitle\">true</item>\n"
                                                    "  </style>\n"
                                                    "</resources>\n"));

        CMakeCommand command = { 0 };

        if (!c_make_file_exists(key_filename))
        {
            c_make_command_append(&command, c_make_get_java_keytool(), "-genkey", "-keystore", key_filename);
            c_make_command_append(&command, "-storepass", "android", "-alias", "androiddebugkey", "-dname", "CN=company name, OU=0, O=Dream, L=City, S=StateProvince, C=Country");
            c_make_command_append(&command, "-keyalg", "RSA", "-keysize", "2048", "-validity", "20000");

            c_make_command_run_and_reset_and_wait(&command);
        }

        c_make_command_append(&command, c_make_get_android_aapt(), "package", "--debug-mode", "-f", "-M", manifest_filename);
        c_make_command_append(&command, "-S", c_make_c_string_path_concat(c_make_get_build_path(), example_name, "res"));
        c_make_command_append(&command, "-I", c_make_get_android_platform_jar());
        c_make_command_append(&command, "-F", apk_unaligned_filename);
        c_make_command_append(&command, c_make_c_string_path_concat(c_make_get_build_path(), example_name, "apk_build"));

        c_make_command_run_and_reset_and_wait(&command);

        c_make_command_append(&command, c_make_get_java_jarsigner(), "-sigalg", "SHA1withRSA", "-digestalg", "SHA1");
        c_make_command_append(&command, "-storepass", "android", "-keypass", "android", "-keystore", key_filename);
        c_make_command_append(&command, "-signedjar", apk_signed_filename, apk_unaligned_filename, "androiddebugkey");

        c_make_command_run_and_reset_and_wait(&command);

        c_make_command_append(&command, c_make_get_android_zipalign(), "-f", "4", apk_signed_filename, apk_filename);

        c_make_command_run_and_reset_and_wait(&command);
    }
    else
    {
        cui_c_make_build(executable_name, c_make_c_string_concat(executable_name, ".c"), c_make_get_target_architecture());
    }
}

C_MAKE_ENTRY()
{
    switch (c_make_target)
    {
        case CMakeTargetSetup:
        {
            c_make_config_set_if_not_exists("cui_backend_x11"    , "on");
            c_make_config_set_if_not_exists("cui_backend_wayland", "on");

            c_make_config_set_if_not_exists("cui_renderer_software"  , "on");
            c_make_config_set_if_not_exists("cui_renderer_opengles2" , "on");
            c_make_config_set_if_not_exists("cui_renderer_metal"     , "on");
            c_make_config_set_if_not_exists("cui_renderer_direct3d11", "on");

            c_make_config_set_if_not_exists("cui_framebuffer_screenshot", "off");
            c_make_config_set_if_not_exists("cui_renderer_opengles2_render_times", "off");

            if (!cui_c_make_configuration_is_valid(CMakeLogLevelWarning))
            {
                return;
            }

            if (c_make_get_target_platform() == CMakePlatformAndroid)
            {
                if (!c_make_setup_java(true))
                {
                    return;
                }

                if (!c_make_setup_android(true))
                {
#if C_MAKE_PLATFORM_LINUX || C_MAKE_PLATFORM_MACOS
                    c_make_log(CMakeLogLevelRaw, "\n");
                    c_make_log(CMakeLogLevelRaw, "  To install the Android SDK you can follow these steps:\n\n");
                    c_make_log(CMakeLogLevelRaw, "    1. Download the Command Line Tools from https://developer.android.com/studio.\n");
                    c_make_log(CMakeLogLevelRaw, "    2. Extract the Command Line Tools and install the Android SDK:\n\n");
                    c_make_log(CMakeLogLevelRaw, "      $ mkdir -p ~/opt/android_sdk/cmdline-tools\n");
#  if C_MAKE_PLATFORM_LINUX
                    c_make_log(CMakeLogLevelRaw, "      $ unzip commandlinetools-linux-<version>_latest.zip -d ~/opt/android_sdk/cmdline-tools/\n");
#  elif C_MAKE_PLATFORM_MACOS
                    c_make_log(CMakeLogLevelRaw, "      $ unzip commandlinetools-mac-<version>_latest.zip -d ~/opt/android_sdk/cmdline-tools/\n");
#  endif
                    c_make_log(CMakeLogLevelRaw, "      $ mv ~/opt/android_sdk/cmdline-tools/cmdline-tools ~/opt/android_sdk/cmdline-tools/latest\n");
                    c_make_log(CMakeLogLevelRaw, "      $ export PATH=\"$HOME/opt/android_sdk/cmdline-tools/latest/bin:$PATH\"\n");
                    c_make_log(CMakeLogLevelRaw, "      $ sdkmanager --install \"build-tools;24.0.3\"\n");
                    c_make_log(CMakeLogLevelRaw, "      $ sdkmanager --install \"platforms;android-24\"\n");
                    c_make_log(CMakeLogLevelRaw, "      $ sdkmanager --install \"ndk;21.4.7075529\"\n");
                    c_make_log(CMakeLogLevelRaw, "\n");
#endif
                    return;
                }
            }
        } break;

        case CMakeTargetBuild:
        {
            if (!cui_c_make_configuration_is_valid(CMakeLogLevelError))
            {
                c_make_set_failed(true);
                return;
            }

            CMakeCommand command = { 0 };

            const char *host_c_compiler = c_make_get_host_c_compiler();

            c_make_command_append(&command, host_c_compiler);
            c_make_command_append_default_compiler_flags(&command, c_make_get_build_type());
            c_make_command_append_output_executable(&command, c_make_c_string_path_concat(c_make_get_build_path(), "shape_compile"), c_make_get_host_platform());
            c_make_command_append(&command, c_make_c_string_path_concat(c_make_get_source_path(), "src", "shape_compile.c"));
            c_make_command_append_default_linker_flags(&command, c_make_get_host_architecture());

            c_make_log(CMakeLogLevelInfo, "compile 'shape_compile'\n");
            c_make_command_run_and_reset(&command);

            switch (c_make_get_target_platform())
            {
                case CMakePlatformAndroid:
                {
                    cui_c_make_build_static_library("cui", c_make_get_target_architecture());
                } break;

                case CMakePlatformFreeBsd:
                {
                } break;

                case CMakePlatformWindows:
                {
                    // cui_c_make_build_shared_library("cui", c_make_get_target_architecture());
                    cui_c_make_build_static_library("cui", c_make_get_target_architecture());
                } break;

                case CMakePlatformLinux:
                {
                    cui_c_make_build_shared_library("cui", c_make_get_target_architecture());
                    cui_c_make_build_static_library("cui", c_make_get_target_architecture());
                } break;

                case CMakePlatformMacOs:
                {
                    if (c_make_get_host_architecture() == CMakeArchitectureAarch64)
                    {
                        cui_c_make_build_shared_library("cui-arm64" , CMakeArchitectureAarch64);
                        cui_c_make_build_shared_library("cui-x86_64", CMakeArchitectureAmd64);
                        cui_c_make_build_static_library("cui-arm64" , CMakeArchitectureAarch64);
                        cui_c_make_build_static_library("cui-x86_64", CMakeArchitectureAmd64);

                        c_make_process_wait_for_all();

                        CMakeCommand command = { 0 };

                        c_make_command_append(&command, "lipo", "-create", "-output", 
                                              c_make_c_string_path_concat(c_make_get_build_path(), "libcui.so"),
                                              c_make_c_string_path_concat(c_make_get_build_path(), "libcui-arm64.so"),
                                              c_make_c_string_path_concat(c_make_get_build_path(), "libcui-x86_64.so"));

                        c_make_log(CMakeLogLevelInfo, "generate 'libcui.so'\n");
                        c_make_command_run_and_reset(&command);

                        c_make_command_append(&command, "lipo", "-create", "-output", 
                                              c_make_c_string_path_concat(c_make_get_build_path(), "libcui.a"),
                                              c_make_c_string_path_concat(c_make_get_build_path(), "libcui-arm64.a"),
                                              c_make_c_string_path_concat(c_make_get_build_path(), "libcui-x86_64.a"));

                        c_make_log(CMakeLogLevelInfo, "generate 'libcui.a'\n");
                        c_make_command_run_and_reset(&command);
                    }
                    else
                    {
                        cui_c_make_build_shared_library("cui", c_make_get_target_architecture());
                        cui_c_make_build_static_library("cui", c_make_get_target_architecture());
                    }
                } break;

                case CMakePlatformWeb:
                {
                } break;
            }

            c_make_process_wait_for_all();

            cui_c_make_build_example("Interface Browser", "interface_browser");
            cui_c_make_build_example("Image Viewer", "image_viewer");
            cui_c_make_build_example("File Search", "file_search");
            cui_c_make_build_example("Color Tool", "color_tool");
        } break;

        case CMakeTargetInstall:
        {
        } break;
    }
}
