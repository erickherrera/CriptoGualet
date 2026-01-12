# CriptoGualet - Cursor IDE Development Setup Guide

## Overview

Your CriptoGualet project is now fully configured for optimal development in Cursor IDE with:
- ✅ clangd language server with background indexing
- ✅ CodeLLDB native debugger
- ✅ CMake Tools integration
- ✅ Multiple build configurations (Ninja + Visual Studio)
- ✅ Comprehensive test debugging
- ✅ Performance optimizations
- ✅ Custom keyboard shortcuts

---

## What Was Configured

### 1. Core Extensions

**Required Extensions** (install via `Ctrl+Shift+P` → "Extensions: Show Recommended Extensions"):
- **llvm-vs-code-extensions.vscode-clangd** - C++ IntelliSense
- **vadimcn.vscode-lldb** - Debugging
- **ms-vscode.cmake-tools** - CMake support
- **twxs.cmake** - CMake syntax
- **matepek.vscode-catch2-test-adapter** - Test explorer

**Optional but Recommended**:
- **eamodio.gitlens** - Advanced Git integration
- **usernamehw.errorlens** - Inline error display
- **streetsidesoftware.code-spell-checker** - Spell checking

### 2. Configuration Files Created/Updated

#### `.vscode/settings.json`
- clangd with clang-tidy integration
- Automatic compile_commands.json copying
- File watcher exclusions for performance
- CMake Tools preset integration
- Cursor AI settings

#### `.vscode/launch.json`
- ✅ Fixed syntax error (removed duplicate closing brace)
- Debug configurations for:
  - CriptoGualet Qt (Visual Studio build)
  - CriptoGualet Qt (Ninja build)
  - BIP39 tests
  - Repository integration tests
  - Auth/database tests
  - Dynamic test file debugging

#### `.vscode/tasks.json`
- CMake build tasks (both generators)
- CMake configure tasks
- "Run All Tests" task
- Individual test runners
- Clean build directory task

#### `.vscode/extensions.json` (NEW)
- Recommended extensions list
- Unwanted extensions (ms-vscode.cpptools to avoid conflicts)

#### `.vscode/KEYBINDINGS_TO_ADD.json` (NEW)
- Suggested keybindings to copy to your user settings
- `F7` - Build
- `Ctrl+F7` - Configure CMake
- `Shift+F7` - Clean build
- `Ctrl+Shift+T` - Run all tests
- **Note**: Must be added to user keybindings (workspace keybindings not supported)

#### `.vscode/README.md` (NEW)
- Complete setup documentation
- Troubleshooting guide
- Common workflows

#### `.gitignore` (UPDATED)
- ✅ Now allows `.vscode/` to be versioned
- ✅ Allows `compile_commands.json` in root
- Still ignores build artifacts

---

## Quick Start Guide

### First Time Setup

1. **Install Extensions**:
   ```
   Ctrl+Shift+P → Extensions: Show Recommended Extensions → Install All
   ```

2. **Configure CMake** (choose one):

   **Option A: Ninja (Recommended for development)**
   ```
   Ctrl+F7
   ```
   or
   ```
   Terminal → Run Task → CMake: configure (Ninja)
   ```

   **Option B: Visual Studio (for IDE integration)**
   ```
   Terminal → Run Task → CMake: configure (Visual Studio)
   ```

3. **Build the Project**:
   ```
   F7 (or Ctrl+Shift+B)
   ```

4. **Run/Debug**:
   ```
   F5 → Select "Debug CriptoGualet Qt (Ninja Build)"
   ```

### Daily Workflow

```bash
# 1. Pull latest changes
git pull

# 2. Configure if CMakeLists.txt changed
Ctrl+F7

# 3. Build
F7

# 4. Run tests
Ctrl+Shift+T

# 5. Debug if needed
F5 → Select configuration
```

---

## Build Configurations

### Ninja Build (Recommended)
**Pros**:
- ⚡ Faster incremental builds
- 🎯 Better clangd integration
- 📋 Single compile_commands.json location
- 🔧 Simpler debugging

