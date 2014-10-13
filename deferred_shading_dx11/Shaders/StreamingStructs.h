#ifndef STREAMINGSTRUCTS_H
#define STREAMINGSTRUCTS_H

#include "StreamingDefines.h"

#if defined(__cplusplus)

struct ShadeNode
{
    float albedo[4];
    float specular[2];
};

struct ShadeNodePacked
{
    unsigned albedo;
    unsigned specular;
};

struct MergeNode
{
    unsigned coverage;
    float zViewDerivatives[2];
    float zView;
    float normal[2];
    ShadeNode shade;
#if defined(STREAMING_DEBUG_OPTIONS)
    unsigned depthTestedCoverage;
    unsigned shadeIndex;
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

struct MergeNodePacked
{
    unsigned coverage;
    unsigned zViewDerivatives;
    unsigned normal;
    float zView;
    unsigned albedo;
#if defined(STREAMING_DEBUG_OPTIONS)
    unsigned depthTestedCoverage;
    unsigned shadeIndex;
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

#if defined(STREAMING_DEBUG_OPTIONS)
struct PixelStats
{
    unsigned executions;
    unsigned discardedSamples;
    MergeNode lastMerge;
    ShadeNode lastShade;
    unsigned merges;
    unsigned discards;
    unsigned nodeCount;
    unsigned nodeList;
};
#endif // defined(STREAMING_DEBUG_OPTIONS)

#else // !defined(__cplusplus)

#include "GBuffer.hlsl"
#include "../PerFrameConstants.hlsl"
#include "UintByteArray.hlsl"
#include "D3DX_DXGIFormatConvert.inl"

struct ShadeNode
{
    float4 albedo;
    float2 specular;
};

struct ShadeNodePacked
{
    uint albedo;
    uint specular;
};

struct MergeNode
{
    uint coverage;
    float2 zViewDerivatives;
    float zView;
    float2 normal;
    ShadeNode shade;
#if defined(STREAMING_DEBUG_OPTIONS)
    uint depthTestedCoverage;
    uint shadeIndex;
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

struct MergeNodePacked
{
    uint coverage;
    uint zViewDerivatives;
    float zView;
    uint normal;
    uint albedo;
#if defined(STREAMING_DEBUG_OPTIONS)
    uint depthTestedCoverage;
    uint shadeIndex;
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

#if defined(STREAMING_DEBUG_OPTIONS)
struct PixelStats
{
    uint executions;
    uint discardedSamples;
    MergeNode lastMerge;
    ShadeNode lastShade;
    uint merges;
    uint discards;
    uint nodeCount;
    uint nodeList;
};
#endif // defined(STREAMING_DEBUG_OPTIONS)

MergeNode GetEmptyMergeNode()
{
    MergeNode merge;
    merge.coverage = 0;
    merge.zViewDerivatives = 0;
    merge.zView = 100000.0f; // far plane distance
    merge.normal = float2(0.0f, 0.0f);
#if defined(STREAMING_DEBUG_OPTIONS)
    merge.depthTestedCoverage = 0;
    merge.shadeIndex = 0;
#endif // defined(STREAMING_DEBUG_OPTIONS)
    merge.shade.albedo = float4(0.0f, 0.0f, 0.0f, 0.0f);
    merge.shade.specular = float2(0.0f, 0.0f);
    return merge;
}

void GetGBufferFromShadeAndMergeNodes(float2 positionViewport, MergeNode merge, ShadeNode shade, out GBuffer gBuffer)
{
    gBuffer.normal_specular = float4(merge.normal, shade.specular.x, shade.specular.y);
    gBuffer.albedo = shade.albedo;
    gBuffer.positionZGrad = merge.zViewDerivatives;
}

uint GetCoverage(in MergeNode merge)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    return merge.coverage;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    return GetByteInUint(merge.coverage, MERGENODE_COVERAGE_BYTE);
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}

void SetCoverage(in out MergeNode merge, in uint coverage)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    merge.coverage = coverage;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    SetByteInUint(merge.coverage, MERGENODE_COVERAGE_BYTE, coverage);
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}

void SetDepthTestedCoverage(in out MergeNode merge, in uint coverage)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    merge.depthTestedCoverage = coverage;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    SetByteInUint(merge.coverage, MERGENODE_DEPTHTESTEDCOVERAGE_BYTE, coverage);
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}

void SetDepthTestedCoverage(in out MergeNodePacked merge, in uint coverage)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    merge.depthTestedCoverage = coverage;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    SetByteInUint(merge.coverage, MERGENODE_DEPTHTESTEDCOVERAGE_BYTE, coverage);
#endif // !defined(STREAMING_DEBUG_OPTIONS)}
}

uint GetDepthTestedCoverage(in MergeNode merge)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    return merge.depthTestedCoverage;
#else // !defined(STREAMING_DEBUG_OPTIONS)
    return GetByteInUint(merge.coverage, MERGENODE_DEPTHTESTEDCOVERAGE_BYTE);
#endif // !defined(STREAMING_DEBUG_OPTIONS)
}
#endif // !defined(__cplusplus)
#endif // STREAMINGSTRUCTS_H