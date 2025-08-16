#define ZSTD_STATIC_LINKING_ONLY
#include "../dependencies/zstd/lib/zstd.h"
#include "xcompression.h"
#include <cassert>
#include <cstring>
#include <iostream>

//-------------------------------------------------------------------------------------------------------
// Add libz libraries
//-------------------------------------------------------------------------------------------------------
#if _DEBUG
# pragma comment(lib, "dependencies/zstd/build/VS2010/libzstd/bin/x64_Debug/libzstd_static.lib")
#else
# pragma comment(lib, "dependencies/zstd/build/VS2010/libzstd/bin/x64_Release/libzstd_static.lib")
#endif

namespace xcompression
{
    //------------------------------------------------------------------------------
    // Description:
    // Computes the Log2 of an integral value.
    // It answers the question: how many bits do I need to rshift 'y' to make this expression true:
    // (input) x == 1 << 'y'. Assuming x was originally a power of 2.
    //------------------------------------------------------------------------------
    template<typename T> constexpr
        T Log2Int(T x, int p = 0) noexcept requires std::is_integral_v<T>
    {
        return (x <= 1) ? p : Log2Int(x >> 1, p + 1);
    }

    //------------------------------------------------------------------------------
    // Description:
    // Determines the minimum power of two that encapsulates the given number
    // Example:
    // Log2IntRoundUp(3) == 2 // it takes 2 bits to store #3
    //------------------------------------------------------------------------------
    template<typename T> constexpr
        T Log2IntRoundUp(T x) noexcept requires std::is_integral_v<T>
    {
        return x < 1 ? 0 : Log2Int(x - 1) + 1;
    }

    //-------------------------------------------------------------------------------------------------------
    void PrintError(size_t result)
    {
#ifdef _DEBUG
        const char* errorName = ZSTD_getErrorName(result);
        std::cout << "ZSTD Error ("
            << result
            << ") : "
            << errorName
            << "\n";
#endif
    }

