// /dev/di, the disc drive, from the player's disc image (docs/runtime/wii/ios.md). Every request is a 0x20-byte block
// whose first word holds the command in its top byte; the reply is DI's own code, 1 for success. From the public
// description of the device: the commands the SDK's DVD driver sends.
#include "wii/ios_disc.h"
#include "disc/wii_disc.h"
#include "memory.h"
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace wii
{
    namespace
    {
        constexpr int32_t Success = 1, Failure = 2;

        uint32_t Get32(uint32_t address) { return g_memory.Read<uint32_t>(address); }
        void Put32(uint32_t address, uint32_t value) { g_memory.Write<uint32_t>(address, value); }

        bool Fits(ios::Buffer buffer, uint32_t size) { return buffer.address != 0 && buffer.size >= size; }

        class DiscDrive : public ios::Device
        {
        public:
            explicit DiscDrive(const disc::WiiDisc& disc) : m_disc(disc) {}

            int32_t Ioctl(uint32_t command, ios::Buffer in, ios::Buffer out) override
            {
                const uint32_t length = in.size >= 8 ? Get32(in.address + 4) : 0;
                const uint64_t offset = in.size >= 12 ? uint64_t(Get32(in.address + 8)) << 2 : 0;
                switch (command)
                {
                case 0x12: // Inquiry: the drive's revision and date
                    if (Fits(out, 0x20))
                    {
                        memset(g_memory.Translate(out.address), 0, 0x20);
                        Put32(out.address, 0x00020002);
                        Put32(out.address + 4, 0x20080714);
                    }
                    return Success;
                case 0x70: // ReadDiskID: the disc's first 0x20 bytes
                    return Fits(out, 0x20) && m_disc.ReadRaw(0, g_memory.Translate(out.address), 0x20) ? Success
                                                                                                       : Failure;
                case 0x71: // Read: the opened partition's data, its offset in words
                    if (!Fits(out, length) || !m_disc.Read(offset, g_memory.Translate(out.address), length))
                        return Failure;
                    m_reads++;
                    m_bytes += length;
                    return Success;
                case 0x8D: // UnencryptedRead: the disc itself (its header, the partition table)
                    return Fits(out, length) && m_disc.ReadRaw(offset, g_memory.Translate(out.address), length)
                               ? Success
                               : Failure;
                case 0x7A: // GetCoverStatus: a disc in, the cover shut
                case 0x88: // GetCoverRegister
                case 0x95: // GetStatusRegister: nothing pending
                case 0x96: // GetControlRegister
                case 0xE0: // RequestError: none
                    if (Fits(out, 4))
                        Put32(out.address, command == 0x7A ? 2 : 0);
                    return Success;
                case 0x86: // ClearCoverInterrupt
                case 0x8A: // Reset
                case 0x8C: // ClosePartition
                case 0xE3: // SetMaximumRotation
                    return Success;
                default: Unknown(command); return Success;
                }
            }

            int32_t Ioctlv(
                uint32_t command, const std::vector<ios::Buffer>& in, const std::vector<ios::Buffer>& out) override
            {
                if (command != 0x8B) // OpenPartition is the drive's only vectored request
                {
                    Unknown(command);
                    return Success;
                }
                // OpenPartition: the request's partition (its offset in words), the ticket and certificates; back,
                // the partition's title metadata and ES's error. The game partition is the one the host reads.
                if (in.empty() || in[0].size < 8 || (uint64_t(Get32(in[0].address + 4)) << 2) != m_disc.GamePartition())
                    return Failure;
                std::vector<uint8_t> tmd;
                if (out.size() >= 1 && out[0].address != 0 && m_disc.TitleMetadata(tmd))
                    memcpy(g_memory.Translate(out[0].address), tmd.data(), std::min<size_t>(tmd.size(), out[0].size));
                if (out.size() >= 2 && Fits(out[1], 4))
                    Put32(out[1].address, 0);
                return Success;
            }

            int32_t Close() override
            {
                if (m_reads != 0)
                    fprintf(stderr, "[di] %llu reads, %llu bytes\n", (unsigned long long)m_reads,
                        (unsigned long long)m_bytes);
                return ios::Ok;
            }

        private:
            const disc::WiiDisc& m_disc;
            uint64_t m_reads = 0, m_bytes = 0;

            static void Unknown(uint32_t command)
            {
                static std::set<uint32_t> logged;
                if (logged.insert(command).second)
                    fprintf(stderr, "[di] command 0x%02X: not known, answered as done\n", command);
            }
        };
    } // namespace

    std::unique_ptr<ios::Device> MakeDiscDrive(const disc::WiiDisc& disc) { return std::make_unique<DiscDrive>(disc); }
}
