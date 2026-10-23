# Clang Development Guide for CriptoGualet

## Quick Start

The checked-in presets are `windows-msvc` (default) and `windows-clang`. The
commands below reuse `out/build/windows-clang`; run
`.\scripts\build.ps1 -Preset windows-clang -Clean` before switching between
ASan and LTO configurations.

### Debug Development with AddressSanitizer (Recommended)

```powershell
# Configure with AddressSanitizer
cmake --preset windows-clang -DENABLE_ASAN=ON

# Build
cmake --build --preset windows-clang

# Run (with ASan environment)
$env:ASAN_OPTIONS = "detect_leaks=1:detect_stack_use_after_return=1"
.\out\build\windows-clang\bin\Debug\CriptoGualetQt.exe
```

### Release Development with LTO

```powershell
# Configure with Link-Time Optimization
cmake --preset windows-clang -DENABLE_LTO=ON

# Build
cmake --build --preset windows-clang --config Release

# Run
.\out\build\windows-clang\bin\Release\CriptoGualetQt.exe
```

## Advanced Clang Features

### AddressSanitizer (ASan)

- **Purpose**: Detect memory errors, buffer overflows, use-after-free
- **Performance Impact**: 2-3x slower runtime
- **Usage**: Security testing, debugging memory issues
- **Options**:
  - `detect_leaks=1` - Enable memory leak detection
  - `detect_stack_use_after_return=1` - Advanced stack detection

### UndefinedBehaviorSanitizer (UBSan)

- **Purpose**: Detect undefined behavior per C++ standard
- **Performance Impact**: Minimal
- **Usage**: Standards compliance testing
- **Enable with**: `-DENABLE_UBSAN=ON`

### Link-Time Optimization (LTO)

- **Purpose**: Optimize across translation units
- **Performance Impact**: Slower builds, 10-15% faster runtime
- **Usage**: Release builds, performance optimization
- **Enable with**: `-DENABLE_LTO=ON`

## Switching Between Compilers

### MSVC (Faster Debug Builds)

```powershell
# Configure for MSVC Debug
cmake --preset windows-msvc

# Build
cmake --build --preset windows-msvc
```

### ClangCL (Better Release Performance)

```powershell
# Configure for ClangCL Release with LTO
cmake --preset windows-clang -DENABLE_LTO=ON

# Build
cmake --build --preset windows-clang --config Release
```

## Security Testing Workflow

### 1. Development Phase

```powershell
# Use ClangCL + ASan for catching security issues
cmake --preset windows-clang -DENABLE_ASAN=ON
cmake --build --preset windows-clang
```

### 2. Testing Phase

```powershell
# Run tests with ASan
ctest --preset windows-clang --output-on-failure

# Check for ASan reports
Select-String -Path output.log -Pattern "runtime error"
```

### 3. Release Phase

```powershell
# Optimize for performance
cmake --preset windows-clang -DENABLE_LTO=ON
cmake --build --preset windows-clang --config Release
```

## Configuration Options

### CMake Options for Clang

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_ASAN` | ON (Debug) | Enable AddressSanitizer |
| `ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |
| `ENABLE_LTO` | OFF | Enable Link-Time Optimization |

### Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `ASAN_OPTIONS` | Configure ASan behavior | `detect_leaks=1:detect_stack_use_after_return=1` |
| `UBSAN_OPTIONS` | Configure UBSan behavior | `print_stacktrace=1` |

## Performance Comparison

| Configuration | Build Time | Runtime | Debug Info | Security |
|---------------|-------------|----------|-------------|----------|
| MSVC Debug | Fast | Slow | Excellent | Basic |
| ClangCL Debug | Slow | Slow | Good | ASan enabled |
| MSVC Release | Medium | Fast | Good | Basic |
| ClangCL Release | Slow | Fast | Good | Basic |
| ClangCL + LTO | Slowest | Fastest | Good | Basic |

## Troubleshooting

### AddressSanitizer Issues

**False Positives**:

```cpp
// Disable ASan for specific functions
__attribute__((no_sanitize("address")))
void* custom_allocator(size_t size) {
    // Custom memory management
}
```

**Performance Issues**:

- Disable ASan in production builds
- Use MSVC for faster debug builds when not debugging memory issues

**Common ASan Errors**:

- `heap-buffer-overflow` - Buffer overflow detection
- `use-after-free` - Dangling pointer usage
- `memory leak` - Memory not freed

