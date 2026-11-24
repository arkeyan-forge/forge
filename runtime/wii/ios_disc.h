#pragma once

// /dev/di, the disc drive (ios_disc.cpp), reading the player's disc image.
#include "wii/ios.h"
#include <memory>

namespace disc
{
    class WiiDisc;
}

namespace wii
{
    std::unique_ptr<ios::Device> MakeDiscDrive(const disc::WiiDisc& disc);
}
