# CriptoGualet - Cursor & Visual Studio 2022 IDE Setup

Quick reference guide for developing CriptoGualet in both Cursor IDE and Visual Studio 2022.

---

## Quick Start

### 1. Install Required Extension
**CodeLLDB** (Required for Cursor debugging)
- Press `Ctrl+Shift+X`
- Search: **CodeLLDB**
- Install by **vadimcn** (vadimcn.vscode-lldb)

### 2. First Build
```powershell
# Configure (works in both IDEs)
cmake --preset win-vs2022-x64-debug

# Build
cmake --build out/build/win-vs2022-x64-debug --config Debug
```

### 3. Start Debugging
- Press `F5`
- Select: **Debug CriptoGualet Qt (Visual Studio Build)**

---

## Debugging

### Quick Reference
| Key | Action |
|-----|--------|
| `F5` | Start/Continue |
| `F9` | Toggle Breakpoint |
| `F10` | Step Over |
| `F11` | Step Into |
| `Shift+F11` | Step Out |
| `Shift+F5` | Stop |

### Debug Configurations
1. **Debug CriptoGualet Qt (Visual Studio Build)** ⭐ Recommended
   - Auto-builds before debugging
   - Executable: `out/build/win-vs2022-x64-debug/bin/Debug/CriptoGualetQt.exe`

2. **Debug CriptoGualet Qt (Ninja Build)**
   - Advanced: Requires Ninja setup
   - Executable: `out/build/win-ninja-x64-debug/bin/CriptoGualetQt.exe`

3. **Debug CriptoGualet Qt (No Build)**
   - Skips build step for quick re-runs

### Test Debugging
- **Debug BIP39 Tests**
- **Debug Repository Integration Tests**
- **Debug Auth Database Tests**
- **Debug Current Test File** - Auto-detects test from open file

---

## Building

### CMake Presets

**Recommended (Both IDEs):**
- `win-vs2022-x64-debug` - Debug build
- `win-vs2022-x64-release` - Release build

**Advanced (Ninja):**
- `win-ninja-x64-debug` - Requires Developer Command Prompt
- `win-ninja-x64-release` - Advanced users only

### Build Tasks
Access via `Ctrl+Shift+P` → "Tasks: Run Task":
- **CMake: build** - Build with Visual Studio generator
- **CMake: build (Ninja)** - Advanced Ninja build
- **CMake: configure (Visual Studio)** - Configure VS generator
- **CMake: configure (Ninja)** - Advanced configuration

### Output Locations
```
out/build/win-vs2022-x64-debug/
├── bin/Debug/
│   ├── CriptoGualetQt.exe          # Main app
│   ├── test_*.exe                  # Tests
│   └── *.dll                       # Auto-deployed
```

---

## IntelliSense (clangd)

### Setup
1. **Install clangd extension**:
   - Search: **clangd**
   - Install by **llvm-vs-code-extensions**

2. **Generate compile_commands.json**:
   - Automatically created during CMake configure
   - Located in root after build

3. **Restart if needed**:
   - `Ctrl+Shift+P` → "clangd: Restart language server"

### Features
- Code completion
- Go to definition (`F12`)
- Find references (`Shift+F12`)
- Rename symbol (`F2`)
- Format on save (`Shift+Alt+F`)

---

## IDE Usage

### Cursor IDE
1. Open project folder
2. Press `F5` to debug
3. Select debug configuration
4. Use CodeLLDB debugger

### Visual Studio 2022
1. Open folder in VS 2022
2. Select `win-vs2022-x64-debug` from preset dropdown
3. Press `F5` to build and debug
4. Full native integration

**Both IDEs share the same build directory: `out/build/win-vs2022-x64-debug/`**

---

## Common Tasks

### Daily Development
```
1. Open project (Cursor or VS 2022)
2. Make changes
3. Press F5 (builds + debugs)
4. Set breakpoints
5. Step through with F10/F11
```

### Running Tests
```powershell
# All tests
Ctrl+Shift+P → Tasks: Run Task → Run All Tests

# Individual test
./out/build/win-vs2022-x64-debug/bin/Debug/test_bip39.exe

# Debug test
F5 → Select test configuration
```

### Adding New Files
```
1. Add to CMakeLists.txt
2. Run: CMake: configure (Visual Studio)
3. Build: Ctrl+Shift+B
4. Restart clangd if needed
```

---

## Troubleshooting

### Debugging

**"Debugger not found"**
→ Install CodeLLDB extension

**"Executable not found"**
→ Run `CMake: build` task first

**"Qt DLLs not found"**
→ Rebuild (DLLs auto-copy)
→ Check Qt PATH: `C:\Program Files\Qt\6.9.1\msvc2022_64\bin`

### IntelliSense

**No code completion**
→ Check `compile_commands.json` exists in root
→ Restart clangd: `Ctrl+Shift+P` → "clangd: Restart"

**Many errors**
→ Build project once
→ Verify C++20 standard

### Build

**CMake not found**
→ Verify VS 2022 installed
→ Path: `C:\Program Files\Microsoft Visual Studio\2022\Community\...\cmake.exe`

**vcpkg packages missing**
→ Run: `vcpkg install` in root

---

## Configuration Files

| File | Purpose |
|------|---------|
| `launch.json` | Debug configurations |
| `tasks.json` | Build tasks |
| `settings.json` | clangd, CMake, editor settings |
| `extensions.json` | Recommended extensions |
| `../CMakePresets.json` | Build presets |
| `../.clangd` | Compiler flags |

---

## Key Paths

| Component | Path |
|-----------|------|
| Executable | `out/build/win-vs2022-x64-debug/bin/Debug/CriptoGualetQt.exe` |
| Tests | `out/build/win-vs2022-x64-debug/bin/Debug/test_*.exe` |
| CMake | `C:\Program Files\Microsoft Visual Studio\2022\Community\...\cmake.exe` |
| Qt DLLs | `C:\Program Files\Qt\6.9.1\msvc2022_64\bin` |

---

## Summary

✅ **Unified build system** for Cursor and Visual Studio 2022
✅ **One-click debug** with F5
✅ **Auto DLL deployment**
✅ **Intelligent IntelliSense** with clangd
✅ **Full debugging** with CodeLLDB
✅ **Consistent builds** across IDEs

**Just press F5 to start!**

---

## Additional Resources
- Project docs: `.claude/CLAUDE.md`
- CMake presets: `../CMakePresets.json`
- Formatting: `../.clang-format`
