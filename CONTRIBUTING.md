# Contributing to CriptoGualet

Thank you for your interest in contributing to CriptoGualet! This document outlines the process for contributing code and the standards we expect.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Code Standards](#code-standards)
- [Pull Request Process](#pull-request-process)
- [CI/CD Requirements](#cicd-requirements)
- [Troubleshooting](#troubleshooting)

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally
3. **Create a branch** for your feature or bug fix
4. **Make your changes** following our code standards
5. **Run local checks** before pushing
6. **Submit a pull request** to the `master` branch

## Development Setup

### Prerequisites

- Windows 10/11 (x64)
- Visual Studio 2022 (with C++ workload)
- Git
- CMake 3.20+
- vcpkg

### Initial Setup

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/CriptoGualet.git
cd CriptoGualet

# Initialize submodules (vcpkg)
git submodule update --init --recursive

# Install dependencies
vcpkg install --triplet x64-windows

# Build the project
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --config Release --parallel
```

## Code Standards

### C++ Style Guide

We follow the **Google C++ Style Guide** with some modifications:

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Maximum 100 characters
- **Naming**:
  - Classes: `CamelCase` (e.g., `WalletManager`)
  - Functions: `CamelCase` (e.g., `GetBalance()`)
  - Variables: `camelBack` (e.g., `walletAddress`)
  - Constants: `kCamelCase` (e.g., `kMaxRetries`)
  - Namespaces: `lower_case`

### Code Formatting

All code must be formatted with `clang-format`:

```bash
# Check formatting
clang-format --dry-run --Werror backend/core/src/Crypto.cpp

# Fix formatting
clang-format -i backend/core/src/Crypto.cpp
```

**Configuration**: `.clang-format` (Google style, C++20)

### Static Analysis

All code must pass `clang-tidy` checks:

```bash
# Run clang-tidy
clang-tidy backend/core/src/Crypto.cpp -- -std=c++20
```

**Configuration**: `.clang-tidy`

**Key Checks**:
- Bug-prone patterns
- Security vulnerabilities
- CERT coding standards
- C++ Core Guidelines
- Performance optimizations

## Pull Request Process

### 1. Create a Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/bug-description
```

**Branch naming conventions**:
- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation updates
- `refactor/description` - Code refactoring

### 2. Make Changes

- Write clear, concise commit messages
- Keep commits focused and atomic
- Add tests for new functionality
- Update documentation if needed

### 3. Run Local Checks

**Before pushing**, run the pre-commit check script:

```batch
scripts\pre-commit-check.bat
```

This will verify:
- ✅ Code formatting
- ✅ Project builds successfully
- ✅ All tests pass
- ✅ Static analysis (optional)

### 4. Push and Create PR

```bash
git push origin feature/your-feature-name
```

Then create a pull request on GitHub targeting the `master` branch.

### 5. PR Requirements

Your PR must:
- Pass all CI checks (see below)
- Have at least 1 approving review
- Be up to date with `master`
- Include a clear description of changes

## CI/CD Requirements

All pull requests and direct pushes to `master` must pass these automated checks:

### Required Checks

| Check | Tool | Description |
|-------|------|-------------|
| **Code Formatting** | `clang-format` | Ensures consistent code style |
| **Static Analysis** | `clang-tidy` | Finds bugs and security issues |
| **Build (Release)** | MSVC | Compiles Release configuration |
| **Build (Debug)** | MSVC | Compiles Debug configuration |
| **Tests** | Custom | Runs all test suites |

### Check Details

#### 1. Code Formatting
- Runs on: Every PR and push to `master`
- Fails if: Any file doesn't match `.clang-format`
- Fix: Run `clang-format -i <file>` on failing files

#### 2. Static Analysis
- Runs on: Every PR and push to `master`
- Fails if: Critical security or bug warnings found
- Tools: `clang-tidy` with comprehensive checks

#### 3. Build & Test
- Runs on: Every PR and push to `master`
- Platforms: Windows x64
- Configurations: Release and Debug
- Fails if: Build errors or test failures

### CI Status

Check the **Actions** tab in GitHub to see:
- Current build status
- Detailed logs for failures
- Build artifacts (Release binaries)

## Troubleshooting

### CI Failures

#### Formatting Check Failed

```bash
# Install LLVM if not already installed
choco install llvm

# Fix all formatting issues in your branch
for /r "backend" %%f in (*.cpp *.h) do clang-format -i "%%f"
for /r "frontend" %%f in (*.cpp *.h) do clang-format -i "%%f"
for /r "src" %%f in (*.cpp *.h) do clang-format -i "%%f"

# Commit the fixes
git add .
git commit -m "style: fix code formatting"
```

#### Build Failed

Common causes:
1. **Missing dependencies**: Run `vcpkg install --triplet x64-windows`
2. **CMake errors**: Delete `build/` directory and reconfigure
3. **Compiler errors**: Check error messages in CI logs

#### Tests Failed

1. Check test output in CI logs
2. Run tests locally:
   ```bash
   cd build
   ctest -C Release --output-on-failure
   ```
3. Ensure test data files are committed

#### Static Analysis Warnings

Review the specific warning:
- Fix legitimate issues
- Suppress false positives with `// NOLINT` comments
- Document why the suppression is needed

### Local Development Issues

#### CMake Can't Find vcpkg

```bash
# Set environment variable
set VCPKG_ROOT=C:\path\to\vcpkg

# Or use CMake preset
cmake --preset win-vs2022
```

#### Tests Pass Locally But Fail in CI

- Check for hardcoded Windows paths
- Ensure all dependencies are in `vcpkg.json`
- Verify test data is in git (not in `.gitignore`)

## Questions?

- **General questions**: Open a [Discussion](https://github.com/erickherrera/CriptoGualet/discussions)
- **Bug reports**: Open an [Issue](https://github.com/erickherrera/CriptoGualet/issues)
- **Security issues**: Email security@criptogualet.com (DO NOT open public issues)

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

**Thank you for contributing to CriptoGualet! 🚀**