    //-------------------------------------------------------------------------------------------------------
    xerr fixed_block_compress::Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel) noexcept
    {
        assert(!m_pCCTX);
        assert(BlockSize > 0);
        assert(SourceUncompress.data());

        auto pCCTX = ZSTD_createCCtx();
        if (!pCCTX) return xerr::create_f<state,"Error ZSTD_createCCtx">();

        // Reset context to ensure clean state
        if (ZSTD_isError(ZSTD_CCtx_reset(pCCTX, ZSTD_reset_session_and_parameters)))
        {
            ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(pCCTX));
            return xerr::create_f<state, "Error ZSTD_CCtx_reset">();
        }

        // Set compression parameters
        int cLevel;
        switch (CompressionLevel)
        {
        case level::FAST: cLevel = 1; break;
        case level::MEDIUM: cLevel = ZSTD_CLEVEL_DEFAULT; break;
        case level::HIGH: cLevel = ZSTD_maxCLevel(); break;
        default: cLevel = ZSTD_CLEVEL_DEFAULT; break;
        }
        if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_compressionLevel, cLevel); ZSTD_isError(err))
        {
            PrintError(err);
            ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(pCCTX));
            return xerr::create_f<state, "Error setting compression level">();
        }

        // Set block size for block mode
        if (bBlockSizeIsOutputSize)
        {
            if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_targetCBlockSize, static_cast<int>(BlockSize)); ZSTD_isError(err))
            {
                PrintError(err);
                ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(pCCTX));
                return xerr::create_f<state, "Error setting target block size">();
            }
        }

        // Set source size hint
        if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_srcSizeHint, static_cast<int>(SourceUncompress.size())); ZSTD_isError(err))
        {
            PrintError(err);
            ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(pCCTX));
            return xerr::create_f<state, "Error setting source size hint">();
        }

        // Disable multi-threading for synchronous operation
        if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_nbWorkers, 0); ZSTD_isError(err))
        {
            PrintError(err);
            ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(pCCTX));
            return xerr::create_f<state, "Error disabling multi-threading">();
        }

        m_pCCTX = pCCTX;
        m_Src = SourceUncompress;
        m_BlockSize = BlockSize;
        m_bBlockSizeIsOutputSize = bBlockSizeIsOutputSize;
        m_Position = 0;

        return {};
    }

    //-------------------------------------------------------------------------------------------------------
    fixed_block_compress::~fixed_block_compress(void) noexcept
    {
        if (m_pCCTX) ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(m_pCCTX));
    }

    //-------------------------------------------------------------------------------------------------------
    xerr fixed_block_compress::Pack(std::uint64_t& CompressedSize, std::span<std::byte> Destination) noexcept
    {
        assert(m_pCCTX);
        assert(Destination.data());
        assert(m_Position <= m_Src.size());

        CompressedSize = 0;

        if (m_bBlockSizeIsOutputSize)
        {
            // Block mode: Ensure output buffer is at least input size
            if (Destination.size() < m_Src.size())
                return xerr::create_f<state, "Output buffer too small">();

            // Compress entire source as a single frame
            ZSTD_inBuffer in = { m_Src.data(), m_Src.size(), 0 };
            ZSTD_outBuffer out = { Destination.data(), Destination.size(), 0 };

            size_t rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out, &in, ZSTD_e_end);
            if (ZSTD_isError(rc))
            {
                PrintError(rc);
                return xerr::create_f<state, "Compression failed">();
            }

            CompressedSize = out.pos;
            if (CompressedSize >= m_Src.size())
                return xerr::create<state::INCOMPRESSIBLE, "Data incompressible">();

            m_Position = m_Src.size();
            return rc == 0 ? xerr{} : xerr::create<state::NOT_DONE, "Waiting to flush">();
        }

        // Streaming mode
        size_t totalOutput = 0;

        // Process input chunk if available
        if (m_Position < m_Src.size())
        {
            const auto        Left   = m_Src.size() - m_Position;
            const auto        InSize = Left > m_BlockSize ? m_BlockSize : Left;
            ZSTD_EndDirective end    = ZSTD_e_end;

            if (Destination.size() < InSize)
                return xerr::create_f<state, "Output buffer too small">();

            ZSTD_inBuffer  in  = { &m_Src[m_Position], InSize, 0 };
            ZSTD_outBuffer out = { Destination.data(), Destination.size(), 0 };

            size_t rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out, &in, end);
            if (ZSTD_isError(rc))
            {
                PrintError(rc);
                return xerr::create_f<state, "Compression failed">();
            }

            totalOutput += out.pos;
            m_Position  += in.pos;
            CompressedSize = totalOutput;

            if (totalOutput >= InSize)
                return xerr::create<state::INCOMPRESSIBLE, "Data incompressible">();
        }

        // Flush if all input processed and no error
        else if (m_Position == m_Src.size())
        {
            while (true)
            {
                ZSTD_inBuffer  in  = { nullptr, 0, 0 };
                ZSTD_outBuffer out = { Destination.data() + totalOutput, Destination.size() - totalOutput, 0 };

                size_t rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out, &in, ZSTD_e_flush);
                if (ZSTD_isError(rc))
                {
                    PrintError(rc);
                    return xerr::create_f<state, "Compression flush failed">();
                }
                totalOutput += out.pos;
                if (rc == 0) // Flush complete
                {
                    CompressedSize = totalOutput;
                    return xerr{};
                }
                if (out.pos == 0) // Buffer full or no more data
                    break;
            }

            CompressedSize = totalOutput;
            return xerr::create<state::NOT_DONE, "More data to flush">();
        }

        CompressedSize = totalOutput;
        return xerr::create<state::NOT_DONE, "More data to process">();
    }

    //-------------------------------------------------------------------------------------------------------
    xerr fixed_block_decompress::Init(bool bBlockIsOutputSize, std::uint64_t BlockSize) noexcept
    {
        assert(!m_pDCTX);
        assert(BlockSize > 0);

        auto pDCTX = ZSTD_createDCtx();
        if (!pDCTX) return xerr::create_f<state, "Failed to create decompression context">();

        // Reset context to ensure clean state
        if (ZSTD_isError(ZSTD_DCtx_reset(pDCTX, ZSTD_reset_session_and_parameters)))
        {
            ZSTD_freeDCtx(pDCTX);
            return xerr::create_f<state, "Error ZSTD_DCtx_reset">();
        }

        m_BlockSize = BlockSize;
        m_bBlockIsOutputSize = bBlockIsOutputSize;

        // Set max window size to the next power of 2 >= BlockSize, clamped to valid range
        const int windowLog = std::min(std::max(Log2IntRoundUp(static_cast<int>(BlockSize)), ZSTD_WINDOWLOG_MIN), ZSTD_WINDOWLOG_MAX);
        if (ZSTD_isError(ZSTD_DCtx_setParameter(pDCTX, ZSTD_d_windowLogMax, windowLog)))
        {
            PrintError(windowLog);
            ZSTD_freeDCtx(pDCTX);
            return xerr::create_f<state, "Error setting windowLogMax">();
        }

        m_pDCTX = pDCTX;
        m_Position = 0;
        m_OutputPosition = 0;

        return {};
    }

    //-------------------------------------------------------------------------------------------------------
    fixed_block_decompress::~fixed_block_decompress(void) noexcept
    {
        if (m_pDCTX) ZSTD_freeDCtx(static_cast<ZSTD_DCtx*>(m_pDCTX));
    }

    //-------------------------------------------------------------------------------------------------------
    xerr fixed_block_decompress::Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed) noexcept
    {
        assert(m_pDCTX);
        assert(!DestinationUncompress.empty());
        assert(!SourceCompressed.empty());

        if (DestinationUncompress.size() != m_BlockSize)
            return xerr::create_f<state, "Output buffer size must equal BlockSize">();

        DecompressSize = 0;

        if (m_bBlockIsOutputSize)
        {
            // Block mode: Decompress entire input as a single frame
            size_t rc = ZSTD_decompressDCtx(static_cast<ZSTD_DCtx*>(m_pDCTX), DestinationUncompress.data(), m_BlockSize, SourceCompressed.data(), SourceCompressed.size());
            if (ZSTD_isError(rc))
            {
                PrintError(rc);
                return xerr::create_f<state, "Decompression failed">();
            }

            DecompressSize = static_cast<std::uint32_t>(rc);
            m_Position += SourceCompressed.size();
            m_OutputPosition += DecompressSize;
            return {};
        }

        // Streaming mode
        ZSTD_inBuffer in = { SourceCompressed.data(), SourceCompressed.size(), 0 };
        ZSTD_outBuffer out = { DestinationUncompress.data(), m_BlockSize, 0 };

        size_t rc = ZSTD_decompressStream(static_cast<ZSTD_DCtx*>(m_pDCTX), &out, &in);
        if (ZSTD_isError(rc))
        {
            PrintError(rc);
            return xerr::create_f<state, "Decompression failed">();
        }

        DecompressSize = static_cast<std::uint32_t>(out.pos);
        m_Position += in.pos;
        m_OutputPosition += DecompressSize;
        return (in.pos < in.size || rc != 0) ? xerr::create<state::NOT_DONE, "More data to decompress">() : xerr{};
    }

    //-------------------------------------------------------------------------------------------------------
    xerr dynamic_block_compress::Init(bool bBlockSizeIsOutputSize, std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, level CompressionLevel) noexcept
    {
        assert(!m_pCCTX);
        assert(BlockSize > 0);
        assert(SourceUncompress.data());

        auto pCCTX = ZSTD_createCCtx();
        if (!pCCTX) return xerr::create_f<state, "Error ZSTD_createCCtx">();

        // Reset context to ensure clean state
        if (ZSTD_isError(ZSTD_CCtx_reset(pCCTX, ZSTD_reset_session_and_parameters)))
        {
            ZSTD_freeCCtx(pCCTX);
            return xerr::create_f<state, "Error ZSTD_CCtx_reset">();
        }

        // Set compression parameters
        int cLevel;
        switch (CompressionLevel)
        {
        case level::FAST: cLevel = 1; break;
        case level::MEDIUM: cLevel = ZSTD_CLEVEL_DEFAULT; break;
        case level::HIGH: cLevel = ZSTD_maxCLevel(); break;
        default: cLevel = ZSTD_CLEVEL_DEFAULT; break;
        }
        if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_compressionLevel, cLevel); ZSTD_isError(err))
        {
            PrintError(err);
            ZSTD_freeCCtx(pCCTX);
            return xerr::create_f<state, "Error setting compression level">();
        }

        // Set block size for block mode
        if ( bBlockSizeIsOutputSize == false )
        {
            if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_targetCBlockSize, static_cast<int>(BlockSize)); ZSTD_isError(err))
            {
                PrintError(err);
                ZSTD_freeCCtx(pCCTX);
                return xerr::create_f<state, "Error setting target block size">();
            }
        }

        // Set source size hint
        if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_srcSizeHint, static_cast<int>(SourceUncompress.size())); ZSTD_isError(err))
        {
            PrintError(err);
            ZSTD_freeCCtx(pCCTX);
            return xerr::create_f<state, "Error setting source size hint">();
        }

        // Disable multi-threading for synchronous operation
        if (auto err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_nbWorkers, 0); ZSTD_isError(err))
        {
            PrintError(err);
            ZSTD_freeCCtx(pCCTX);
            return xerr::create_f<state, "Error disabling multi-threading">();
        }

        // Make sure that the check sum is turn off
        if (auto Err = ZSTD_CCtx_setParameter(pCCTX, ZSTD_c_checksumFlag, 0); ZSTD_isError(Err))
        {
            PrintError(Err);
            ZSTD_freeCCtx(pCCTX);
            return xerr::create_f<state, "Error setting forceIgnoreChecksum">();
        }


        m_pCCTX = pCCTX;
        m_Src = SourceUncompress;
        m_BlockSize = BlockSize;
        m_bBlockSizeIsOutputSize = bBlockSizeIsOutputSize;
        m_Position = 0;

        return {};
    }

    //-------------------------------------------------------------------------------------------------------
    dynamic_block_compress::~dynamic_block_compress(void) noexcept
    {
        if (m_pCCTX) ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(m_pCCTX));
    }

    //-------------------------------------------------------------------------------------------------------

    xerr dynamic_block_compress::Pack(std::uint64_t& CompressedSize, std::span<std::byte> Destination ) noexcept
    {
        assert(m_pCCTX);
        assert(Destination.data());
        assert(m_Position <= m_Src.size());

        CompressedSize = 0;

        if (m_bBlockSizeIsOutputSize)
        {
            // Block mode: Ensure output buffer is at least input size
            if (Destination.size() < m_Src.size())
                return xerr::create_f<state, "Output buffer too small">();

            // Compress entire source as a single frame
            ZSTD_inBuffer in = { m_Src.data(), m_Src.size(), 0 };
            ZSTD_outBuffer out = { Destination.data(), Destination.size(), 0 };

            size_t rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out, &in, ZSTD_e_end);
            if (ZSTD_isError(rc))
            {
                PrintError(rc);
                return xerr::create_f<state, "Compression failed">();
            }

            CompressedSize = out.pos;
            if (CompressedSize >= m_Src.size())
                return xerr::create<state::INCOMPRESSIBLE, "Data incompressible">();

            m_Position = m_Src.size();
            return rc == 0 ? xerr{} : xerr::create<state::NOT_DONE, "Waiting to flush">();
        }

        // Streaming mode: Use binary search to find input size for compressed output ~ BlockSize
        size_t totalOutput = 0;
        if (m_Position < m_Src.size())
        {
            const auto          Left                 = m_Src.size() - m_Position;
            const std::size_t   MaxSizeAllowed       = std::min(Left, m_BlockSize);
            std::size_t         low                  = MaxSizeAllowed;
            std::size_t         high                 = std::min( Left, MaxSizeAllowed*4 );
            std::size_t         optimalInSize        = 0;
            std::size_t         optimalCompressed    = 0;

            ZSTD_inBuffer       in;
            ZSTD_outBuffer      out;
            bool                bLastWasOptimal      = false;

            // Maximun number of searching steps...
            int CountDown = 10;

            // Binary search for optimal input size
            while (low <= high && (--CountDown))
            {
                ZSTD_CCtx_reset(static_cast<ZSTD_CCtx*>(m_pCCTX), ZSTD_reset_session_only);

                size_t mid = low + (high - low) / 2;
                in  = ZSTD_inBuffer { &m_Src[m_Position],  mid,            0 };
                out = ZSTD_outBuffer{ Destination.data(),  MaxSizeAllowed, 0 };

                size_t rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out, &in, ZSTD_e_end);
                if (ZSTD_isError(rc))
                {
                    PrintError(rc);
                    return xerr::create_f<state, "Compression failed">();
                }
                
                if (out.pos < MaxSizeAllowed)
                {
                    // Include the flash
                    if (rc == 0)
                    {
                        // Frame is complete, no need for flush
                    }
                    else
                    {
                        auto in2 = ZSTD_inBuffer{ nullptr, 0, 0 };
                        auto out2 = ZSTD_outBuffer{ Destination.data() + out.pos, MaxSizeAllowed - out.pos, 0 };
                        rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out2, &in2, ZSTD_e_flush);

                        if (ZSTD_isError(rc))
                        {
                            PrintError(rc);
                            return xerr::create_f<state, "Compression failed">();
                        }

                        // The compression is telling us we can not fit...
                        if (rc) out.pos = MaxSizeAllowed * 2;

                        // Add it to the total
                        out.pos += out2.pos;
                    }
                }
                else
                {
                    // The compression is telling us we can not fit...
                    out.pos = MaxSizeAllowed * 2;
                }

                if (out.pos >= MaxSizeAllowed || (in.pos != in.size))
                {
                    high = mid - 1;
                    bLastWasOptimal = false;
                }
                else
                {
                    optimalInSize = mid;
                    optimalCompressed = out.pos;
                    low = mid + 1;
                    bLastWasOptimal = true;
                }
            }

            // Uncompressable...
            if (optimalInSize==0)
            {
                out.pos = 0;
                in.size = in.pos = MaxSizeAllowed;
            }
            // Compress with optimal input size and finalize frame
            else if ( bLastWasOptimal == false)
            {
                ZSTD_CCtx_reset(static_cast<ZSTD_CCtx*>(m_pCCTX), ZSTD_reset_session_only);

                in  = ZSTD_inBuffer{ &m_Src[m_Position],  optimalInSize,  0 };
                out = ZSTD_outBuffer{ Destination.data(), MaxSizeAllowed, 0 };

                size_t rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out, &in, ZSTD_e_end);
                if (ZSTD_isError(rc))
                {
                    PrintError(rc);
                    return xerr::create_f<state, "Compression failed">();
                }

                // Include the flash
                if (rc == 0)
                {
                    // Frame is complete, no need for flush
                }
                else
                {
                    auto in2 = ZSTD_inBuffer{ nullptr, 0, 0 };
                    auto out2 = ZSTD_outBuffer{ Destination.data() + out.pos, MaxSizeAllowed - out.pos, 0 };
                    rc = ZSTD_compressStream2(static_cast<ZSTD_CCtx*>(m_pCCTX), &out2, &in2, ZSTD_e_flush);

                    if (ZSTD_isError(rc))
                    {
                        PrintError(rc);
                        return xerr::create_f<state, "Compression failed">();
                    }

                    out.pos += out2.pos;
                }
            }

            totalOutput = out.pos;
            m_Position += in.pos;
            CompressedSize = totalOutput;

            if (in.pos == MaxSizeAllowed)
                return xerr::create<state::INCOMPRESSIBLE, "Data incompressible">();
        }

        // we are done...
        if (m_Position == m_Src.size()) 
            return {};

        return xerr::create<state::NOT_DONE, "More data to process">();
    }

    //-------------------------------------------------------------------------------------------------------

    xerr dynamic_block_decompress::Init(bool bBlockIsOutputSize, std::uint64_t BlockSize) noexcept
    {
        assert(!m_pDCTX);
        assert(BlockSize > 0);

        auto pDCTX = ZSTD_createDCtx();
        if (!pDCTX) return xerr::create_f<state, "Failed to create decompression context">();

        // Reset context to ensure clean state
        if (ZSTD_isError(ZSTD_DCtx_reset(pDCTX, ZSTD_reset_session_and_parameters)))
        {
            ZSTD_freeDCtx(pDCTX);
            return xerr::create_f<state, "Error ZSTD_DCtx_reset">();
        }

        m_BlockSize = BlockSize;
        m_bBlockIsOutputSize = bBlockIsOutputSize;

        // Set max window size to the next power of 2 >= BlockSize, clamped to valid range
        const int windowLog = std::min(std::max(Log2IntRoundUp(static_cast<int>(BlockSize)), ZSTD_WINDOWLOG_MIN), ZSTD_WINDOWLOG_MAX);
        if (ZSTD_isError(ZSTD_DCtx_setParameter(pDCTX, ZSTD_d_windowLogMax, windowLog)))
        {
            PrintError(windowLog);
            ZSTD_freeDCtx(pDCTX);
            return xerr::create_f<state, "Error setting windowLogMax">();
        }

        // Reduce buffering by ignoring checksums (optional, for performance)
        if (ZSTD_isError(ZSTD_DCtx_setParameter(pDCTX, ZSTD_d_forceIgnoreChecksum, 1)))
        {
            PrintError(1);
            ZSTD_freeDCtx(pDCTX);
            return xerr::create_f<state, "Error setting forceIgnoreChecksum">();
        }

        m_pDCTX = pDCTX;
        m_Position = 0;
        m_OutputPosition = 0;

        return {};
    }

    //-------------------------------------------------------------------------------------------------------
    dynamic_block_decompress::~dynamic_block_decompress(void) noexcept
    {
        if (m_pDCTX) ZSTD_freeDCtx(static_cast<ZSTD_DCtx*>(m_pDCTX));
    }


    //-------------------------------------------------------------------------------------------------------

    xerr dynamic_block_decompress::Unpack(std::uint32_t& DecompressSize, std::span<std::byte> DestinationUncompress, const std::span<const std::byte> SourceCompressed) noexcept
    {
        assert(m_pDCTX);
        assert(!DestinationUncompress.empty());
        assert(!SourceCompressed.empty());

        DecompressSize = 0;

        if (m_bBlockIsOutputSize)
        {
            // Block mode: Decompress entire input as a single frame
            size_t rc = ZSTD_decompressDCtx(static_cast<ZSTD_DCtx*>(m_pDCTX), DestinationUncompress.data(), DestinationUncompress.size(), SourceCompressed.data(), SourceCompressed.size());
            if (ZSTD_isError(rc))
            {
                PrintError(rc);
                return xerr::create_f<state, "Decompression failed">();
            }

            DecompressSize = static_cast<std::uint32_t>(rc);
            m_Position += SourceCompressed.size();
            m_OutputPosition += DecompressSize;
            return {};
        }

        // Streaming mode
        ZSTD_inBuffer  in  = { SourceCompressed.data(),      SourceCompressed.size(),      0 };
        ZSTD_outBuffer out = { DestinationUncompress.data(), DestinationUncompress.size(), 0 };

        size_t rc = ZSTD_decompressStream(static_cast<ZSTD_DCtx*>(m_pDCTX), &out, &in);
        if (ZSTD_isError(rc))
        {
            PrintError(rc);
            return xerr::create_f<state, "Decompression failed">();
        }

        DecompressSize = static_cast<std::uint32_t>(out.pos);
        m_Position       += in.pos;
        m_OutputPosition += DecompressSize;
        return (in.pos < in.size || rc != 0) ? xerr::create<state::NOT_DONE, "More data to decompress">() : xerr{};
    }
}
