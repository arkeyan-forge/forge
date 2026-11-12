// Checks the DXIL containers tools/shaders/extract_dxil.py extracted from the shader cache with Microsoft's validator
// (dxil.dll, from XenosRecomp's dxc-bin): each must validate, and the validator, signing it again, must write the same
// hash as XenosRecomp did (D3D12 rejects a container whose hash does not match). A Windows program: build it with
// llvm-mingw and run it under Wine on the Mac (tools/shaders/check_dxil.sh).
#include <windows.h>
#include <dxcapi.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    // As in dxcapi.h (llvm-mingw has no __uuidof for its interfaces).
    constexpr GUID IID_Blob = { 0x8BA5FB08, 0x5195, 0x40e2, { 0xAC, 0x58, 0x0D, 0x98, 0x9C, 0x3A, 0x01, 0x02 } };
    constexpr GUID IID_Validator = { 0xA6E82BD2, 0x1FD7, 0x4826, { 0x98, 0x11, 0x28, 0x57, 0xE7, 0x97, 0xF4, 0x9A } };

    // The containers are handed to the validator through this minimal blob.
    struct Blob final : IDxcBlob
    {
        std::vector<uint8_t> data;
        ULONG references = 1;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
        {
            if (riid == IID_IUnknown || riid == IID_Blob)
            {
                *object = static_cast<IDxcBlob*>(this);
                AddRef();
                return S_OK;
            }
            *object = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override { return --references; } // on the stack
        LPVOID STDMETHODCALLTYPE GetBufferPointer() override { return data.data(); }
        SIZE_T STDMETHODCALLTYPE GetBufferSize() override { return data.size(); }
    };
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: dxil_check <dxil.dll> <containers file>\n");
        return 2;
    }
    HMODULE library = LoadLibraryA(argv[1]);
    auto create = library != nullptr
                      ? reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(library, "DxcCreateInstance"))
                      : nullptr;
    IDxcValidator* validator = nullptr;
    if (create == nullptr || FAILED(create(CLSID_DxcValidator, IID_Validator, reinterpret_cast<void**>(&validator))))
    {
        fprintf(stderr, "cannot load the validator from %s\n", argv[1]);
        return 2;
    }

    FILE* file = fopen(argv[2], "rb");
    if (file == nullptr)
    {
        fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    uint32_t checked = 0, invalid = 0, wrongHash = 0;
    for (;;)
    {
        uint64_t shader;
        uint32_t variant, size;
        if (fread(&shader, 8, 1, file) != 1 || fread(&variant, 4, 1, file) != 1 || fread(&size, 4, 1, file) != 1)
            break;
        Blob blob;
        blob.data.resize(size);
        if (fread(blob.data.data(), 1, size, file) != size)
            break;
        uint8_t hash[16];
        memcpy(hash, blob.data.data() + 4, 16);

        IDxcOperationResult* result = nullptr;
        HRESULT status = E_FAIL;
        if (SUCCEEDED(validator->Validate(&blob, DxcValidatorFlags_InPlaceEdit, &result)) && result != nullptr)
            result->GetStatus(&status);
        checked++;
        if (FAILED(status))
        {
            invalid++;
            IDxcBlobEncoding* errors = nullptr;
            if (result != nullptr && SUCCEEDED(result->GetErrorBuffer(&errors)) && errors != nullptr)
            {
                printf("%016llX variant %u: invalid\n%.*s\n", (unsigned long long)shader, variant,
                    int(errors->GetBufferSize()), static_cast<const char*>(errors->GetBufferPointer()));
                errors->Release();
            }
            else
            {
                printf("%016llX variant %u: invalid\n", (unsigned long long)shader, variant);
            }
        }
        else if (memcmp(hash, blob.data.data() + 4, 16) != 0)
        {
            wrongHash++;
            printf("%016llX variant %u: the validator writes another hash\n", (unsigned long long)shader, variant);
        }
        if (result != nullptr)
            result->Release();
    }
    fclose(file);
    printf("%u containers: %u invalid, %u with a wrong hash\n", checked, invalid, wrongHash);
    return invalid + wrongHash != 0 || checked == 0;
}