**Configure**:
```bash
cmake --preset win-ninja-x64-debug
```

**Build**:
```bash
cmake --build out/build/win-ninja-x64-debug
```

**Output**:
```
out/build/win-ninja-x64-debug/bin/CriptoGualetQt.exe
out/build/win-ninja-x64-debug/bin/test_*.exe
```

### Visual Studio Build
**Pros**:
- 🏗️ Visual Studio integration
- 📊 Multi-config (Debug/Release in one build)
- 🔨 MSBuild tooling

**Configure**:
```bash
cmake --preset win-vs2022-x64-debug
```

**Build**:
```bash
cmake --build build-simple --config Debug
```

**Output**:
```
build-simple/bin/Debug/CriptoGualetQt.exe
```

---

## Debugging Guide

### Debug Configurations Available

1. **Debug CriptoGualet Qt (Visual Studio Build)**
   - Uses `build-simple/bin/Debug/CriptoGualetQt.exe`
   - Automatically builds before debugging
   - Qt DLLs in PATH

2. **Debug CriptoGualet Qt (Ninja Build)** ⭐ Recommended
   - Uses `out/build/clangd-ninja/bin/CriptoGualetQt.exe`
   - Faster build times
   - Better clangd integration

3. **Debug CriptoGualet Qt (No Build)**
   - Skips build step
   - Useful for quick re-runs

4. **Debug BIP39 Tests**
   - Test seed phrase generation
   - Runs `test_bip39.exe`

5. **Debug Repository Integration Tests**
   - Test database operations
   - Runs `test_repository_integration.exe`

6. **Debug Auth Database Tests**
   - Test authentication flow
   - Runs `test_auth_database_integration.exe`

7. **Debug Current Test File**
   - Opens whatever test file you're viewing
   - Auto-detects executable name

### Debugging Steps

1. **Set Breakpoints**: Click in the gutter (left of line numbers) or press `F9`

2. **Start Debugging**: Press `F5` or go to Run → Start Debugging

3. **Debug Controls**:
   - `F5` - Continue
   - `F10` - Step Over
   - `F11` - Step Into
   - `Shift+F11` - Step Out
   - `Ctrl+Shift+F5` - Restart
   - `Shift+F5` - Stop

4. **Watch Variables**:
   - Hover over variables to see values
   - Add to Watch panel for persistence
   - Inspect in Debug Console

---

## Testing Workflow

### Run All Tests
```
Ctrl+Shift+T
```
or
```
Terminal → Run Task → Run All Tests
```

This runs all `test_*.exe` files in the build directory.

### Run Specific Tests

Via Tasks:
```
Terminal → Run Task → Run BIP39 Tests
Terminal → Run Task → Run Repository Integration Tests
```

Via Command Line:
```powershell
./out/build/win-ninja-x64-debug/bin/test_bip39.exe
./out/build/win-ninja-x64-debug/bin/test_repository_integration.exe
./out/build/win-ninja-x64-debug/bin/test_wallet_repository.exe
```

### Debug Tests

1. Open a test file (e.g., `Tests/test_bip39.cpp`)
2. Set breakpoints
3. Press `F5`
4. Select the corresponding debug configuration
5. Step through test execution

---

## Code Navigation & IntelliSense

### clangd Features

**Go to Definition**: `F12`
- Jump to where a symbol is defined

**Find All References**: `Shift+F12`
- See everywhere a symbol is used

**Rename Symbol**: `F2`
- Safely rename across the codebase

**Format Document**: `Shift+Alt+F`
- Uses `.clang-format` rules

**Code Actions**: `Ctrl+.`
- Fix includes, generate code, etc.

**Parameter Hints**: `Ctrl+Shift+Space`
- Show function signatures

### IntelliSense Sources

clangd uses `compile_commands.json` which contains:
- Compiler flags
- Include paths (Qt, vcpkg, project headers)
- Defines (Windows, Qt macros)
- C++20 standard

This is automatically generated by CMake and copied to the root by CMake Tools.

---

## Performance Optimization

### File Watcher Exclusions

