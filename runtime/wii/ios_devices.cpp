// The host's IOS devices (wii/ios.h). For now every device the boot asks for answers its requests with success and is
// logged, until each gets its real answer.
#include "wii/ios.h"
#include "wii/ios_disc.h"
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
    } // namespace

    void SetDisc(const disc::WiiDisc& disc) { g_disc = &disc; }

    ios::Ios& Hosted()
    {
        static ios::Ios& hosted = []() -> ios::Ios& {
            auto* made = new ios::Ios;
            made->Serve("/dev/stm/immediate", [] { return std::make_unique<Silent>(); });
            made->Serve("/dev/stm/eventhook", [] { return std::make_unique<EventHook>(); });
            if (g_disc != nullptr)
                made->Serve("/dev/di", [] { return MakeDiscDrive(*g_disc); });
            return *made;
        }();
        return hosted;
    }
}
