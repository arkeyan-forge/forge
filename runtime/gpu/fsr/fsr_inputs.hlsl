// FSR's inputs from the game's scene, and the clears of FSR's own resources, for D3D12 (DXIL) and Vulkan (SPIR-V); keep
// in sync with fsr_inputs.metal. Register = binding, space = descriptor set, as plume maps them (gpu/fsr/backend.cpp).

// InputsCS: the camera's motion of each pixel and its depth, from the scene's depth target. The motion is the pixel's
// position without this frame's jitter, at its depth, taken back to the previous frame's clip space by the camera's
// reprojection: previous - current, in texture coordinates. The depth is the normalised device depth (0 near, 1 far),
// whatever the viewport's depth range.
[[vk::binding(0, 0)]] Texture2D<float> g_sceneDepth : register(t0, space0);
// Vulkan: the views' formats (fsr.cpp), not the HLSL types' (DXC would declare rg32f and rgba32f).
[[vk::binding(1, 0)]] [[vk::image_format("rg16f")]] RWTexture2D<float2> g_motion : register(u1, space0);
[[vk::binding(2, 0)]] RWTexture2D<float> g_depth : register(u2, space0);
// The object mask (gpu::UpscaleDesc::objectMotions): a pixel of object n (n / 255 in the mask) moves by object n's own
// reprojection, rows[n * 4] to rows[n * 4 + 3], instead of the camera's. rows[0].x: the frame before's mask is valid,
// rows[0].y: the reactive value of a pixel an object left (Reactive).
[[vk::binding(3, 0)]] Texture2D<float> g_objectMask : register(t3, space0);
struct ObjectMotions
{
    float4 rows[128 * 4];
};
[[vk::binding(4, 0)]] ConstantBuffer<ObjectMotions> g_objects : register(b4, space0);
[[vk::binding(5, 0)]] Texture2D<float> g_previousMask : register(t5, space0);
[[vk::binding(6, 0)]] [[vk::image_format("r8")]] RWTexture2D<float> g_reactive : register(u6, space0);

// FSR's reactive mask: where an object was in the frame before (all around the place the pixel's motion takes it to)
// and none is now (nor beside it): FSR's history there holds the object, the trail it leaves; the history is dropped.
// One pixel's margin either side: the masks' edges move with the jitter.
float Reactive(uint2 id, float2 motion, float2 size)
{
    if (g_objects.rows[0].x < 0.5)
        return 0.0;
    const int2 last = int2(size) - 1;
    const int2 previous = int2(floor(float2(id) + 0.5 + motion * size));
    if (any(previous < 0) || any(previous > last))
        return 0.0;
    const int2 taps[5] = { int2(0, 0), int2(1, 0), int2(-1, 0), int2(0, 1), int2(0, -1) };
    for (int i = 0; i < 5; i++)
        if (g_previousMask.Load(int3(clamp(previous + taps[i], int2(0, 0), last), 0)) < 0.5 / 255.0 ||
            g_objectMask.Load(int3(clamp(int2(id) + taps[i], int2(0, 0), last), 0)) > 0.5 / 255.0)
            return 0.0;
    return g_objects.rows[0].y;
}
// One push constant block for all three (SPIR-V allows one): the clears read their value from reprojection[0] and the
// mip's size from size.
struct Constants
{
    float4 reprojection[4]; // rows: previous clip = reprojection x current clip
    float2 size;
    float2 jitter;          // in clip space
    float depthMin, depthScale;
    float invertDepth; // 1: the depth written is 1 - ndc z (a reversed projection), so that 0 is near for FSR
    float padding;
};
[[vk::push_constant]] ConstantBuffer<Constants> g_inputs : register(b0, space1);

