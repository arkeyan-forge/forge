// Stand-in for DirectX-Headers' dxguids.h, which D3D12MemoryAllocator includes when built with MinGW (plume's
// D3D12 backend). The mingw-w64 headers already declare the D3D12/DXGI interface GUIDs (__CRT_UUID_DECL), so
// __uuidof works without it.
#pragma once
