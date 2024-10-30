const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const apps = .{ "simsti", "rtrrecv" };
    const flags = &.{"-std=c++20"};
    inline for (apps) |name| {
        const exe = b.addExecutable(.{
            .name = name,
            .target = target,
            .optimize = optimize,
        });
        exe.defineCMacro("FMT_HEADER_ONLY", "");
        exe.defineCMacro("SPDLOG_FMT_EXTERNAL", "");
        exe.defineCMacro("SPDLOG_FMT_EXTERNAL_HO", "");
        exe.defineCMacro("_WIN32_WINNT", "0x601");
        exe.defineCMacro("Boost_USE_STATIC_LIBS", "");
        exe.want_lto = false;

        exe.linkLibC();
        exe.linkLibCpp();

        if (target.result.os.tag == .windows) {
            exe.linkSystemLibrary("ws2_32");
            exe.linkSystemLibrary("Mswsock");

            // const boost_dir = std.c.getenv("BOOST_LATEST");
            // exe.addIncludePath(std.c.getenv("SIMPLE_CPP"));
            // exe.addIncludePath(std.c.getenv("VCPKG_LATEST") + "");
            // exe.addLibraryPath(boost_dir + "/lib64-msvc-14.2");
        }
        exe.addIncludePath(.{ .cwd_relative = "D:/Local/boost_1_84_0/" });
        exe.addIncludePath(.{ .cwd_relative = "D:/Code/simple/" });
        exe.addIncludePath(.{ .cwd_relative = "D:/Local/vcpkg-latest/installed/x64-windows/include/" });
        const cpps = std.fmt.allocPrint(std.heap.page_allocator, "{s}.cpp", .{name}) catch "";
        exe.addCSourceFile(.{ .file = b.path(cpps), .flags = flags });

        b.installArtifact(exe);
        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());
        if (b.args) |args| run_cmd.addArgs(args);
        const run_step = b.step(name, "Run ");
        run_step.dependOn(&run_cmd.step);
    }
}

// zig build -Doptimize=ReleaseFast  -Dtarget=aarch64-linux
// zig build -Doptimize=ReleaseSmall -Dtarget=x86-linux-musl --prefix-exe-dir out-linux
