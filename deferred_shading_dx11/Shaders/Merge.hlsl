#ifndef MERGE_HLSL
#define MERGE_HLSL

#include "StreamingStructs.h"
#include "DepthTests.hlsl"
#include "../PerFrameConstants.hlsl"

//--------------------------------------------------------------------------------------
bool Compare(in out MergeNode existing, in out MergeNode incoming,
             in float incomingMin, in float incomingMax)
{
    bool merge = false;

    // Compare coverage masks
    bool mutuallyExclusiveCoverage = (existing.coverage & incoming.coverage) == 0;
#if defined(STREAMING_DEBUG_OPTIONS)
    if(!mUI.compareCoverage) {
        mutuallyExclusiveCoverage = true;
    }
#endif // !defined(STREAMING_DEBUG_OPTIONS)

    if (mutuallyExclusiveCoverage) {

        // Compare depth ranges
        float existingMin, existingMax;
        GetDepthRange(existing, existingMin, existingMax);
        bool depthRangesOverlap = (existingMin <= incomingMax && incomingMin <= existingMax);

#if defined(STREAMING_DEBUG_OPTIONS)
        if(!mUI.compareDepth) {
            depthRangesOverlap = true;
        }
#endif // !defined(STREAMING_DEBUG_OPTIONS)

        if (depthRangesOverlap) {

            float cosTheta;
#if defined(STREAMING_DEBUG_OPTIONS)
            cosTheta = mUI.mergeCosTheta;
#else // !defined(STREAMING_DEBUG_OPTIONS)
            cosTheta = STREAMING_COS_THETA;
#endif // !defined(STREAMING_DEBUG_OPTIONS)

#if defined(STREAMING_DEBUG_OPTIONS)
            int metric = mUI.mergeMetric;
            if (metric == MERGEMETRIC_NORMALS || metric == MERGEMETRIC_BOTH) {
                float3 n0 = DecodeSphereMap(existing.normal);
                float3 n1 = DecodeSphereMap(incoming.normal);
                merge = (abs(dot(n0, n1)) >= cosTheta);
            }

            if (metric == MERGEMETRIC_DERIVATIVES || metric == MERGEMETRIC_BOTH) {
                // Compute face normal of existing. I'm not sure if this is correct.
                // Without the normalize(existing.zViewDerivatives),
                // the reconstructed normals are almost always parallel.
                // There is something wierd going on here...
                float2 tmpExisting = normalize(existing.zViewDerivatives);
                // float2 tmpExisting = existing.zViewDerivatives;
                float3 v0 = normalize(float3(1.0f, 0.0f, tmpExisting.x));
                float3 v1 = normalize(float3(0.0f, 1.0f, tmpExisting.y));
                float3 n0 = cross(v0, v1);

                float2 tmpIncoming = normalize(incoming.zViewDerivatives);
                // float2 tmpIncoming = incoming.zViewDerivatives;
                v0 = normalize(float3(1.0f, 0.0f, tmpIncoming.x));
                v1 = normalize(float3(0.0f, 1.0f, tmpIncoming.y));
                float3 n1 = cross(v0, v1);
                if (metric == MERGEMETRIC_DERIVATIVES) {
                    merge = (abs(dot(n0, n1)) >= cosTheta);
                } else if (metric == MERGEMETRIC_BOTH) {
                    merge = merge && (abs(dot(n0, n1)) >= cosTheta);
                }
            }

            if(!mUI.compareNormals) {
                    merge = true;
            }
#else // !defined(STREAMING_DEBUG_OPTIONS)
            float3 n0 = DecodeSphereMap(existing.normal);
            float3 n1 = DecodeSphereMap(incoming.normal);
            merge = (abs(dot(n0, n1)) >= cosTheta);
#endif // defined(STREAMING_DEBUG_OPTIONS)
        }
    }
    return merge;
}

//--------------------------------------------------------------------------------------
void AverageNormals(in out MergeNode existing, in MergeNode incoming)
{
    float3 existingNormal = DecodeSphereMap(existing.normal);
    float3 incomingNormal = DecodeSphereMap(incoming.normal);
    existing.normal = EncodeSphereMap((existingNormal + incomingNormal) / 2.0f);
}

//--------------------------------------------------------------------------------------
void UseMinDerivatives(in out MergeNode existing, in MergeNode incoming)
{
    [flatten] if (abs(existing.zViewDerivatives.x) > abs(incoming.zViewDerivatives.x)) {
        existing.zViewDerivatives.x = incoming.zViewDerivatives.x;
    }

    [flatten] if (abs(existing.zViewDerivatives.y) > abs(incoming.zViewDerivatives.y)) {
        existing.zViewDerivatives.y = incoming.zViewDerivatives.y;
    }
}

// Always combine coverage. Conditionally combine normals, depths, and derivatives.
//--------------------------------------------------------------------------------------
bool Merge(in out MergeNode existing, in out MergeNode incoming,
           in float incomingMin, in float incomingMax)
{
    if (Compare(existing, incoming, incomingMin, incomingMax)) {
        SetCoverage(existing, GetCoverage(existing)| GetCoverage(incoming));
        SetDepthTestedCoverage(existing, GetDepthTestedCoverage(existing) | GetDepthTestedCoverage(incoming));

#if defined(STREAMING_DEBUG_OPTIONS)
        if (mUI.averageNormals) {
            AverageNormals(existing, incoming);
        }

        int mode = mUI.derivatives;
        if (mode == DERIVATIVES_AVERAGE) {
            existing.zViewDerivatives = (existing.zViewDerivatives + incoming.zViewDerivatives) / 2.0f;
        } else if (mode == DERIVATIVES_MIN) {
            UseMinDerivatives(existing, incoming);
        }

        mode = mUI.depth;
        if (mode == DEPTH_AVERAGE) {
            existing.zView = (existing.zView + incoming.zView) / 2.0f;
        } else if (mode == DEPTH_MAX) {
            existing.zView = max(existing.zView, incoming.zView);
        } else if (mode == DEPTH_MIN) {
            existing.zView = min(existing.zView, incoming.zView);
        }

#else // !defined(STREAMING_DEBUG_OPTIONS)
        AverageNormals(existing, incoming);
        existing.zViewDerivatives = (existing.zViewDerivatives + incoming.zViewDerivatives) / 2.0f;
        existing.zView = min(existing.zView, incoming.zView);
#endif // !defined(STREAMING_DEBUG_OPTIONS)
        return true;
    } else {
        return false;
    }
}

//--------------------------------------------------------------------------------------
void AverageShadeNodes(in out ShadeNode existing, in ShadeNode incoming)
{
    existing.albedo = (existing.albedo + incoming.albedo) / 2.0f;
    existing.specular = (existing.specular + incoming.specular) / 2.0f;
}
#endif // MERGE_HLSL