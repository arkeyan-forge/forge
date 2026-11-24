// The host's IOS devices (wii/ios.h): the state module, the disc drive, the NAND and the title service.
#include "wii/ios.h"
#include "wii/ios_disc.h"
#include "wii/ios_nand.h"
#include "wii/nand.h"
#include "wii/os.h"

namespace wii
{
    namespace
    {
        class Silent : public ios::Device
        {
        public:
            int32_t Ioctl(uint32_t, ios::Buffer, ios::Buffer) override { return ios::Ok; }
            int32_t Ioctlv(uint32_t, const std::vector<ios::Buffer>&, const std::vector<ios::Buffer>&) override
            {
                return ios::Ok;
            }
        };

        // /dev/stm/eventhook: its one request waits for the power or reset button, which never comes here.
        class EventHook : public ios::Device
        {
        public:
            int32_t Ioctl(uint32_t, ios::Buffer, ios::Buffer) override { return Pending; }
        };
        const disc::WiiDisc* g_disc = nullptr;
        nand::FileSystem* g_nand = nullptr;
        uint64_t g_title = 0;
    } // namespace

    void SetDisc(const disc::WiiDisc& disc) { g_disc = &disc; }

    void SetNand(nand::FileSystem& nand, uint64_t titleId)
    {
        g_nand = &nand;
        g_title = titleId;
    }

    ios::Ios& Hosted()
    {
        static ios::Ios& hosted = []() -> ios::Ios& {
            auto* made = new ios::Ios;
            made->Serve("/dev/stm/immediate", [] { return std::make_unique<Silent>(); });
            made->Serve("/dev/stm/eventhook", [] { return std::make_unique<EventHook>(); });
            if (g_disc != nullptr)
                made->Serve("/dev/di", [] { return MakeDiscDrive(*g_disc); });
            if (g_nand != nullptr)
            {
                made->Serve("/dev/fs", [] { return MakeFileSystemDevice(*g_nand); });
                made->ServeFiles([](const std::string& path) { return MakeNandFile(*g_nand, path); });
                made->Serve("/dev/es", [] { return MakeTitleService(g_title); });
            }
            return *made;
        }();
        return hosted;
    }
}
