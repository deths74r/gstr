const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Create the gstr module
    const gstr_mod = b.createModule(.{
        .root_source_file = b.path("src/gstr.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Add include path for C header
    gstr_mod.addIncludePath(b.path("include"));

    // Export as a named module for dependencies
    _ = b.addModule("gstr", .{
        .root_source_file = b.path("src/gstr.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Tests - create a module for testing
    const test_mod = b.createModule(.{
        .root_source_file = b.path("src/gstr.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    test_mod.addIncludePath(b.path("include"));

    const tests = b.addTest(.{
        .root_module = test_mod,
    });

    const run_tests = b.addRunArtifact(tests);
    const test_step = b.step("test", "Run gstr unit tests");
    test_step.dependOn(&run_tests.step);
}
