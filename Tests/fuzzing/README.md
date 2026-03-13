# CriptoGualet Fuzzing Suite

A comprehensive fuzzing infrastructure for the CriptoGualet cryptocurrency wallet, using libFuzzer to find security vulnerabilities and edge cases.

## Overview

This fuzzing suite targets critical components of the wallet:
- **Address Parsing**: Bech32, Bech32m, Base58Check validation
- **Transaction Processing**: RLP encoding, Bitcoin scripts, serialization
- **Cryptographic Operations**: ECDSA, hashing, key derivation, encryption
- **Database Input**: SQL injection prevention, input validation
- **API Responses**: JSON parsing, network data handling
- **Wallet Operations**: Balance calculations, fee estimation, transaction validation

## Prerequisites

- **Clang compiler** with libFuzzer support (included in Clang 6.0+)
- **CMake** 3.16 or higher
- **AddressSanitizer** and **UndefinedBehaviorSanitizer** (included with Clang)

### Platform-Specific Setup

#### Windows
1. Install LLVM/Clang from https://releases.llvm.org/
2. Ensure `clang-cl.exe` is in your PATH
3. Use the provided Clang-CL CMake presets

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y clang llvm cmake
```

#### macOS
```bash
brew install llvm
# Add to PATH: export PATH="/usr/local/opt/llvm/bin:$PATH"
```

## Building Fuzzers

### Using CMake Presets (Recommended)

#### Windows (Clang-CL)
```bash
# Configure with Clang-CL and fuzzing enabled
cmake --preset win-vs2022-clangcl-release -DENABLE_FUZZING=ON

# Build all fuzzers
cmake --build out/build/win-vs2022-clangcl-release --target fuzz_address_parsing
```

#### Linux/macOS
```bash
# Configure with Clang
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang \
    -DENABLE_FUZZING=ON

# Build fuzzers
cmake --build build --target fuzz_address_parsing --parallel
```

### Manual CMake Configuration

```bash
# Configure with fuzzing enabled
mkdir build-fuzz && cd build-fuzz
cmake .. -DENABLE_FUZZING=ON \
         -DCMAKE_CXX_COMPILER=clang++ \
         -DCMAKE_C_COMPILER=clang

# Build all fuzzers
cmake --build . --target run_fuzzers_5min
```

## Running Fuzzers

### Individual Fuzzers

```bash
# Run address parsing fuzzer for 5 minutes
./out/build/win-vs2022-clangcl-debug/fuzzers/fuzz_address_parsing \
    -max_total_time=300 \
    -max_len=4096 \
    corpus/addresses

# Run with verbose output
./fuzz_address_parsing -max_total_time=60 -print_final_stats=1
```

### All Fuzzers (CMake Targets)

```bash
# Run all fuzzers for 5 minutes each
cmake --build out/build/win-vs2022-clangcl-debug --target run_fuzzers_5min

# Run all fuzzers for 1 hour each
cmake --build out/build/win-vs2022-clangcl-debug --target run_fuzzers_1hour
```

### Minimize Corpus

After finding crashes, minimize the corpus to remove redundant inputs:

```bash
cmake --build out/build/win-vs2022-clangcl-debug --target minimize_corpus
```

## Fuzzer Targets

| Fuzzer | Target Component | Description |
|--------|-----------------|-------------|
| `fuzz_address_parsing` | Crypto::Bech32_* | Tests address encoding/decoding |
| `fuzz_transaction_parsing` | RLP::Encoder | Tests transaction serialization |
| `fuzz_crypto_operations` | Crypto::* | Tests hashing, signing, encryption |
| `fuzz_database_input` | Input validation | Tests SQL injection prevention |
| `fuzz_json_parsing` | API responses | Tests JSON validation |
| `fuzz_wallet_operations` | Wallet logic | Tests balance, fee calculations |

## Corpus Management

### Initial Seed Corpus

Place sample inputs in the `corpus/` subdirectories:
- `corpus/addresses/` - Valid and invalid addresses
- `corpus/transactions/` - Sample transaction data
- `corpus/crypto/` - Key material, signatures
- `corpus/database/` - SQL queries, user input
- `corpus/json/` - API responses

### Generating Corpus from Real Data

```bash
# Extract addresses from blockchain
./scripts/extract_corpus_from_chain.py --type=addresses --output=corpus/addresses/

