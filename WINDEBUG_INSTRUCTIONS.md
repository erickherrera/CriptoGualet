# WinDbg Debugging Script for Transaction Parsing Crash

## Prerequisites

1. **Install WinDbg Preview** (recommended) or WinDbg from Windows SDK:
   ```powershell
   # Option 1: Microsoft Store (WinDbg Preview)
   # Search for "WinDbg Preview" in Microsoft Store
   
   # Option 2: Windows SDK
   winget install Microsoft.WindowsSDK
   
   # Option 3: Direct download
   # https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/
   ```

2. **Ensure PDB symbols are available** (they should be in the build directory)

## Debugging Steps

### Step 1: Launch WinDbg

Open PowerShell as Administrator and run:

```powershell
# Navigate to the fuzzer directory
cd "C:\Users\erick\source\repos\erickherrera\CriptoGualet\out\build\win-vs2022-clangcl-release\fuzzers\Release"

# Launch WinDbg with the fuzzer and crash input
& "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe" `
    -g `
    -G `
    -c ".sympath+ C:\Users\erick\source\repos\erickherrera\CriptoGualet\out\build\win-vs2022-clangcl-release\fuzzers\Release; .reload /f; bp fuzz_transaction_parsing!LLVMFuzzerTestOneInput; g" `
    "fuzz_transaction_parsing.exe" `
    "crash-c9d654284e783b9296ced072bb534f4e9dd3c1e3"
```

### Step 2: WinDbg Commands to Run

Once WinDbg opens, execute these commands in order:

```windbg
# 1. Set symbol path
.sympath+ C:\Users\erick\source\repos\erickherrera\CriptoGualet\out\build\win-vs2022-clangcl-release\fuzzers\Release
.sympath+ C:\Users\erick\source\repos\erickherrera\CriptoGualet\out\build\win-vs2022-clangcl-release\backend\core\Release
.sympath+ C:\Users\erick\source\repos\erickherrera\CriptoGualet\out\build\win-vs2022-clangcl-release\backend\utils\Release

# 2. Reload symbols
.reload /f

# 3. Set breakpoint on the fuzzer entry point
bp fuzz_transaction_parsing!LLVMFuzzerTestOneInput

# 4. Go (run until breakpoint)
g

# 5. When breakpoint hits, step through the code
# Use these commands to trace:

# Step over (next line)
p

# Step into (enter function)
t

# Step out (return from function)
pt

# View current line
.

# View call stack
k

# View full call stack with parameters
kb

# View registers
r

# View memory at address
# Example: dd 0x11ab22aa017c L10

# View source code (if available)
ls

# View local variables
 dv

# Set breakpoint on specific function
bp RLPEncoder!RLP::Encoder::EncodeString
bp RLPEncoder!RLP::Encoder::EncodeBytes
bp RLPEncoder!RLP::Encoder::EncodeList
bp Crypto!CreateSegWitSigHash

# Continue execution
 g
```

### Step 3: Analyze the Crash

When the crash occurs, run these commands:

```windbg
# Get detailed exception information
.exr -1

# Get stack trace
k

# Get stack trace with source lines
kn

# Get all stacks for all threads
~* k

# View memory around crash address
# Replace 0xXXXXXXXX with actual address from crash report
dd 0x11ab22aa017c L20
db 0x11ab22aa017c L40

# Check heap allocation
dh 0x11ab22aa017c

# View loaded modules
lm

# View specific module details
lmvm fuzz_transaction_parsing

# Check for memory corruption
!heap -stat
!heap -p -a 0x11ab22aa017c

# View ASan shadow memory (if applicable)
# Shadow memory is typically at address >> 3 + shadow_offset
```

### Step 4: Identify Root Cause

Key things to look for:

1. **Exact crash location**: Which function and line?
2. **Buffer bounds**: What is the allocated size vs. accessed size?
3. **Call chain**: Which code path led to the crash?
4. **Memory state**: Is the heap corrupted?

### Step 5: Alternative: Use GFlags for Page Heap

Enable page heap debugging for more precise detection:

```powershell
# Install Debugging Tools for Windows if not already installed
# Then run:
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe" /p /enable fuzz_transaction_parsing.exe /full

# Run the fuzzer normally - it will break at exact overflow point
./fuzz_transaction_parsing.exe crash-c9d654284e783b9296ced072bb534f4e9dd3c1e3

# Disable when done
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe" /p /disable fuzz_transaction_parsing.exe
```

## Expected Findings

Based on the crash report:
- **Crash Type**: Heap-buffer-overflow (READ)
- **Access Size**: 25 bytes
- **Buffer Size**: 21 bytes (7 bytes overflow)
- **Offset**: 0x140007aba in fuzz_transaction_parsing.exe

### Likely Culprits to Check:

1. **RLP::Encoder::EncodeString** - String encoding with null bytes
2. **RLP::Encoder::EncodeBytes** - Byte array encoding
3. **RLP::Encoder::EncodeList** - List encoding with nested items
4. **Crypto::CreateSegWitSigHash** - Transaction hash creation
5. **Hex string conversion** - BytesToHex or HexToBytes

## Quick Analysis Script

Save this as `analyze_crash.wds` and run with: `windbg -c "$<analyze_crash.wds" fuzz_transaction_parsing.exe crash-c9d654284e783b9296ced072bb534f4e9dd3c1e3`

```windbg
# analyze_crash.wds - WinDbg script for crash analysis

# Setup
.logopen C:\Users\erick\source\repos\erickherrera\CriptoGualet\crash_analysis.txt
.sympath+ C:\Users\erick\source\repos\erickherrera\CriptoGualet\out\build\win-vs2022-clangcl-release\fuzzers\Release
.reload /f

# Break on entry
bp fuzz_transaction_parsing!LLVMFuzzerTestOneInput

# Set memory access breakpoint (if you know the address)
# ba r4 0x11ab22aa017c

# Run
g

# When crash happens:
.exr -1
k L30
lmvm fuzz_transaction_parsing

# View crash input
db poi(@rsp) L40

# Close log
.logclose
q
```

## Crash Input Analysis

The crash input (21 bytes):
```
74 fe 0f 00 00 00 00 00 00 74 74 74 ad ad ad ad ad ad ad e4 31
```

Decoded:
- Starts with `0x74` (ASCII 't')
- Contains `0xfe` (potential length marker or error code)
- Has `0x0f` (15 in decimal - could be a length)
- Contains multiple `0x74` bytes (ASCII 't' repeated)
- Ends with `0xad` bytes (padding or data)
- Final bytes `0xe4 0x31`

This pattern suggests:
1. Could be triggering RLP encoding with malformed length prefix
2. Might be causing buffer miscalculation in list encoding
3. Could be hitting edge case in string encoding with specific byte patterns

## Next Steps After Debugging

1. **Document exact crash location** (function + line number)
2. **Identify root cause** (off-by-one, missing bounds check, etc.)
3. **Create minimal reproduction case** (smaller input triggering same crash)
4. **Implement fix** with proper bounds checking
5. **Verify fix** with fuzzer and regression test

## Additional Resources

- WinDbg documentation: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/
- ASan on Windows: https://clang.llvm.org/docs/AddressSanitizer.html#windows
- libFuzzer docs: https://llvm.org/docs/LibFuzzer.html

---

**Note**: If you cannot install WinDbg, you can also use:
1. **Visual Studio Debugger**: Attach to process and debug
2. **LLDB** (if available): `lldb ./fuzz_transaction_parsing.exe -- crash-c9d654284e783b9296ced072bb534f4e9dd3c1e3`
3. **GDB** (if available): `gdb ./fuzz_transaction_parsing.exe`
