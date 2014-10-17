#ifndef STREAMINGBUFFERS_HLSL
#define STREAMINGBUFFERS_HLSL

#include "StreamingStructs.h"
#include "UintByteArray.hlsl"
#include "D3DX_DXGIFormatConvert.inl"

RWStructuredBuffer<MergeNodePacked> gMergeBuffer : register(u3);
RWTexture2D<uint> gCountTexture : register(u4);

#if defined(STREAMING_DEBUG_OPTIONS)
RWStructuredBuffer<PixelStats> gPixelStats : register(u5);
#endif // defined(STREAMING_DEBUG_OPTIONS)

#if defined (STREAMING_USE_LIST_TEXTURE)
RWTexture2D<uint> gListTexture : register(u5);
#endif // defined(STREAMING_USE_LIST_TEXTURE)

ShadeNode UnpackShadeNode(in ShadeNodePacked packed);
ShadeNodePacked PackShadeNode(in ShadeNode shade);

uint GetNodeIndex(uint2 coords)
{
#if defined(STREAMING_TILED_ADDRESSING)
#define AOIT_TILE_LOGX 0U
#define AOIT_TILE_LOGY 1U
     const uint surfaceWidth = mFramebufferDimensions.x >> AOIT_TILE_LOGX;
     uint2 tileAddr2D  = uint2(coords[0] >> AOIT_TILE_LOGX, coords[1] >> AOIT_TILE_LOGY);
     uint  tileAddr1D  = (tileAddr2D[0] + surfaceWidth * tileAddr2D[1]) << (AOIT_TILE_LOGX + AOIT_TILE_LOGY);
     uint2 pixelAddr2D = uint2(coords[0] & ((1 << AOIT_TILE_LOGX) - 1), coords[1] & ((1 << AOIT_TILE_LOGY) - 1));
     uint  pixelAddr1D = (pixelAddr2D[1] << AOIT_TILE_LOGX) | pixelAddr2D[0];
     return tileAddr1D | pixelAddr1D;
#else // !defined(STREAMING_TILED_ADDRESSING)
    return (coords.x + mFramebufferDimensions.x * coords.y) * STREAMING_MAX_SURFACES_PER_PIXEL;
#endif // !defined(STREAMING_TILED_ADDRESSING)
}

uint GetNodeIndex(uint2 coords, uint index)
{
    return (mFramebufferDimensions.x * mFramebufferDimensions.y * index) + GetNodeIndex(coords);
}

uint GetNodeCountIndex(uint2 coords)
{
    return (coords.x + mFramebufferDimensions.x * coords.y);
}

uint GetNodeCount(uint2 coords)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    uint nodeCount = gPixelStats[GetNodeCountIndex(coords)].nodeCount;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    uint nodeCount = gCountTexture[coords] & 0x3;
#endif // !defined(STREAMING_DEBUG_OPTIONS)
    return min(nodeCount, STREAMING_MAX_SURFACES_PER_PIXEL);
}

void SetNodeCount(uint2 coords, uint value)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    gPixelStats[GetNodeCountIndex(coords)].nodeCount = value;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    uint count = gCountTexture[coords];
    count = count & ~(0x3);
    count = count | (value & 0x3);
    gCountTexture[coords] = count;
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}

MergeNode GetMergeNode(uint2 coords, in uint index)
{
    MergeNodePacked packed = gMergeBuffer[GetNodeIndex(coords, index)];
    MergeNode merge;

    merge.normal = D3DX_R16G16_FLOAT_to_FLOAT2(packed.normal);
    merge.zViewDerivatives = D3DX_R16G16_FLOAT_to_FLOAT2(packed.zViewDerivatives);
    merge.zView = packed.zView;

    merge.coverage = packed.coverage & 0xFFFF; // only take coverage data from packed.coverage
    merge.shade.specular.y = f16tof32(packed.coverage >> 16);

    float4 temp = D3DX_R8G8B8A8_UNORM_to_FLOAT4(packed.albedo);
    merge.shade.albedo = float4(temp.xyz, 1.0f);
    merge.shade.specular.x = temp.w;

#if defined(STREAMING_DEBUG_OPTIONS)
    merge.depthTestedCoverage = packed.depthTestedCoverage;
    merge.shadeIndex = packed.shadeIndex;
#endif // defined(STREAMING_DEBUG_OPTIONS)

    return merge;
}

