#include "wii/settings.h"
#include <cstring>

namespace wii
{
    namespace
    {
        enum Type : uint8_t
        {
            LongArray = 1, // the length on 16 bits
            ShortArray = 2,
            Byte = 3,
            Long = 5,
        };

        struct Writer
        {
            std::vector<uint8_t> items;
            std::vector<uint16_t> offsets;

            void Begin(Type type, const char* name)
            {
                offsets.push_back(uint16_t(items.size()));
                const size_t length = strlen(name);
                items.push_back(uint8_t(type << 5 | (length - 1)));
                items.insert(items.end(), name, name + length);
            }
            void PutByte(const char* name, uint8_t value)
            {
                Begin(Byte, name);
                items.push_back(value);
            }
            void PutLong(const char* name, uint32_t value)
            {
                Begin(Long, name);
                for (int shift = 24; shift >= 0; shift -= 8)
                    items.push_back(uint8_t(value >> shift));
            }
            void PutArray(const char* name, const std::vector<uint8_t>& value)
            {
                const size_t stored = value.size() - 1;
                if (value.size() > 0x100)
                {
                    Begin(LongArray, name);
                    items.push_back(uint8_t(stored >> 8));
                }
                else
                    Begin(ShortArray, name);
                items.push_back(uint8_t(stored));
                items.insert(items.end(), value.begin(), value.end());
            }
        };

        constexpr uint32_t SettingKey = 0x73B5DBFA;
    } // namespace

    std::vector<uint8_t> MakeSysconf(const ConsoleSettings& settings)
    {
        Writer writer;
        writer.PutLong("IPL.CB", 0); // the clock's bias: the host's clock is the real time already
        writer.PutByte("IPL.AR", settings.wide ? 1 : 0);
        writer.PutByte("IPL.DH", 0);
        writer.PutByte("IPL.E60", settings.euRgb60 ? 1 : 0);
        writer.PutArray("IPL.IDL", { 0, 0 }); // no standby connection
        writer.PutByte("IPL.LNG", settings.language);
        writer.PutByte("IPL.PGS", settings.progressive ? 1 : 0);
        writer.PutByte("IPL.SSV", 0);
        writer.PutByte("IPL.SND", settings.sound);
        writer.PutArray("BT.DINF", std::vector<uint8_t>(0x461)); // no remote paired yet
        writer.PutArray("BT.CDIF", std::vector<uint8_t>(0x205));
        writer.PutLong("BT.SENS", 3);
        writer.PutByte("BT.SPKV", 0x58);
        writer.PutByte("BT.MOT", 1);
        writer.PutByte("BT.BAR", 0); // the sensor bar under the screen

        std::vector<uint8_t> file(SysconfSize);
        memcpy(file.data(), "SCv0", 4);
        const size_t count = writer.offsets.size();
        const size_t first = 6 + 2 * (count + 1);
        file[4] = uint8_t(count >> 8);
        file[5] = uint8_t(count);
        writer.offsets.push_back(uint16_t(writer.items.size()));
        for (size_t i = 0; i <= count; i++)
        {
            const size_t offset = first + writer.offsets[i];
            file[6 + 2 * i] = uint8_t(offset >> 8);
            file[7 + 2 * i] = uint8_t(offset);
        }
        memcpy(file.data() + first, writer.items.data(), writer.items.size());
        memcpy(file.data() + SysconfSize - 4, "SCed", 4);
        return file;
    }

    std::vector<uint8_t> MakeSettingText(char region)
    {
        const char* area = "EUR";
        const char* model = "RVL-001(EUR)";
        const char* game = "EU";
        const char* video = "PAL";
        const char* code = "LEH";
        switch (region)
        {
        case 'J': area = "JPN", model = "RVL-001(JPN)", game = "JP", video = "NTSC", code = "LJH"; break;
        case 'E': area = "USA", model = "RVL-001(USA)", game = "US", video = "NTSC", code = "LU"; break;
        case 'K': area = "KOR", model = "RVL-001(KOR)", game = "KR", video = "NTSC", code = "LKH"; break;
        default: break;
        }
        const std::string text = std::string("AREA=") + area + "\r\nMODEL=" + model +
                                 "\r\nDVD=0\r\nMPCH=0x7FFE\r\nCODE=" + code + "\r\nSERNO=000000000\r\nVIDEO=" + video +
                                 "\r\nGAME=" + game + "\r\n";
        std::vector<uint8_t> out(SettingTextSize);
        uint32_t key = SettingKey;
        for (size_t i = 0; i < out.size(); i++)
        {
            const uint8_t plain = i < text.size() ? uint8_t(text[i]) : 0;
            out[i] = uint8_t(plain ^ key);
            key = key << 1 | key >> 31;
        }
        return out;
    }

    std::string DecryptSettingText(const std::vector<uint8_t>& encrypted)
    {
        std::string text;
        uint32_t key = SettingKey;
        for (uint8_t byte : encrypted)
        {
            const char plain = char(byte ^ uint8_t(key));
            if (plain == 0)
                break;
            text += plain;
            key = key << 1 | key >> 31;
        }
        return text;
    }
}
