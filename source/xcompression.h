#ifndef XCOMPRESSION_H
#define XCOMPRESSION_H
#pragma once

#include "source/xerr.h"

#include <memory>
#include <span>
#include <cstddef>

namespace xcompression
{
    enum class state : std::uint8_t
    { OK
    , FAILURE
    , NOT_DONE
    , INCOMPRESSIBLE
    };

    //-----------------------------------------------------------------------------------------------------
    struct fixed_block_compress
    {
        enum class level : std::uint8_t
        { FAST
        , MEDIUM
        , HIGH
        };

        fixed_block_compress() = default;
        ~fixed_block_compress(void) noexcept;

        // Initializes compression context.
        // bBlockSizeIsOutputSize: If true, compresses entire input as a single frame with target block size BlockSize.
        // If false, uses streaming mode with BlockSize as the maximum input chunk size per Pack call (last chunk may be smaller).
        // SourceUncompress: The input data to compress.
        // CompressionLevel: The desired compression level (FAST, MEDIUM, HIGH).
        xerr Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel = level::HIGH) noexcept;

        // Compresses data into DestinationCompress, updating CompressedSize with bytes written.
        // DestinationCompress must be at least SourceUncompress.size() in block mode, or BlockSize (or remaining input size) in streaming mode.
        // Returns err::state::INCOMPRESSIBLE if the compressed size is not smaller than the input size,
        // in which case DestinationCompress is unchanged and the user should fall back to the original data.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        xerr Pack(std::uint64_t& CompressedSize, std::span<std::byte> DestinationCompress) noexcept;

        void* m_pCCTX = nullptr;
        std::uint64_t m_Position = 0;
        std::span<const std::byte> m_Src = {};
        std::uint64_t m_BlockSize = 0;
        bool m_bBlockSizeIsOutputSize = false;
    };

    //-----------------------------------------------------------------------------------------------------
    struct fixed_block_decompress
    {
        fixed_block_decompress() = default;
        ~fixed_block_decompress(void) noexcept;

        // Initializes decompression context.
        // bBlockIsOutputSize: If true, decompresses entire input as a single frame, expecting output size == BlockSize.
        // If false, uses streaming mode with BlockSize as the maximum decompressed block size (last block may be smaller).
        xerr Init(bool bBlockIsOutputSize, std::uint64_t BlockSize) noexcept;

        // Decompresses into DestinationUncompress, updating DecompressSize with bytes written.
        // DestinationUncompress must be exactly BlockSize in both block and streaming modes.
        // In streaming mode, DecompressSize may be less than BlockSize for the last block; users should advance their cursor by DecompressSize.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        xerr Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed) noexcept;

        void* m_pDCTX = nullptr;
        std::uint64_t m_Position = 0; // Tracks input progress
        std::uint64_t m_OutputPosition = 0; // Tracks output progress
        std::uint64_t m_BlockSize = 0;
        bool m_bBlockIsOutputSize = false;
    };

    //-----------------------------------------------------------------------------------------------------
    struct dynamic_block_compress
    {
        enum class level : std::uint8_t
        {
            FAST
            , MEDIUM
            , HIGH
        };

        dynamic_block_compress() = default;
        ~dynamic_block_compress(void) noexcept;

        // Initializes compression context.
        // bBlockSizeIsOutputSize: If true, compresses entire input as a single frame with target block size BlockSize.
        // If false, uses streaming mode with BlockSize as the maximum input chunk size per Pack call (last chunk may be smaller).
        // SourceUncompress: The input data to compress.
        // CompressionLevel: The desired compression level (FAST, MEDIUM, HIGH).
        xerr Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel = level::HIGH) noexcept;

        // Compresses data into DestinationCompress, updating CompressedSize with bytes written.
        // DestinationCompress must be at least SourceUncompress.size() in block mode, or BlockSize (or remaining input size) in streaming mode.
        // Returns err::state::INCOMPRESSIBLE if the compressed size is not smaller than the input size,
        // in which case DestinationCompress is unchanged and the user should fall back to the original data.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        xerr Pack(std::uint64_t& CompressedSize, std::span<std::byte> DestinationCompress) noexcept;

        void* m_pCCTX = nullptr;
        std::uint64_t m_Position = 0;
        std::span<const std::byte> m_Src = {};
        std::uint64_t m_BlockSize = 0;
        bool m_bBlockSizeIsOutputSize = false;
    };

    //-----------------------------------------------------------------------------------------------------
    struct dynamic_block_decompress
    {
        dynamic_block_decompress() = default;
        ~dynamic_block_decompress(void) noexcept;

        // Initializes decompression context.
        // bBlockSizeIsOutputSize: If true, decompresses entire input as a single frame, expecting output size == BlockSize.
        // If false, uses streaming mode with BlockSize as the maximum input chunk size per Unpack call (last chunk may be smaller).
        xerr Init(bool bBlockIsOutputSize, std::uint64_t BlockSize) noexcept;

        // Decompresses into DestinationUncompress, updating DecompressSize with bytes written.
        // DestinationUncompress must be at least BlockSize in both block and streaming modes.
        // In streaming mode, DecompressSize may be less than BlockSize for the last block; users should advance their cursor by DecompressSize.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        xerr Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed) noexcept;

        void* m_pDCTX = nullptr;
        std::uint64_t m_Position = 0; // Tracks input progress
        std::uint64_t m_OutputPosition = 0; // Tracks output progress
        std::uint64_t m_BlockSize = 0;
        bool m_bBlockIsOutputSize = false;
    };
}

#endif