void SetMergeNode(in uint2 coords, in uint index, in MergeNode merge)
{
    MergeNodePacked packed;

    packed.normal = D3DX_FLOAT2_to_R16G16_FLOAT(merge.normal);
    packed.zViewDerivatives = D3DX_FLOAT2_to_R16G16_FLOAT(merge.zViewDerivatives);
    packed.zView = merge.zView;
    packed.coverage = merge.coverage;

    packed.albedo = D3DX_FLOAT4_to_R8G8B8A8_UNORM(float4(merge.shade.albedo.xyz, merge.shade.specular.x));
    packed.coverage = packed.coverage | (asuint(f32tof16(merge.shade.specular.y)) << 16);

#if defined(STREAMING_DEBUG_OPTIONS)
    packed.depthTestedCoverage = merge.depthTestedCoverage;
    packed.shadeIndex = merge.shadeIndex;
#endif // defined(STREAMING_DEBUG_OPTIONS)

    gMergeBuffer[GetNodeIndex(coords, index)] = packed;
}

uint LoadMergeNodes(uint2 coords, out MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL+1])
{
    uint index = GetNodeIndex(coords);
    uint count = GetNodeCount(coords);
    [unroll] for (uint i = 0; i < STREAMING_MAX_SURFACES_PER_PIXEL; i++) {
        [flatten] if (i < count) {
            mergeNodes[i] = GetMergeNode(coords, i);
        } else {
            mergeNodes[i] = GetEmptyMergeNode();
        }
    }
    mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL] = GetEmptyMergeNode();
    return count;
}

uint LoadMergeNodes(uint2 coords, out MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL])
{
    uint index = GetNodeIndex(coords);
    uint count = GetNodeCount(coords);
    [unroll] for (uint i = 0; i < STREAMING_MAX_SURFACES_PER_PIXEL; i++) {
        [flatten] if (i < count) {
            mergeNodes[i] = GetMergeNode(coords, i);
        } else {
            mergeNodes[i] = GetEmptyMergeNode();
        }
    }
    return count;
}

void WriteMergeNodes(uint2 coords, in uint nodeCount, in MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL])
{
    uint index = GetNodeIndex(coords);
    nodeCount = min(nodeCount, STREAMING_MAX_SURFACES_PER_PIXEL);
    [unroll] for (uint i = 0; i < nodeCount; i++) {
        SetMergeNode(coords, i, mergeNodes[i]);
    }
}

void WriteMergeNodes(uint2 coords, in uint nodeCount, in MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL+1])
{
    uint index = GetNodeIndex(coords);
    nodeCount = min(nodeCount, STREAMING_MAX_SURFACES_PER_PIXEL);
    [unroll] for (uint i = 0; i < nodeCount; i++) {
        SetMergeNode(coords, i, mergeNodes[i]);
    }

}

void WriteValidMergeNodes(uint2 coords, in uint nodeCount, in MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL+1])
{
    uint index = GetNodeIndex(coords);
    nodeCount = min(nodeCount, STREAMING_MAX_SURFACES_PER_PIXEL+1);
    uint count = 0;
    uint i = 0;
    [unroll] while (i < STREAMING_MAX_SURFACES_PER_PIXEL+1 && count < nodeCount) {
        if (GetDepthTestedCoverage(mergeNodes[i]) != 0) {
            SetMergeNode(coords, count, mergeNodes[i]);
            count++;
        }
        i++;
    }
}

#if defined(STREAMING_DEBUG_OPTIONS)
uint GetExecutionCount(in uint2 coords)
{
    return gPixelStats[GetNodeCountIndex(coords)].executions;
}

