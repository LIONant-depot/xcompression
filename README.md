# xcompression: Supercharge Your Data Compression in C++

Unlock the full potential of high-speed, efficient data compression with xcompression – a modern C++ wrapper around the blazing-fast 
Facebook Zstandard (Zstd) library! Bid farewell to cumbersome compression APIs and embrace a streamlined interface for block and 
streaming modes, fixed or dynamic blocks, and tunable compression levels. Whether you're crunching big data, optimizing storage, 
or streaming content, xcompression delivers top-tier performance with ease!

## Key Features

* ***Dual Modes Mastery:*** Seamless support for block mode (single-frame compression) and streaming mode (chunked processing for large datasets)
* ***Fixed & Dynamic Blocks:*** Choose fixed for predictable sizing or dynamic for adaptive compression on varied data patterns
* ***Compression Levels:*** FAST for speed, MEDIUM for balance, HIGH for maximum ratio – pick your power!
* ***Smart Error Handling:*** Lightweight `err` struct with states like OK, FAILURE, NOT_DONE, and INCOMPRESSIBLE (fallback to original data effortlessly)
* ***Modern C++ Goodness:*** Uses `std::span<std::byte>` for safe, efficient memory views; C++20 compatible
* ***Incompressible Data Detection:*** Automatically signals when compression doesn't shrink data, saving you time and space
* ***Zstd-Powered:*** Leverages the proven Zstd engine for ultra-fast compression/decompression
* ***MIT License:*** Completely open and free to use
* ***Easy Integration:*** Include `xcompression.h`, link against Zstd (`-lzstd`), and you're set – no other dependencies!
* ***Documentation:*** [Documentated](https://github.com/LIONant-depot/xcompression/blob/main/documentation/documentation.md) so you can follow up easiely 

## Code Example

```cpp
#include "xcompression.h"
#include <vector>
#include <span>
#include <iostream>

int main() {
    std::vector<std::byte> source(1000, std::byte{'A'});  // Compressible sample data
    std::vector<std::byte> compressed(source.size());
    std::vector<std::byte> decompressed(source.size());

    // Compress in block mode (fixed)
    xcompression::fixed_block_compress compressor;
    if (auto err = compressor.Init(true, source.size(), source, xcompression::fixed_block_compress::level::MEDIUM); err) {
        std::cerr << "Init failed: " << err.m_pMessage << std::endl;
        return 1;
    }
    std::uint64_t compressedSize;
    if (auto err = compressor.Pack(compressedSize, compressed); err) {
        if (err.getState() == xcompression::err::state::INCOMPRESSIBLE) {
            compressed = source;  // Fallback to original
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

    // Verify (in real code, check equality)
    std::cout << "Compressed size: " << compressedSize << " | Decompressed size: " << decompressedSize << std::endl;
    return 0;
}
```

Dive in and compress your way to efficiency – star, fork, and contribute now! 🚀