# Generate synthetic test vectors
./scripts/generate_test_vectors.py --count=1000 --output=corpus/
```

## Interpreting Results

### Finding Crashes

When a fuzzer finds a crash, it saves the input to a file:
```
crash-7b3f8a2d9e1c4b5a6f0e8d7c3b2a1f4e
```

### Reproducing Crashes

```bash
# Run fuzzer with the crash input
./fuzz_address_parsing crash-7b3f8a2d9e1c4b5a6f0e8d7c3b2a1f4e
```

### Analyzing with Debugger

```bash
# Run with debugger attached
lldb ./fuzz_address_parsing -- crash-7b3f8a2d9e1c4b5a6f0e8d7c3b2a1f4e

# In lldb:
run
bt  # Get backtrace when it crashes
```

## Continuous Integration

### GitHub Actions (Implemented)

The project now includes automated fuzzing in CI/CD. Fuzzing runs on every PR and push to master:

```yaml
# See .github/workflows/ci.yml for the actual implementation
```

**CI Configuration:**
- **Platform**: Ubuntu Latest (Linux)
- **Compiler**: Clang with libFuzzer
- **Duration**: 2 minutes per fuzzer (smoke test)
- **Artifacts**: Corpus and crash files uploaded automatically
- **Trigger**: PRs, master branch pushes, or commits with `[fuzz]` tag

### Local CI Simulation

Test fuzzing locally before pushing:

```bash
# Linux/macOS
./fuzz_address_parsing -max_total_time=120 -max_len=4096

# Windows
fuzz_address_parsing.exe -max_total_time=120 -max_len=4096
```

## Performance Tuning

### Parallel Fuzzing

Run multiple fuzzer instances in parallel:

```bash
# Run 4 parallel jobs
for i in {0..3}; do
    ./fuzz_address_parsing corpus/addresses/ -max_total_time=3600 &
done
wait
```

### Memory Limits

```bash
# Limit memory to 2GB
./fuzz_address_parsing -max_len=4096 -rss_limit_mb=2048
```

### Dictionary Support

Create a dictionary for better coverage:

```bash
# Generate dictionary from existing corpus
./fuzz_address_parsing corpus/addresses/ -dump_dictionary=dict.txt

# Use dictionary
./fuzz_address_parsing -dict=dict.txt corpus/addresses/
```

## Security Considerations

⚠️ **WARNING**: Fuzzing can generate malicious inputs that may:
- Cause infinite loops
- Consume excessive memory
- Trigger security vulnerabilities

**Always run fuzzers in isolated environments** with:
- Resource limits (CPU, memory)
- Network isolation
- Regular monitoring

## Troubleshooting

### Windows: "container-overflow" False Positives

On Windows, you may encounter AddressSanitizer container-overflow errors that are false positives from the fuzzer's internal mutation tracking (in `__sanitizer_weak_hook_memmem`), not actual vulnerabilities in your code.

**Solution:** Disable container overflow detection when running fuzzers on Windows:

```bash
# Windows (Command Prompt)
set ASAN_OPTIONS=detect_container_overflow=0
fuzz_address_parsing.exe -max_total_time=300

# Windows (PowerShell)
$env:ASAN_OPTIONS="detect_container_overflow=0"
./fuzz_address_parsing.exe -max_total_time=300

# Linux/macOS (not typically needed, but same option works)
ASAN_OPTIONS=detect_container_overflow=0 ./fuzz_address_parsing -max_total_time=300
```

This is a known issue with ASan's strict container bounds checking interacting with libFuzzer's memory operations during input mutation. The crypto operations are still fully tested and protected by other ASan checks (heap overflow, use-after-free, etc.).

### "undefined reference to `LLVMFuzzerTestOneInput`"

Ensure you're linking with `-fsanitize=fuzzer`:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=fuzzer,address,undefined")
```

### Slow Performance

- Reduce `-max_len` for faster iterations
- Use `-only_ascii=1` for text-based targets
- Enable `-fork=1` for parallel fuzzing

### Out of Memory

- Set `-rss_limit_mb=4096` (4GB limit)
- Reduce corpus size with `-merge=1`
- Use `-max_total_time` to limit run duration

## Contributing

When adding new fuzzers:
1. Create harness in `harnesses/fuzz_<component>.cpp`
2. Add to `fuzzing/CMakeLists.txt`
3. Generate initial corpus
4. Document target in this README

## References

- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [OSS-Fuzz](https://github.com/google/oss-fuzz)

## License

Same as CriptoGualet project (see main LICENSE file)
