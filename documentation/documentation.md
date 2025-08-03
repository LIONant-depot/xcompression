# xcompression Library

## Overview

The `xcompression` library is a C++ wrapper around the [Facebook Zstandard (Zstd)](https://github.com/facebook/zstd) compression library.
It provides a simplified interface for compressing and decompressing data in both **block mode** (treating the entire input as a single frame)
and **streaming mode** (processing data in chunks). The library supports two variants:

- **Fixed Block**: Designed for scenarios where block sizes are fixed and predictable, often optimizing for consistent performance.
- **Dynamic Block**: Allows for more flexible block handling, potentially adapting to data patterns for better compression ratios in 
   varying scenarios.

Key features:
- Compression levels: FAST, MEDIUM, HIGH.
- Handles incompressible data gracefully by returning an error code, allowing fallback to original data.
- Supports input/output as `std::span<std::byte>` for modern C++ memory safety.
- Error handling via a lightweight `err` struct.
- Streaming mode enables processing large datasets without loading everything into memory.

This library is header-only for the interface but relies on Zstd for the underlying compression engine. It uses Zstd contexts internally 
for efficiency.

**Note**: The library does not perform compression itself; it delegates to Zstd. If compression does not reduce size 
(i.e., data is incompressible), it signals this, and you should store/use the original data.

## Dependencies

In the build directory there is a batch file which you can run which will download [zstd](https://github.com/facebook/zstd)
and compile it. xcompression.cpp will automatically link with the lib. Just make sure that your project the linker seach directory
has xcompression project in it.

**C++ Standard**: Requires C++20 or later (uses `std::span`, `std::byte`, etc.).

To use:
- Include `xcompression.h` and `xcompression.cpp` in your code.

## Error Handling

Errors are returned via the `err` struct, which is lightweight and can be used like a boolean (truthy if an error occurred).

- **States** (enum `err::state`):
  - `OK`: Success.
  - `FAILURE`: General failure (e.g., initialization or internal error).
  - `NOT_DONE`: In streaming mode, indicates more data needs processing (call the method again).
  - `INCOMPRESSIBLE`: Compression did not reduce size; fallback to original data.

- Usage example:
  ```cpp
  xcompression::err error = someFunction();
  if (error) 
  {
      if (error.getState() == xcompression::err::state::INCOMPRESSIBLE) 
      {
          // Use original data
      } 
      else 
      {
          std::cout << "Error: " << error.m_pMessage << std::endl;
      }
  }
  ```

- Clear an error with `error.clear()`.

## Compression Levels

All compressors support three levels (enum `level`):
- `FAST`: Prioritizes speed over ratio.
- `MEDIUM`: Balanced (default in examples).
- `HIGH`: Prioritizes compression ratio over speed.

Specify during `Init()`.

## Fixed Block Compression

### fixed_block_compress

Handles compression in fixed block sizes.

- **Constructor/Destructor**: Default constructor; destructor frees internal Zstd context.
- **Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel = level::HIGH)**:
  - `bBlockSizeIsOutputSize`: `true` for block mode (compress entire input to target output size `BlockSize`); `false` for streaming mode (input chunk size <= `BlockSize` per `Pack` call).
  - `BlockSize`: Block size in bytes.
  - `SourceUncompress`: Input data to compress.
  - Returns `err` on failure.

- **Pack(std::uint64_t& CompressedSize, std::span<std::byte> DestinationCompress)**:
  - Compresses data into `DestinationCompress` (must be at least input size or `BlockSize`).
  - Updates `CompressedSize` with bytes written.
  - In streaming mode, returns `NOT_DONE` if more chunks remain.
  - Returns `INCOMPRESSIBLE` if no size reduction (destination unchanged).
  - Returns `err` on other failures.

### fixed_block_decompress

Handles decompression for fixed blocks.

- **Constructor/Destructor**: Similar to compressor.
- **Init(bool bBlockIsOutputSize, std::uint64_t BlockSize)**:
  - `bBlockIsOutputSize`: `true` for block mode (expect output == `BlockSize`); `false` for streaming (max decompressed block == `BlockSize`).
  - `BlockSize`: Block size in bytes.
  - Returns `err` on failure.

- **Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed)**:
  - Decompresses `SourceCompressed` into `DestinationUncompress` (must be exactly `BlockSize`).
  - Updates `DecompressSize` with bytes written (may be < `BlockSize` for last streaming block).
  - In streaming mode, returns `NOT_DONE` if more data needed.
  - Returns `err` on failures.

