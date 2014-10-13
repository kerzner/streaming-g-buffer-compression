// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.

#ifndef STREAMING_SKYBOX_TONE_MAP_FX
#define STREAMING_SKYBOX_TONE_MAP_FX

// Currently need GBuffer for albedo... used in ambient
#include "../GBuffer.hlsl"
#include "../FramebufferFlat.hlsl"

//--------------------------------------------------------------------------------------
// Tone mapping, post processing, skybox, etc.
// Rendered using skybox geometry, hence the naming
//--------------------------------------------------------------------------------------
TextureCube<float4> gSkyboxTexture : register(t5);
Texture2D<float4> gLitTexture : register(t7);


struct SkyboxVSOut
{
    float4 positionViewport : SV_Position;
    float3 skyboxCoord : skyboxCoord;
};

SkyboxVSOut SkyboxVS(GeometryVSIn input)
{
    SkyboxVSOut output;
    // NOTE: Don't translate skybox and make sure depth == 1 (no clipping)
    output.positionViewport = mul(float4(input.position, 0.0f), mCameraViewProj).xyww;
    output.skyboxCoord = input.position;
    return output;
}

float4 SkyboxPS(SkyboxVSOut input) : SV_Target0
{
    uint2 coords = input.positionViewport.xy;
    float4 lit = gLitTexture.Load(uint3(coords, 0)).xyzw;
    float skyboxWeight = lit.w;

    // If necessary, add skybox contribution
    [branch] if (skyboxWeight > 0.0f) {
        float3 skybox = gSkyboxTexture.Sample(gDiffuseSampler, input.skyboxCoord).xyz;
        // Tone map and accumulate
        lit += skyboxWeight * float4(skybox, 1.0f);
    }
    return float4(lit.xyz, 1.0f);
}
#endif // STREAMING_SKYBOX_TONE_MAP_FX
