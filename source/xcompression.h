#ifndef XCOMPRESSION_H
#define XCOMPRESSION_H
#pragma once

#include <memory>
#include <span>
#include <array>
#include <cstddef>

namespace xcompression
{
    //-----------------------------------------------------------------------------------------------------
    struct err
    {
        enum class state : std::uint8_t
        {
            OK = 0
            , FAILURE
            , NOT_DONE
            , INCOMPRESSIBLE
        };

        constexpr err(void) noexcept = default;
        consteval err(const char* p) noexcept : m_pMessage(p) {}

        constexpr operator bool(void) const noexcept { return !!m_pMessage; }
        constexpr state getState(void) const noexcept { return m_pMessage ? static_cast<state>(m_pMessage[-1]) : state::OK; }
        inline void clear(void) noexcept { m_pMessage = nullptr; }

        template<std::size_t N>
        struct string_literal
        {
            std::array<char, N> m_Value;
            consteval string_literal(const char(&str)[N]) noexcept { for (std::size_t i = 0; i < N; ++i) m_Value[i] = str[i]; }
        };

        template <string_literal T_STR_V, state T_STATE_V>
        inline constexpr static auto data_v = []() consteval noexcept
            {
                std::array<char, T_STR_V.m_Value.size() + 1> temp = {};
                temp[0] = static_cast<char>(T_STATE_V);
                for (std::size_t i = 1; i < T_STR_V.m_Value.size(); ++i) temp[i] = T_STR_V.m_Value[i - 1];
                return temp;
            }();

        template <state T_STATE_V, string_literal T_STR_V> consteval static err create() noexcept { return { data_v<T_STR_V, T_STATE_V>.data() + 1 }; }
        template <string_literal T_STR_V> consteval static err create_f() noexcept { return { data_v<T_STR_V, state::FAILURE>.data() + 1 }; }

        const char* m_pMessage = nullptr;
    };

    //-----------------------------------------------------------------------------------------------------
    struct fixed_block_compress
    {
        enum class level : std::uint8_t
        {
            FAST
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
        err Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel = level::HIGH) noexcept;

        // Compresses data into DestinationCompress, updating CompressedSize with bytes written.
        // DestinationCompress must be at least SourceUncompress.size() in block mode, or BlockSize (or remaining input size) in streaming mode.
        // Returns err::state::INCOMPRESSIBLE if the compressed size is not smaller than the input size,
        // in which case DestinationCompress is unchanged and the user should fall back to the original data.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        err Pack(std::uint64_t& CompressedSize, std::span<std::byte> DestinationCompress) noexcept;

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
        err Init(bool bBlockIsOutputSize, std::uint64_t BlockSize) noexcept;

        // Decompresses into DestinationUncompress, updating DecompressSize with bytes written.
        // DestinationUncompress must be exactly BlockSize in both block and streaming modes.
        // In streaming mode, DecompressSize may be less than BlockSize for the last block; users should advance their cursor by DecompressSize.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        err Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed) noexcept;

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
        err Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel = level::HIGH) noexcept;

        // Compresses data into DestinationCompress, updating CompressedSize with bytes written.
        // DestinationCompress must be at least SourceUncompress.size() in block mode, or BlockSize (or remaining input size) in streaming mode.
        // Returns err::state::INCOMPRESSIBLE if the compressed size is not smaller than the input size,
        // in which case DestinationCompress is unchanged and the user should fall back to the original data.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        err Pack(std::uint64_t& CompressedSize, std::span<std::byte> DestinationCompress) noexcept;

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
        err Init(bool bBlockIsOutputSize, std::uint64_t BlockSize) noexcept;

        // Decompresses into DestinationUncompress, updating DecompressSize with bytes written.
        // DestinationUncompress must be at least BlockSize in both block and streaming modes.
        // In streaming mode, DecompressSize may be less than BlockSize for the last block; users should advance their cursor by DecompressSize.
        // Returns err::state::NOT_DONE in streaming mode if more data needs to be processed.
        err Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed) noexcept;

        void* m_pDCTX = nullptr;
        std::uint64_t m_Position = 0; // Tracks input progress
        std::uint64_t m_OutputPosition = 0; // Tracks output progress
        std::uint64_t m_BlockSize = 0;
        bool m_bBlockIsOutputSize = false;
    };
}

#endif