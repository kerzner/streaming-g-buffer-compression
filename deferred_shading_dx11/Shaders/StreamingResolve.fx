#ifndef STREAMINGRESOLVE_FX
#define STREAMINGRESOLVE_FX

#include "..\PerFrameConstants.hlsl"
#include "..\FullScreenTriangle.hlsl"
#include "StreamingStructs.h"
#include "StreamingBuffers.hlsl"
#include "BasicLoop.hlsl"
#include "DepthTests.hlsl"
#include "Debug.hlsl"
#include "..\GBuffer.hlsl"

TextureCube<float4> gSkyboxTexture : register(t6);

// TODO: This should be somewhere else...
struct SkyboxVSOut
{
    float4 positionViewport : SV_Position;
    float3 skyboxCoord : skyboxCoord;
};

float4 StreamingResolvePS(SkyboxVSOut input) : SV_TARGET
{
    // 1. Load indexing data for this pixel.
    // 2. Clear indexing data (to avoid a clear on the CPU)
    // 3. Compute weights for all surfaces.
    // 4. Shade each surface and weight appropriately.
    // 5. Average surface colors to final pixel color.

    // 1. Load indexing data for this pixel.
    uint nodeCount = GetNodeCount(input.positionViewport.xy);
    uint nodeIndex = GetNodeIndex(input.positionViewport.xy);
    uint nodeList  = GetNodeList(input.positionViewport.xy);
    uint discardedSamples = GetDiscardedSamples(input.positionViewport.xy);

    // 2. Clear indexing data (to avoid a clear on the CPU)
    gCountTexture[input.positionViewport.xy] = 0;
#if defined(STREAMING_USE_LIST_TEXTURE)
    SetNodeList(input.positionViewport.xy, 0);
#endif // defined(STREAMING_USE_LIST_TEXTURE)

    // For converting from our data structure to Lauritzen's
    GBuffer rawData;
    ShadeNode shade;
    SurfaceData surface;
    MergeNode merge;

    float weightSum = 0.0f;
    float4 lit = float4(0.0f, 0.0f, 0.0f, 0.0f);
    uint weights = 0;
    float4 output = float4(0.0f, 0.0f, 0.0f, 0.0f);

#if defined(STREAMING_DEBUG_OPTIONS)
    uint surfacesShaded = 0;
#endif // defined(STREAMING_DEBUG_OPTIONS)
    [branch] if (nodeCount > 0) {

#if MSAA_SAMPLES == 1

        uint closestIndex = 0;
        float closestDepth = 100000.00f; // far plane distance
        [unroll] for (uint i = 0; i < nodeCount; i++) {
            uint tempIndex = Get2BitsInByte(nodeList, i);
            MergeNode temp = GetMergeNode(input.positionViewport.xy, tempIndex);
            if (temp.zView < closestDepth) {
                closestIndex = tempIndex;
                closestDepth = temp.zView;
            }
        }

        merge = GetMergeNode(input.positionViewport.xy, closestIndex);
        shade = merge.shade;

        GetGBufferFromShadeAndMergeNodes(input.positionViewport.xy, merge, shade, rawData);
        surface = ComputeSurfaceDataFromGBufferData(input.positionViewport.xy, merge.zView, rawData);

        weightSum = 1.0f;
        lit = BasicLoop(surface);

#else // MSAA_SAMPLES > 1

        // 3. Compute weights for all surfaces.
        ResolveSurfaceWeights(input.positionViewport.xy, nodeList, nodeCount, weights, weightSum);

        // 4. Shade each surface and weight appropriately.
        [unroll] for (uint i = 0; i < nodeCount; i++) {
            uint tempIndex = Get2BitsInByte(nodeList, i);
            uint tempWeight = GetByteInUint(weights, tempIndex);
            [branch] if (tempWeight > 0) {
#if defined(STREAMING_DEBUG_OPTIONS)
                surfacesShaded++;
#endif // defined(STREAMING_DEBUG_OPTIONS)

                // Convert our data structure to Lauritzen's
                merge = GetMergeNode(input.positionViewport.xy, tempIndex);
                shade = merge.shade;
                GetGBufferFromShadeAndMergeNodes(input.positionViewport.xy, merge, shade, rawData);
                surface = ComputeSurfaceDataFromGBufferData(input.positionViewport.xy, merge.zView, rawData);

                output += BasicLoop(surface) * tempWeight;
            }
        }

#endif // MSAA_SAMPLES > 1

    }

#if defined(STREAMING_DEBUG_OPTIONS)
    uint2 coords = input.positionViewport.xy;
    uint merges = GetMergeCount(coords);
    uint discards = GetDiscardCount(coords);
    uint executions = GetExecutionCount(coords);
    SetNodeCount(coords, 0);
    SetNodeList(coords, 0);
    SetExecutionCount(coords, 0);
    SetMergeCount(coords, 0);
    SetDiscardCount(coords, 0);
    SetDiscardedSamples(coords, 0);

    switch (mUI.stats) {
    case VISUALIZE_MERGES:
        return float4(SequentialBlack5ColorMap(merges).xyz, 1.0f);
    case VISUALIZE_DISCARDS:
        return float4(SequentialRed3ColorMap(discardedSamples).xyz, 1.0f);
    case VISUALIZE_SHADING:
        return float4(SequentialSingleHueBlue4ColorMap(surfacesShaded).xyz, 1.0f);
    case VISUALIZE_EXECUTIONS:
        return float4(SequentialBlue9ColorMap(executions).xyz, 1.0f);
    default:
        break;
    }
#endif // defined (STREAMING_DEBUG_OPTION)

    // 5. Average surface colors to final pixel color.
    // If we discarded  samples, then compute the average using the sum of all samples that 
    // we have information about. Otherwise, use the total number of samples.
    if (discardedSamples > 0 && nodeCount != 0) {
        return float4(output.xyz / weightSum, 1.0f);
    } else {
        float skyboxWeight = MSAA_SAMPLES - weightSum;
        float4 skybox = gSkyboxTexture.Sample(gDiffuseSampler, input.skyboxCoord);
        return float4((output.xyz + skybox.xyz * skyboxWeight) / (float) MSAA_SAMPLES, 1.0f);
    }
}

#endif // STREAMINGRESOLVE_FX