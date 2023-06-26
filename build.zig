const std = @import("std");

pub fn build(b: *std.Build) void {
    b.vcpkg_root = .{ .found = "D:/Local/vcpkg-latest/" };

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const apps = .{ "simsti", "rtrrecv" };
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

        if (target.isWindows()) {
            exe.linkSystemLibraryName("ws2_32");
            exe.linkSystemLibraryName("Mswsock");
            exe.addLibraryPath("D:/Local/boost_1_81_0/lib64-msvc-14.2");
        }
        exe.addIncludePath("D:/Code/simple");
        exe.addIncludePath("D:/Local/boost_1_81_0");
        exe.addIncludePath("D:/Local/vcpkg-latest/installed/x64-windows/include/");
        const cpps = std.fmt.allocPrint(std.heap.page_allocator, "{s}.cpp", .{name}) catch "";
        exe.addCSourceFiles(&.{cpps}, &.{"-std=c++20"});

        b.installArtifact(exe);
        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());
        if (b.args) |args| run_cmd.addArgs(args);
        const run_step = b.step(name, "Run ");
        run_step.dependOn(&run_cmd.step);
    }
}

// zig build -Dtarget=aarch64-linux -Doptimize=ReleaseFast
