#include <vector>
#include <array>
#include <iostream>
#include <random>
#include <cassert>

namespace xcompression::unit_test
{
    void TestFixedInputDrivenStreaming(std::span<const std::byte> Source, const std::size_t BlockSize)
    {
        std::size_t                         TotalSizeCompress       = 0;
        std::vector<std::vector<std::byte>> streamCompressedBlocks  = {};

        // Compressed buffer sized to input size (sufficient since incompressible data returns error)
        // Test input-driven streaming (fixed_block_compress/fixed_block_decompress, double-buffer simulation)

        //
        // Compress
        //
        std::vector<std::byte> compressed(Source.size());
        {
            xcompression::fixed_block_compress compressor;
            if (auto err = compressor.Init(false, BlockSize, Source, xcompression::fixed_block_compress::level::MEDIUM); err)
            {
                std::cout << "Streaming mode (input-driven) compression init failed: " << err.m_pMessage << "\n";
                assert(false);
            }
            std::uint64_t lastPosition = 0;
            std::uint64_t compressedSize = 0;
            while (true)
            {
                lastPosition = compressor.m_Position;
                auto err = compressor.Pack(compressedSize, compressed);
                if (err)
                {
                    if (err.getState<xcompression::state>() == xcompression::state::INCOMPRESSIBLE)
                    {
                        std::cout << "Streaming mode (input-driven): Data incompressible at position " << compressor.m_Position << ", using original chunk\n";
                        streamCompressedBlocks.emplace_back(Source.begin() + lastPosition, Source.begin() + compressor.m_Position);
                        TotalSizeCompress += BlockSize;
                        continue;
                    }
                    else if (err.getState<xcompression::state>() != xcompression::state::NOT_DONE)
                    {
                        std::cout << "Streaming mode (input-driven) compression failed: " << err.m_pMessage << "\n";
                        assert(false);
                    }
                }

                if (compressedSize > 0)
                {
                    streamCompressedBlocks.emplace_back(compressed.begin(), compressed.begin() + compressedSize);
                    TotalSizeCompress += compressedSize;
                }

                if (err == false )
                    break;
            }
        }

        //
        // Decompress
        // 
        {
            // Double-buffer simulation: two buffers of BlockSize
            xcompression::fixed_block_decompress decompressor;
            if (auto err = decompressor.Init(false, BlockSize); err)
            {
                std::cout << "Streaming mode (input-driven) decompression init failed: " << err.m_pMessage << "\n";
                assert(false);
            }

            std::array              buffer          = { std::vector<std::byte>(BlockSize), std::vector<std::byte>(BlockSize) };
            int                     useBuffer       = 0;
            std::vector<std::byte>  rebuiltSource   = {};
            for (const auto& block : streamCompressedBlocks)
            {
                if (block.size() == BlockSize)
                {
                    rebuiltSource.insert(rebuiltSource.end(), block.begin(), block.end());
                    continue;
                }

                std::vector<std::byte>& currentBuffer           = buffer[useBuffer & 1];
                std::uint32_t           blockDecompressedSize;

                auto err = decompressor.Unpack(blockDecompressedSize, std::span(currentBuffer.data(), BlockSize), std::span(block.data(), block.size()));
                while (err && err.getState<xcompression::state>() == xcompression::state::NOT_DONE)
                {
                    rebuiltSource.insert(rebuiltSource.end(), currentBuffer.begin(), currentBuffer.begin() + blockDecompressedSize);
                    err = decompressor.Unpack(blockDecompressedSize, std::span(currentBuffer.data(), BlockSize), std::span(block.data(), block.size()));
                }

                if (err)
                {
                    std::cout << "Streaming mode (input-driven) decompression failed: " << err.m_pMessage << "\n";
                    assert(false);
                }

                rebuiltSource.insert(rebuiltSource.end(), currentBuffer.begin(), currentBuffer.begin() + blockDecompressedSize);

                // Swap buffers
                useBuffer++; 
            }

            //
            // Check that everything when OK
            //
            if (false == std::equal(rebuiltSource.begin(), rebuiltSource.end(), Source.begin(), Source.end()))
            {
                std::cout << "Streaming mode (input-driven): Rebuilt data does not match original\n";
                assert(false);
            }

            // Tell the world what we did...
            std::cout << "Streaming mode (input-driven): match original ( with compression size : " << TotalSizeCompress << " and number of block " << streamCompressedBlocks.size() << ") \n";
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    void TestFixedBlock(std::span<const std::byte> Source)
    {
        // Compressed buffer sized to input size (sufficient since incompressible data returns error)
        std::vector<std::byte> compressed(Source.size());
        //
        // Test block mode
        //
        {
            xcompression::fixed_block_compress compressor;
            if (auto err = compressor.Init(true, Source.size(), Source, xcompression::fixed_block_compress::level::MEDIUM); err)
            {
                std::cout << "Block mode compression init failed: " << err.m_pMessage << "\n";
                assert(false);
            }
            std::uint64_t           compressedSize;
            std::vector<std::byte>  blockOutput;
            if (auto err = compressor.Pack(compressedSize, compressed); err)
            {
                if (err.getState<xcompression::state>() == xcompression::state::INCOMPRESSIBLE)
                {
                    std::cout << "Block mode: Data incompressible, using original data\n";
                    blockOutput.insert(blockOutput.begin(), Source.begin(), Source.end());
                    compressedSize = Source.size();
                }
                else
                {
                    std::cout << "Block mode compression failed: " << err.m_pMessage << "\n";
                    assert(false);
                }
            }
            else
            {
                blockOutput.assign(compressed.begin(), compressed.begin() + compressedSize);
            }

            //
            // Decompressed
            //
            std::vector<std::byte> verifiedDecompressed;
            if (compressedSize == Source.size())
            {
                verifiedDecompressed.insert(verifiedDecompressed.begin(), Source.begin(), Source.end());
            }
            else
            {
                xcompression::fixed_block_decompress decompressor;
                if (auto err = decompressor.Init(true, Source.size()); err)
                {
                    std::cout << "Block mode decompression init failed: " << err.m_pMessage << "\n";
                    assert(false);
                }
                std::uint32_t decompressedSize;
                verifiedDecompressed.resize(Source.size());
                if (auto err = decompressor.Unpack(decompressedSize, std::span(verifiedDecompressed.data(), Source.size()), std::span(blockOutput.data(), blockOutput.size())); err)
                {
                    std::cout << "Block mode decompression failed: " << err.m_pMessage << "\n";
                    assert(false);
                }
                verifiedDecompressed.resize(decompressedSize);
            }
            if (false == std::equal(verifiedDecompressed.begin(), verifiedDecompressed.end(), Source.begin(), Source.end()))
            {
                std::cout << "Block mode: Decompressed data does not match original\n";
                assert(false);
            }

            std::cout << "Block mode decompressed size: " << verifiedDecompressed.size() << " (matches original) Compressed Size " << compressedSize << "\n";
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    void TestDynamicBlock(std::span<const std::byte> Source)
    {
        // Compressed buffer sized to input size (sufficient since incompressible data returns error)
        std::vector<std::byte> compressed(Source.size());
        //
        // Test block mode
        //
        {
            xcompression::dynamic_block_compress compressor;
            if (auto err = compressor.Init(true, Source.size(), Source, xcompression::dynamic_block_compress::level::MEDIUM); err)
            {
                std::cout << "Dynamic block mode compression init failed: " << err.m_pMessage << "\n";
                assert(false);
            }

            std::uint64_t           compressedSize;
            std::vector<std::byte>  blockOutput;
            if (auto err = compressor.Pack(compressedSize, compressed); err)
            {
                if (err.getState<xcompression::state>() == xcompression::state::INCOMPRESSIBLE)
                {
                    std::cout << "Dynamic block mode: Data incompressible, using original data\n";
                    blockOutput.insert(blockOutput.begin(), Source.begin(), Source.end());
                    compressedSize = Source.size();
                }
                else
                {
                    std::cout << "Dynamic block mode compression failed: " << err.m_pMessage << "\n";
                    assert(false);
                }
            }
            else
            {
                blockOutput.assign(compressed.begin(), compressed.begin() + compressedSize);
            }

            //
            // Decompress
            //
            std::vector<std::byte> verifiedDecompressed;
            if (compressedSize == Source.size())
            {
                verifiedDecompressed.insert(verifiedDecompressed.begin(), Source.begin(), Source.end());
            }
            else
            {
                xcompression::dynamic_block_decompress decompressor;
                if (auto err = decompressor.Init(true, Source.size()); err)
                {
                    std::cout << "Dynamic block mode decompression init failed: " << err.m_pMessage << "\n";
                    assert(false);
                }

                std::uint32_t decompressedSize;
                verifiedDecompressed.resize(Source.size());
                if (auto err = decompressor.Unpack(decompressedSize, std::span(verifiedDecompressed.data(), verifiedDecompressed.size()), std::span(blockOutput.data(), blockOutput.size())); err)
                {
                    std::cout << "Dynamic block mode decompression failed: " << err.m_pMessage << "\n";
                    assert(false);
                }
                verifiedDecompressed.resize(decompressedSize);
            }

            //
            // Make sure everything went OK
            //
            if (false == std::equal(verifiedDecompressed.begin(), verifiedDecompressed.end(), Source.begin(), Source.end()))
            {
                std::cout << "Dynamic block mode: Decompressed data does not match original\n";
                assert(false);
            }

            std::cout << "Dynamic block mode decompressed size: " << verifiedDecompressed.size() << " (matches original), Compressed Size: " << compressedSize << "\n";
        }
    }

    //-------------------------------------------------------------------------------------------------------------

    void TestDynamicInputDrivenStreaming(std::span<const std::byte> Source, const std::size_t BlockSize)
    {
        std::size_t                         TotalSizeCompress       = 0;
        std::vector<std::vector<std::byte>> streamCompressedBlocks  = {};

        // Compressed buffer sized to input size (sufficient since incompressible data returns error)
        // Test input-driven streaming (dynamic_block_compress/dynamic_block_decompress, double-buffer simulation)

        //
        // Compress the data
        //
        std::vector<std::byte> compressed(Source.size());
        {
            xcompression::dynamic_block_compress compressor;
            if (auto err = compressor.Init(false, BlockSize, Source, xcompression::dynamic_block_compress::level::MEDIUM); err)
            {
                std::cout << "Streaming mode (input-driven, dynamic): compression init failed: " << err.m_pMessage << "\n";
                assert(false);
            }

            std::uint64_t lastPosition = 0;
            std::uint64_t compressedSize = 0;
            while (true)
            {
                lastPosition = compressor.m_Position;
                auto err = compressor.Pack(compressedSize, compressed);
                if (err)
                {
                    if (err.getState<xcompression::state>() == xcompression::state::INCOMPRESSIBLE)
                    {
                        std::cout << "Streaming mode (input-driven, dynamic): Data incompressible at position " << compressor.m_Position << ", using original chunk\n";
                        streamCompressedBlocks.emplace_back(Source.begin() + lastPosition, Source.begin() + compressor.m_Position);
                        TotalSizeCompress += BlockSize;
                        continue;
                    }
                    else if (err.getState<xcompression::state>() != xcompression::state::NOT_DONE)
                    {
                        std::cout << "Streaming mode (input-driven, dynamic): compression failed: " << err.m_pMessage << "\n";
                        assert(false);
                    }
                }

                if (compressedSize > 0)
                {
                    TotalSizeCompress += compressedSize;
                    streamCompressedBlocks.emplace_back(compressed.begin(), compressed.begin() + compressedSize);
                }

                // Are we done?
                if (err == false) 
                    break;
            }
        }

        //
        // Decompress the data
        //
        {
            // Double-buffer simulation: two buffers of BlockSize
            xcompression::dynamic_block_decompress decompressor;
            if (auto err = decompressor.Init(false, BlockSize); err)
            {
                std::cout << "Streaming mode (input-driven, dynamic): decompression init failed: " << err.m_pMessage << "\n";
                assert(false);
            }

            int                     useBuffer = 0;
            std::vector<std::byte>  rebuiltSource(Source.size());
            std::size_t             Pos = 0;

            for (const auto& block : streamCompressedBlocks)
            {
                // This is a block that did not get compressed
                if (block.size() == BlockSize)
                {
                    for (int i = 0; i < block.size(); ++i)
                    {
                        rebuiltSource[Pos++] = block[i];
                    }
                    continue;
                }

                std::uint32_t blockDecompressedSize;
                auto err = decompressor.Unpack(blockDecompressedSize, std::span(rebuiltSource.data() + Pos, rebuiltSource.size() - Pos), std::span(block.data(), block.size()));
                Pos += blockDecompressedSize;
                if (err)
                {
                    if (err.getState<xcompression::state>() == xcompression::state::NOT_DONE)
                    {
                        continue;
                    }

                    std::cout << "Streaming mode (input-driven, dynamic): decompression failed: " << err.m_pMessage << "\n";
                    assert(false);
                }
            }

            //
            // Check everything is OK
            //
            if (false == std::equal(rebuiltSource.begin(), rebuiltSource.end(), Source.begin(), Source.end()))
            {
                std::cout << "Streaming mode (input-driven, dynamic): Rebuilt data does not match original\n";
                assert(false);
            }

            // Tell the world what we accomplished 
            std::cout << "Streaming mode (input-driven, dynamic): match original (Compressed size " << TotalSizeCompress << " and number of block " << streamCompressedBlocks.size() << " ) \n";
        }
    }

    //-------------------------------------------------------------------------------------------------------------

    void RunAllUnitTest()
    {
        constexpr auto SourceSize = 1000;
        constexpr auto BlockSize  = 100;

        //
        // Initialize Source Data
        //
        std::vector<std::byte>          source;
        unsigned int                    seed = 12345;
        std::mt19937                    gen(seed);
        std::uniform_int_distribution<> dis(0, 255);

        do
        {
            int count = dis(gen);
            for (size_t i = 0; i < count; ++i)
            {
                if (source.size() >= SourceSize) break;

                // Compressible pattern
                source.push_back(std::byte{ 'A' }); 
            }

            count = dis(gen);
            for (size_t i = 0; i < count; ++i)
            {
                if (source.size() >= SourceSize) break;

                // Random, possibly incompressible
                source.push_back(std::byte(static_cast<unsigned char>(dis(gen)))); 
            }

        } while (source.size() < SourceSize);

        //
        // Run all the tests
        //
        TestFixedInputDrivenStreaming(source, BlockSize);
        TestFixedBlock(source);
        TestDynamicBlock(source);
        TestDynamicInputDrivenStreaming(source, BlockSize);
    }
}
