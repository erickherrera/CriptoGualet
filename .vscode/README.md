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
cmake --preset windows-msvc

# Build
cmake --build --preset windows-msvc
```

### 3. Start Debugging
- Press `F5`
- Select: **Debug CriptoGualet Qt (MSVC)**

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
1. **Debug CriptoGualet Qt (MSVC)** ⭐ Recommended
   - Auto-builds before debugging
   - Executable: `out/build/windows-msvc/bin/Debug/CriptoGualetQt.exe`

2. **Debug CriptoGualet Qt (Clang-CL)**
   - Auto-builds with the ClangCL toolset
   - Executable: `out/build/windows-clang/bin/Debug/CriptoGualetQt.exe`

### Test Debugging
- **Debug Current Test File (MSVC)** - Auto-detects test from open file
- **Debug Current Test File (Clang-CL)** - Auto-detects test from open file

---

## Building

### CMake Presets

**Local Windows builds:**
- `windows-msvc` - Default MSVC build
- `windows-clang` - ClangCL build

### Build Tasks
Access via `Ctrl+Shift+P` → "Tasks: Run Task":
- **CMake: build (MSVC)** - Build the MSVC preset
- **CMake: build (Clang-CL)** - Build the ClangCL preset
- **CMake: configure (MSVC)** - Configure the MSVC preset
- **CMake: configure (Clang-CL)** - Configure the ClangCL preset
- **Run Tests (MSVC)** / **Run Tests (Clang-CL)** - Run CTest

### Output Locations
```
out/build/windows-msvc/
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
2. Select `windows-msvc` (or `windows-clang`) from the preset dropdown
3. Press `F5` to build and debug
4. Full native integration

**Both IDEs share the same build directory: `out/build/windows-msvc/`**

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
Ctrl+Shift+P → Tasks: Run Task → Run Tests (MSVC)

# Individual test
./out/build/windows-msvc/bin/Debug/test_bip39.exe

# Debug test
F5 → Select test configuration
```

### Adding New Files
```
1. Add to CMakeLists.txt
2. Run: CMake: configure (MSVC)
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
| Executable | `out/build/windows-msvc/bin/Debug/CriptoGualetQt.exe` |
| Tests | `out/build/windows-msvc/bin/Debug/test_*.exe` |
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
- Project docs: [`../ProjectGuides/INDEX.md`](../ProjectGuides/INDEX.md)
- CMake presets: `../CMakePresets.json`
- Formatting: `../.clang-format`