### Link-Time Optimization Issues

**Link Errors**:

- Ensure all functions have definitions
- Check for missing `inline` specifiers
- Verify template instantiations

**Debug Info**:

- Use `-g` for better debugging with LTO
- LTO can obscure some debugging information

### Qt Integration Issues

**Missing Headers**:

```bash
# Verify Qt MSVC build path
ls "C:/Program Files/Qt/6.9.1/msvc2022_64/include"
```

**Runtime Errors**:

- Check Qt DLL compatibility
- Ensure MSVC-built Qt with ClangCL
- Verify Qt path in environment

## Best Practices

### 1. Development Workflow

```powershell
# Daily development: MSVC for speed
cmake --preset windows-msvc
cmake --build --preset windows-msvc

# Security testing: ClangCL + ASan
cmake --preset windows-clang -DENABLE_ASAN=ON
cmake --build --preset windows-clang

# Release builds: ClangCL + LTO
cmake --preset windows-clang -DENABLE_LTO=ON
cmake --build --preset windows-clang --config Release
```

### 2. Testing Strategy

- Unit tests: Run with both compilers
- Integration tests: Use ClangCL + ASan
- Performance tests: Use ClangCL + LTO
- Security tests: Use ClangCL + ASan + UBSan

### 3. CI/CD Integration

```yaml
# GitHub Actions example
- name: Configure ClangCL
  run: cmake --preset windows-clang -DENABLE_ASAN=ON

- name: Build ClangCL
  run: cmake --build --preset windows-clang

- name: Test ClangCL
  run: ctest --preset windows-clang --output-on-failure
```

## Advanced Usage

### Custom Sanitizer Options

```bash
# Advanced ASan configuration
export ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:quarantine_size_mb=256:malloc_context_size=5"

# Advanced UBSan configuration
export UBSAN_OPTIONS="print_stacktrace=1:print_summary=1"
```

### Performance Profiling

```bash
# Enable profiling with ClangCL
cmake -DENABLE_LTO=ON -DCMAKE_CXX_FLAGS="-fprofile-generate" .

# Generate optimized build
cmake -DENABLE_LTO=ON -DCMAKE_CXX_FLAGS="-fprofile-use" .
```

### Static Analysis Integration

```bash
# Run clang-tidy
find . -name "*.cpp" -o -name "*.h" | xargs clang-tidy -p compile_commands.json

# Run clang-static-analyzer
clang-static-analyzer --analyze -Xanalyzer -std=c++20 src/**/*.cpp
```

## Directory Structure

```text
CriptoGualet/
├── out/build/
│   ├── windows-msvc/                        # MSVC builds
│   ├── windows-clang/                       # ClangCL builds
│   ├── linux-dbg/                           # Linux debug builds
│   └── linux-rel/                           # Linux release builds
├── compile_commands.json                    # Generated compile commands
├── CMakePresets.json                        # All available presets
├── .clangd                                  # clangd configuration
└── .vscode/                                 # VS Code configuration
```

## Getting Help

### Useful Commands

```powershell
# List all available presets
cmake --list-presets

# Show preset details
cmake --preset windows-clang --trace

# Run specific tests
ctest --preset windows-clang --output-on-failure

# Clean build directory
.\scripts\build.ps1 -Preset windows-clang -Clean -ConfigureOnly
```

### Debugging ASan Issues

```powershell
# Generate ASan report with stack traces
$env:ASAN_OPTIONS = "symbolize=1:print_stats=1"

# Run under the Visual Studio debugger
devenv /debugexe .\out\build\windows-clang\bin\Debug\CriptoGualetQt.exe
```

## Summary

✅ **Enhanced Security**: ASan detects memory vulnerabilities  
✅ **Better Performance**: LTO optimizes across translation units  
✅ **Independent Operation**: Clang and MSVC work separately  
✅ **IDE Integration**: Full Visual Studio and VS Code support  
✅ **Cross-Platform Ready**: Foundation for Linux/macOS builds  

**Recommended Workflow**:

1. **Development**: MSVC for faster iteration
2. **Security Testing**: ClangCL + ASan for memory safety
3. **Release**: ClangCL + LTO for maximum performance
4. **CI/CD**: Test both compilers for compatibility

---

**For issues or questions**, check:

- CMakePresets.json for available configurations
- CMakeLists.txt for compiler-specific options
- .clangd for language server configuration