[numthreads(8, 8, 1)]
void InputsCS(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= uint(g_inputs.size.x) || id.y >= uint(g_inputs.size.y))
        return;
    float2 ndc = float2((float(id.x) + 0.5) / g_inputs.size.x * 2.0 - 1.0, 1.0 - (float(id.y) + 0.5) / g_inputs.size.y * 2.0);
    ndc -= g_inputs.jitter;
    // A depth outside the scene's range is another layer's (drawn in its own slice of the range, a HUD's portrait
    // [Alchemy]): it stays where it is, as the HUD does.
    const float unclamped = (g_sceneDepth.Load(int3(id.xy, 0)) - g_inputs.depthMin) * g_inputs.depthScale;
    const bool layer = unclamped < -1e-4 || unclamped > 1.0 + 1e-4;
    float z = saturate(unclamped);
    z = isfinite(z) ? z : 1.0;
    float4 current = float4(ndc, z, 1.0);
    const uint object = uint(g_objectMask.Load(int3(id.xy, 0)) * 255.0 + 0.5);
    const float4 r0 = object != 0 ? g_objects.rows[object * 4] : g_inputs.reprojection[0];
    const float4 r1 = object != 0 ? g_objects.rows[object * 4 + 1] : g_inputs.reprojection[1];
    const float4 r2 = object != 0 ? g_objects.rows[object * 4 + 2] : g_inputs.reprojection[2];
    const float4 r3 = object != 0 ? g_objects.rows[object * 4 + 3] : g_inputs.reprojection[3];
    float4 previous = float4(dot(r0, current), dot(r1, current), dot(r2, current), dot(r3, current));
    // Only finite motion within the picture reaches FSR (a NaN or an infinity would spread through its history: a
    // point near the camera plane, w close to 0, gives one).
    float2 delta = previous.w > 1e-6 ? previous.xy / previous.w - ndc : float2(0.0, 0.0);
    delta = all(isfinite(delta)) && !layer ? clamp(delta, -2.0, 2.0) : float2(0.0, 0.0);
    g_motion[id.xy] = delta * float2(0.5, -0.5);
    g_depth[id.xy] = g_inputs.invertDepth > 0.5 ? 1.0 - z : z;
    g_reactive[id.xy] = Reactive(id.xy, delta * float2(0.5, -0.5), g_inputs.size);
}

// ObjectMaskPS: the object mask's replays (gpu::detail::ReplayObjects); the blend writes the blend factor, the object's
// number / 255.
float4 ObjectMaskPS() : SV_Target0
{
    return float4(1.0, 1.0, 1.0, 1.0);
}

// ClearFloatCS, ClearUintCS: one value in every texel of a mip (FSR's clear jobs). A uint target takes the value's bits,
// as Vulkan's clears do.
[[vk::binding(0, 0)]] RWTexture2D<float4> g_clearFloat : register(u0, space0);
[[vk::binding(0, 0)]] RWTexture2D<uint> g_clearUint : register(u0, space0);

[numthreads(8, 8, 1)]
void ClearFloatCS(uint3 id : SV_DispatchThreadID)
{
    if (id.x < uint(g_inputs.size.x) && id.y < uint(g_inputs.size.y))
        g_clearFloat[id.xy] = g_inputs.reprojection[0];
}

[numthreads(8, 8, 1)]
void ClearUintCS(uint3 id : SV_DispatchThreadID)
{
    if (id.x < uint(g_inputs.size.x) && id.y < uint(g_inputs.size.y))
        g_clearUint[id.xy] = asuint(g_inputs.reprojection[0].x);
}

// SanitizeCS: a float scene's colour for FSR, without NaN or infinite values (FSR's history would spread them): NaN
// is 0, the rest clamped to half precision's range. Set 0 = { colour, its copy }.
[[vk::binding(0, 0)]] Texture2D<float4> g_sceneColor : register(t0, space0);
[[vk::binding(1, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> g_cleanColor : register(u1, space0);

[numthreads(8, 8, 1)]
void SanitizeCS(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= uint(g_inputs.size.x) || id.y >= uint(g_inputs.size.y))
        return;
    float4 c = g_sceneColor.Load(int3(id.xy, 0));
    g_cleanColor[id.xy] = clamp(any(isnan(c)) ? float4(0.0, 0.0, 0.0, 1.0) : c, 0.0, 65504.0);
}
