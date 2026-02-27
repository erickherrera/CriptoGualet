# CI/CD Pipeline Documentation

## Overview

This project uses **GitHub Actions** for continuous integration and deployment. The CI pipeline ensures that only high-quality code reaches the `master` branch.

## Pipeline Jobs

The CI pipeline consists of 4 jobs that run on every pull request and push to `master`:

### 1. Code Formatting (`format-check`)
- **Tool**: `clang-format` (LLVM 17.0.6)
- **Config**: `.clang-format` (Google style, C++20)
- **Checks**: All `.cpp`, `.h`, and `.hpp` files in `backend/`, `frontend/`, `src/`, and `Tests/`
- **Fail Condition**: Any file that doesn't match the formatting rules
- **Auto-fix**: Run `clang-format -i <file>` locally to fix formatting

### 2. Static Analysis (`static-analysis`)
- **Tool**: `clang-tidy` (LLVM 17.0.6)
- **Config**: `.clang-tidy`
- **Checks**:
  - `bugprone-*` - Bug-prone patterns
  - `clang-analyzer-*` - Clang static analyzer
  - `cert-*` - CERT coding standards
  - `cppcoreguidelines-*` - C++ Core Guidelines
  - `hicpp-*` - High Integrity C++
  - `misc-*` - Miscellaneous checks
  - `modernize-*` - Modernization suggestions
  - `performance-*` - Performance optimizations
  - `portability-*` - Portability issues
  - `readability-*` - Readability improvements
  - `security-*` - Security vulnerabilities
- **Warnings as Errors**: `bugprone-*`, `clang-analyzer-*`, `cert-*`, `security-*`

### 3. Build & Test (`build-and-test`)
- **Platform**: Windows (x64)
- **Compiler**: MSVC (Visual Studio 2022)
- **Build Types**: Release and Debug
- **Steps**:
  1. Configure with CMake
  2. Build all targets
  3. Run all test executables from `Tests/`
- **Artifacts**: Release binaries uploaded for 7 days

### 4. Security Scan (`security-scan`)
- **Tool**: Trivy vulnerability scanner
- **Scans**: Filesystem for known vulnerabilities
- **Severity**: CRITICAL and HIGH only
- **Reporting**: SARIF format uploaded to GitHub Security tab

## Running Checks Locally

### Quick Check (Windows)
```batch
scripts\pre-commit-check.bat
```

### Manual Checks

#### Formatting
```bash
# Check all files
clang-format --dry-run --Werror backend/**/*.cpp frontend/**/*.cpp

# Fix a specific file
clang-format -i backend/core/src/Crypto.cpp
```

#### Building
```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

# Build
cmake --build build --config Release --parallel

# Run tests
cd build && ctest -C Release --output-on-failure
```

#### Static Analysis
```bash
# Generate compile commands
cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy backend/core/src/Crypto.cpp -- -std=c++20
```

## Branch Protection Rules

The `master` branch is protected with the following rules:

1. **Require pull request reviews before merging**
   - At least 1 approving review required

2. **Require status checks to pass**
   - `format-check` - Code formatting
   - `static-analysis` - Static analysis
   - `build-and-test (Release)` - Release build and tests
   - `build-and-test (Debug)` - Debug build and tests

3. **Require branches to be up to date before merging**
   - Ensures PRs are tested against latest master

4. **Restrict pushes that create files larger than 100MB**
   - Prevents accidental large file commits

## CI Configuration Files

- `.github/workflows/ci.yml` - Main CI workflow
- `.clang-format` - Code formatting rules
- `.clang-tidy` - Static analysis configuration
- `scripts/pre-commit-check.bat` - Local pre-commit check script

## Troubleshooting

### Build Failures

**Issue**: CMake can't find vcpkg
**Solution**: Ensure vcpkg is initialized:
```bash
git submodule update --init --recursive
```

**Issue**: Missing dependencies
**Solution**: Install via vcpkg:
```bash
vcpkg install --triplet x64-windows
```

### Formatting Failures

**Issue**: CI reports formatting errors
**Solution**: 
1. Install LLVM: `choco install llvm`
2. Run: `clang-format -i <file>` for each failing file
3. Or use the pre-commit script: `scripts\pre-commit-check.bat`

### Test Failures

**Issue**: Tests pass locally but fail in CI
**Solution**: 
- Check for hardcoded paths (use relative paths)
- Ensure all test dependencies are in vcpkg.json
- Verify test data files are committed to git

### Static Analysis Warnings

**Issue**: clang-tidy reports issues
**Solution**:
- Review the specific warning
- Fix legitimate issues
- Suppress false positives with `// NOLINT` comments

## Future Enhancements

Planned CI/CD improvements:

- [ ] Linux builds (Ubuntu, Fedora)
- [ ] macOS builds
- [ ] Code coverage reporting (codecov.io)
- [ ] Automated release creation on tag push
- [ ] Docker image builds
- [ ] Performance regression testing
- [ ] Memory leak detection (Valgrind/AddressSanitizer)

## Support

For CI/CD issues:
1. Check the [Actions tab](https://github.com/erickherrera/CriptoGualet/actions) for detailed logs
2. Review this documentation
3. Open an issue with the `ci-cd` label