Large directories are excluded from file watching for better performance:
- `out/` - Build outputs
- `build*/` - Build directories
- `vcpkg_installed/` - Dependencies
- `.vs/` - Visual Studio files

### Search Exclusions

Same directories excluded from search for faster Ctrl+Shift+F.

### clangd Background Indexing

- **Enabled**: clangd indexes your codebase in the background
- **First indexing**: May take 1-2 minutes
- **Subsequent loads**: Much faster
- **Status**: Check bottom-right status bar

---

## Keyboard Shortcuts Reference

### Building (Optional - See Setup Below)
| Shortcut | Action |
|----------|--------|
| `F7` | Build project |
| `Ctrl+F7` | Configure CMake (Ninja) |
| `Shift+F7` | Clean build directory |
| `Ctrl+Shift+B` | Build task menu |

**Note**: To enable these shortcuts, copy keybindings from [.vscode/KEYBINDINGS_TO_ADD.json](.vscode/KEYBINDINGS_TO_ADD.json) to your user keybindings file (`Ctrl+Shift+P` → "Preferences: Open Keyboard Shortcuts (JSON)").

### Testing (Optional - See Setup Below)
| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+T` | Run all tests |

### Debugging
| Shortcut | Action |
|----------|--------|
| `F5` | Start debugging / Continue |
| `F9` | Toggle breakpoint |
| `F10` | Step over |
| `F11` | Step into |
| `Shift+F11` | Step out |
| `Shift+F5` | Stop debugging |

### Navigation
| Shortcut | Action |
|----------|--------|
| `F12` | Go to definition |
| `Shift+F12` | Find all references |
| `F2` | Rename symbol |
| `Ctrl+Shift+O` | Go to symbol in file |
| `Ctrl+T` | Go to symbol in workspace |

### Editing
| Shortcut | Action |
|----------|--------|
| `Shift+Alt+F` | Format document |
| `Ctrl+.` | Quick fix / Code actions |
| `Ctrl+Space` | Trigger suggestions |
| `Ctrl+Shift+Space` | Parameter hints |

---

## Troubleshooting

### 1. clangd Not Providing IntelliSense

**Check compile_commands.json exists**:
```bash
ls compile_commands.json
```

**If missing**:
1. Configure with Ninja: `Ctrl+F7`
2. Build once: `F7`
3. Check Settings: CMake should copy it automatically

**Restart clangd**:
```
Ctrl+Shift+P → clangd: Restart language server
```

**Check clangd output**:
```
View → Output → Select "clangd" from dropdown
```

### 2. Duplicate IntelliSense Suggestions

**Ensure C/C++ extension is disabled**:
1. Open Settings: `Ctrl+,`
2. Search: `C_Cpp.intelliSenseEngine`
3. Should be: `disabled`

**Or uninstall** ms-vscode.cpptools if installed.

### 3. Build Failures

**Clean and rebuild**:
```
Shift+F7 (Clean)
Ctrl+F7 (Configure)
F7 (Build)
```

**Check CMake output**:
```
View → Output → Select "CMake" from dropdown
```

**Common issues**:
- vcpkg not found: Check VCPKG_ROOT environment variable
- Qt not found: Verify Qt installation path in CMakePresets.json
- Compiler errors: Check C++ standard is set to C++20

### 4. Debugger Won't Start

**Qt DLLs missing**:
```powershell
# Add Qt to PATH temporarily
$env:PATH = "C:\Program Files\Qt\6.9.1\msvc2022_64\bin;$env:PATH"