void SetExecutionCount(in uint2 coords, in uint executions)
{
    gPixelStats[GetNodeCountIndex(coords)].executions = executions;
}

void SetLastMerge(in uint2 coords, in MergeNode merge)
{
    gPixelStats[GetNodeCountIndex(coords)].lastMerge = merge;
}

void SetLastShade(in uint2 coords, in ShadeNode shade)
{
    gPixelStats[GetNodeCountIndex(coords)].lastShade = shade;
}
#endif // defined(STREAMING_DEBUG_OPTIONS)

void SetDiscardedSamples(in uint2 coords, in uint discardedSamples)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    gPixelStats[GetNodeCountIndex(coords)].discardedSamples = discardedSamples;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    uint temp = gCountTexture[coords];
    temp &= ~(0x1 << 9);
    temp |= (discardedSamples << 9);
    gCountTexture[coords] = temp;
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}

uint GetDiscardedSamples(in uint2 coords)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    return gPixelStats[GetNodeCountIndex(coords)].discardedSamples;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    return (gCountTexture[coords] & (0x1 << 9)) >> 9;
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}

#if defined(STREAMING_DEBUG_OPTIONS)

void SetMergeCount(in uint2 coords, in uint merges)
{
    gPixelStats[GetNodeCountIndex(coords)].merges = merges;
}

uint GetMergeCount(in uint2 coords)
{
    return gPixelStats[GetNodeCountIndex(coords)].merges;
}

void IncrementMergeCount(in uint2 coords)
{
    gPixelStats[GetNodeCountIndex(coords)].merges++;
}

void SetDiscardCount(in uint2 coords, in uint discards)
{
    gPixelStats[GetNodeCountIndex(coords)].discards = discards;
}

uint GetDiscardCount(in uint2 coords)
{
    return gPixelStats[GetNodeCountIndex(coords)].discards;
}

void IncrementDiscardCount(in uint2 coords)
{
    gPixelStats[GetNodeCountIndex(coords)].discards++;
}
#endif // defined(STREAMING_DEBUG_OPTIONS

uint GetNodeList(uint2 coords)
{
#if defined(STREAMING_USE_LIST_TEXTURE)
    return gListTexture[coords];
#else // !defined(STREAMING_USE_LIST_TEXTURE)
#if defined(STREAMING_DEBUG_OPTIONS)
    return gPixelStats[GetNodeCountIndex(coords)].nodeList;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    return (gCountTexture[coords] & 0xFC) >> 2;
#endif // !defined(STREAMING_DEBUG_OPTIONS)
#endif // !defined(STREAMING_LIST_TEXTURE)
}

void SetNodeList(uint2 coords, in uint nodeList)
{
#if defined(STREAMING_USE_LIST_TEXTURE)
    gListTexture[coords] = nodeList;
#else // !defined(STREAMING_USE_LIST_TEXTURE)
#if defined(STREAMING_DEBUG_OPTIONS)
    gPixelStats[GetNodeCountIndex(coords)].nodeList = nodeList;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    uint temp = gCountTexture[coords];
    temp &= ~0xFC;
    temp |= ((nodeList & 0x3F) << 2);
    gCountTexture[coords] = temp;
#endif // !defined(STREAMING_DEBUG_OPTIONS)
#endif // !defined(STREAMING_USE_LIST_TEXTURE)
}

ShadeNodePacked PackShadeNode(in ShadeNode shade)
{
    ShadeNodePacked packed;
    packed.albedo = D3DX_FLOAT4_to_R8G8B8A8_UNORM(float4(shade.albedo.xyz, 0.0f));
    packed.specular = D3DX_FLOAT2_to_R16G16_FLOAT(shade.specular);
    return packed;
}

ShadeNode UnpackShadeNode(in ShadeNodePacked packed)
{
    ShadeNode shade;
    shade.albedo = D3DX_R8G8B8A8_UNORM_to_FLOAT4(packed.albedo);
    shade.specular = D3DX_R16G16_FLOAT_to_FLOAT2(packed.specular);
    return shade;
}
#endif // STREAMINGBUFFERS_HLSL