## Dynamic Block Compression

Similar to fixed block but optimized for dynamic/variable data patterns. Internally, it may use different Zstd parameters for adaptability.

### dynamic_block_compress

- **Init** and **Pack**: Identical interface and behavior to `fixed_block_compress`.

### dynamic_block_decompress

- **Init** and **Unpack**: Identical interface and behavior to `fixed_block_decompress`.

**Key Difference**: Use dynamic for data with varying compressibility (e.g., mixed text/binary). Fixed may be better for uniform data. Test both for your use case.

## Examples

### Block Mode (Entire Input as Single Frame)

Compress and decompress a full buffer:

```cpp
#include "xcompression.h"
#include <vector>
#include <span>
#include <iostream>

int main() {
    std::vector<std::byte> source(1000, std::byte{'A'});  // Compressible data
    std::vector<std::byte> compressed(source.size());
    std::vector<std::byte> decompressed(source.size());

    // Compress (fixed block)
    xcompression::fixed_block_compress compressor;
    if (auto err = compressor.Init(true, source.size(), source, xcompression::fixed_block_compress::level::MEDIUM); err) {
        std::cerr << "Init failed: " << err.m_pMessage << std::endl;
        return 1;
    }
    std::uint64_t compressedSize;
    if (auto err = compressor.Pack(compressedSize, compressed); err) {
        if (err.getState() == xcompression::err::state::INCOMPRESSIBLE) {
            // Fallback to original
            compressed = source;
            compressedSize = source.size();
        } else {
            std::cerr << "Pack failed: " << err.m_pMessage << std::endl;
            return 1;
        }
    }

    // Decompress
    xcompression::fixed_block_decompress decompressor;
    if (auto err = decompressor.Init(true, source.size()); err) {
        std::cerr << "Init failed: " << err.m_pMessage << std::endl;
        return 1;
    }
    std::uint32_t decompressedSize;
    if (auto err = decompressor.Unpack(decompressedSize, decompressed, std::span(compressed.data(), compressedSize)); err) {
        std::cerr << "Unpack failed: " << err.m_pMessage << std::endl;
        return 1;
    }

    // Verify
    if (std::equal(decompressed.begin(), decompressed.begin() + decompressedSize, source.begin())) {
        std::cout << "Success! Compressed size: " << compressedSize << std::endl;
    }
    return 0;
}
```

### Streaming Mode (Chunked Processing)

Process in chunks (e.g., for large files). See unit tests for double-buffer simulation in streaming decompression.

From unit tests (simplified):
- Initialize with `false` for streaming.
- Loop `Pack` until done, handling `NOT_DONE` and `INCOMPRESSIBLE`.
- For decompression, loop `Unpack` on each compressed chunk.

For dynamic variant, replace `fixed_block_*` with `dynamic_block_*`.

## Unit Tests

The provided `xcompression_unittest.h` includes tests for all modes:
- `TestFixedInputDrivenStreaming`: Streaming with fixed blocks.
- `TestFixedBlock`: Block mode with fixed.
- `TestDynamicBlock`: Block mode with dynamic.
- `TestDynamicInputDrivenStreaming`: Streaming with dynamic.
- Run `RunAllUnitTest()` to verify.

These generate random compressible/incompressible data and assert round-trip integrity.
Please look at the unitest as it provides good examples on how to use the library.

## Limitations and Tips

- **Buffer Sizing**: Always allocate compressed buffers >= input size (worst case: incompressible).
- **Incompressible Data**: Handle `INCOMPRESSIBLE` by storing original chunks.
- **Streaming**: Track positions manually; last chunk may be smaller.
- **Performance**: Test levels and modes for your data. Zstd is fast, but HIGH level may be slower.
- **Thread Safety**: Not inherently thread-safe; use per-thread contexts.
- **Zstd Version**: Compatible with recent Zstd versions; check for updates.

For advanced Zstd features, refer to Zstd docs. If issues arise, ensure Zstd is properly linked.