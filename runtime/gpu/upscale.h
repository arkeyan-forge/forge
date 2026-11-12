#pragma once

// Temporal upscaling of a jittered picture with its depth and motion (docs/runtime/gpu/upscaling.md): AMD FSR 3.1 on
// every backend (gpu/fsr/); MetalFX's temporal scaler on Metal as an internal comparison (SKY_UPSCALER=metalfx, never
// offered to players). The motion is the camera's, from a reprojection matrix and the depth.
#include "gpu/gpu.h"

namespace gpu
{
    enum class Upscaler : uint8_t
    {
        Fsr3,
        MetalFx,
    };

    // Whether this device has `upscaler` (after Init).
    bool UpscalerAvailable(Upscaler upscaler);

    struct UpscaleDesc
    {
        Upscaler upscaler;
        Texture* color;  // the jittered picture
        Texture* depth;  // its depth target, the same size
        Texture* output; // the result, any size the upscaler supports; made with TextureUsage::Storage
        float jitter[2]; // the picture's sub-pixel offset this frame, in its pixels (x right, y down)
        // This frame's clip space to the previous frame's (row-major, column vectors): the camera's motion.
        double reprojection[16];
        // The draws agreed on the camera's motion (upscale::CameraVote). Not: reprojection is the identity, and frame
        // generation takes the frame as a camera cut (a cut changes the draws: nothing pairs).
        bool cameraFound;
        float depthMin, depthMax; // the viewport's depth range: window depth = min + ndc z x (max - min)
        bool depthReversed;       // ndc z is 1 at the near plane (FSR gets 1 - z: 0 near, 1 far)
        bool reset;               // no usable history (the first frame, or after frames without a scene)
        float sharpness;          // FSR's sharpening, 0 (off) to 1
        // The camera, for FSR's depth tests: near and far planes, vertical field of view in radians.
        float cameraNear, cameraFar, fovY;
        // game::Adapter::sceneMotionLayers: the scene's other motions (up to two more reprojections, of draws moving apart
        // from the camera's); the inputs pass gives each pixel the one that finds its colour in the previous frame, and
        // a depth outside the scene's range (the HUD's portrait and its mask) the nearest depth and no motion.
        bool motionLayers;
        // game::Adapter::frameGenerationCutShift (0: 0.5).
        float cutShift;
        uint32_t otherMotionCount;
        double otherMotions[2][16];
        // Per-object motion (upscale::CameraVote's objects): the frame's draws numbered 1 to objectCount
        // (DrawDesc::object) are drawn again into an object mask over `depth`, and a pixel of object n moves by
        // objectMotions[n * 16] to [n * 16 + 15] (rows, as `reprojection`) instead of the camera's. Upscale copies them.
        uint32_t objectCount;
        const float* objectMotions;
    };

    // The object mask's numbers: 1 to 127 (R8, 0 is the camera's).
    constexpr uint32_t MaxUpscaleObjects = 127;

    // Queued like the draws: runs after what was recorded before and before what is recorded after.
    void Upscale(const UpscaleDesc& desc);
}