# Or run from PowerShell with launch.json PATH settings
```

**Executable not found**:
- Build first: `F7`
- Check path in launch.json matches your build output

**CodeLLDB not installed**:
```
Ctrl+Shift+X → Search "CodeLLDB" → Install
```

### 5. Tests Not Running

**Build tests first**:
```
F7
```

**Check test executables exist**:
```powershell
ls out/build/win-ninja-x64-debug/bin/test_*.exe
```

**Run manually to see errors**:
```powershell
./out/build/win-ninja-x64-debug/bin/test_bip39.exe
```

### 6. Slow Performance

**Indexing in progress**:
- Wait for clangd to finish (check status bar)
- First index can take 1-2 minutes

**Too many files open**:
- Close unused tabs: `Ctrl+K W`

**File watchers**:
- Already excluded large directories
- Check: Settings → `files.watcherExclude`

---

## Best Practices

### 1. Use Ninja for Daily Development
- Faster builds
- Better clangd experience
- Simpler configuration

### 2. Use Visual Studio for Release Builds
- Multi-config support
- MSBuild integration
- Better optimization tools

### 3. Run Tests Frequently
- `Ctrl+Shift+T` after changes
- Debug tests when they fail (`F5` → test config)

### 4. Keep compile_commands.json Updated
- Reconfigure when adding files: `Ctrl+F7`
- CMake Tools copies it automatically

### 5. Use clangd Features
- Fix includes with `Ctrl+.`
- Format on save is enabled
- Rename safely with `F2`

### 6. Version Control .vscode
- Now tracked in Git
- Share configuration with team
- Update `.vscode/README.md` for team changes

---

## Additional Configuration Options

### CMake Presets Customization

Edit `CMakePresets.json` to:
- Change Qt path
- Modify vcpkg location
- Add custom cache variables
- Change compiler flags

### clangd Customization

Edit `.clangd` to:
- Adjust warning levels
- Change include paths
- Modify compilation flags
- Configure diagnostics

### Custom Tasks

Add to `.vscode/tasks.json`:
```json
{
  "label": "Your Custom Task",
  "type": "shell",
  "command": "your-command",
  "group": "build"
}
```

### Additional Launch Configurations

Add to `.vscode/launch.json`:
```json
{
  "name": "Your Config",
  "type": "lldb",
  "request": "launch",
  "program": "${workspaceFolder}/path/to/exe",
  "args": [],
  "cwd": "${workspaceFolder}"
}
```

---

## CI/CD Integration

The same CMake presets work in CI:

```yaml
# GitHub Actions example
- name: Configure
  run: cmake --preset win-ninja-x64-release

- name: Build
  run: cmake --build --preset win-ninja-build-release

- name: Test
  run: ctest --preset win-ninja-test-release
```

---

## Team Collaboration

### Sharing Configuration

Now that `.vscode/` is tracked:
1. Team members get same setup
2. Update `.vscode/README.md` for changes
3. Extensions are auto-recommended

### Local Overrides

Create `.vscode/settings.local.json` for personal settings (this is in .gitignore):
```json
{
  "clangd.path": "c:/my-custom-path/clangd.exe"
}
```

### Extension Recommendations

Team members will see:
```
This workspace has extension recommendations.
[Install All] [Show Recommendations]
```

---

## Resources

### Documentation
- [Cursor IDE](https://cursor.sh/docs)
- [CMake Tools](https://vector-of-bool.github.io/docs/vscode-cmake-tools/)
- [clangd](https://clangd.llvm.org/)
- [CodeLLDB](https://github.com/vadimcn/vscode-lldb)
- [Qt Documentation](https://doc.qt.io/)

### Project Documentation
- [.vscode/README.md](.vscode/README.md) - Detailed .vscode configuration
- [.claude/CLAUDE.md](.claude/CLAUDE.md) - Project architecture and structure
- [CMakePresets.json](CMakePresets.json) - Build configuration

### Getting Help
- Check `.vscode/README.md` for common issues
- Review CMake output for build errors
- Check clangd output for IntelliSense issues
- Consult extension documentation

---

## Summary

Your CriptoGualet project is now optimized for Cursor IDE with:

✅ **Powerful IntelliSense** via clangd with background indexing
✅ **Native Debugging** with CodeLLDB for C++ and Qt
✅ **Fast Builds** using Ninja generator
✅ **Comprehensive Testing** with individual test runners
✅ **Custom Shortcuts** for common workflows
✅ **Performance Optimized** with smart file exclusions
✅ **Team Ready** with versioned configuration

**Next Steps**:
1. Install recommended extensions
2. Run `Ctrl+F7` to configure
3. Press `F7` to build
4. Press `F5` to debug

Happy coding! 🚀
