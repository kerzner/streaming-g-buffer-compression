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

#ifndef PER_FRAME_CONSTANTS_HLSL
#define PER_FRAME_CONSTANTS_HLSL

#include "Shaders/StreamingDefines.h"

struct UIConstants
{
    uint lightingOnly;
    uint faceNormals;
    uint visualizeLightCount;
    uint visualizePerSampleShading;
    uint lightCullTechnique;
#if defined(STREAMING_DEBUG_OPTIONS)
    int executionCount;
    float mergeCosTheta;
    int stats;
    int mergeMetric;
    int averageNormals;
    int averageShade;
    int derivatives;
    int depth;
    uint compareCoverage;
    uint compareDepth;
    uint compareNormals;
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

cbuffer PerFrameConstants : register(b0)
{
    float4x4 mCameraWorldViewProj;
    float4x4 mCameraWorldView;
    float4x4 mCameraViewProj;
    float4x4 mCameraProj;
    float4 mCameraNearFar;
    uint4 mFramebufferDimensions;
    UIConstants mUI;
};

#endif // PER_FRAME_CONSTANTS_HLSL
