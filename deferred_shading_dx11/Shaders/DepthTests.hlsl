//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
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
//--------------------------------------------------------------------------------------

#ifndef DEPTHTESTS_HLSH
#define DEPTHTESTS_HLSH

#include "StreamingStructs.h"
#include "SamplePositions.hlsl"

// Returns viewspace depth at sample loations
//-----------------------------------------------------------------------------
float EstimateDepthAtSample(in MergeNode merge, in uint sampleId)
{
    float2 derivatives = merge.zViewDerivatives;
    float2 position = GetSamplePosition(sampleId);
    return merge.zView + (derivatives.x * position.x) + (derivatives.y * position.y);
}

// Sets start and end to the min and max of merge's depth range.
//-----------------------------------------------------------------------------
void GetDepthRange(in MergeNode merge, out float start, out float end)
{
    start = merge.zView + (merge.zViewDerivatives.x < 0 ? merge.zViewDerivatives.x : -merge.zViewDerivatives.x);
    start = start + (merge.zViewDerivatives.y < 0 ? merge.zViewDerivatives.y : -merge.zViewDerivatives.y);

    end = merge.zView + (merge.zViewDerivatives.x > 0 ? merge.zViewDerivatives.x : -merge.zViewDerivatives.x);
    end = end + (merge.zViewDerivatives.y > 0 ? merge.zViewDerivatives.y : -merge.zViewDerivatives.y);
}

// Returns true if the mergeNode's estimated depth ranges overlap
//-----------------------------------------------------------------------------
bool DepthRangesOverlap(in uint2 coords,
                        in uint nodeList,
                        in uint nodeCount,
                        in out float weights[STREAMING_MAX_SURFACES_PER_PIXEL],
                        in out float weightSum)
{
    float occluderStart = 0.0f;
    float occluderEnd = 0.0f;
    [unroll] for (uint i = 0; i < nodeCount; i++) {
        MergeNode temp = GetMergeNode(coords, Get2BitsInByte(nodeList, i));
        float mergeStart, mergeEnd; // depth range of merge
        GetDepthRange(temp, mergeStart, mergeEnd);
        [flatten] if (occluderStart <= mergeEnd && mergeStart <= occluderEnd) {
            weightSum = 0.0f;
            return true;
        }
        weights[i] = (float) countbits(GetDepthTestedCoverage(temp));
        weightSum += weights[i];
    }
    return false;
}

// Resolve per-sample coverage accounting for interpenetrating fragments.
// weights[i] is # of samples covered by mergeNode[i]
// weightSum is total of weights. (1 - weightSum) is skybox contribution.
//-----------------------------------------------------------------------------
void ResolveSurfaceWeights(in uint2 coords,
                           in uint nodeList,
                           in uint nodeCount,
                           in out uint weights,
                           in out float weightSum)
{
    // 1. For each surface, loop over samples it covers.
    // 2. For each sample covered, check that the surface is the closest one at that sample
    // 3. Track the closest surface at each sample
    // 4. Count the total number of samples where each surface is unoccluded

    // 2bits in here store the surface Ids. surfaceIds[i] == 3 implies the surface is uncovered.
#if (STREAMING_MAX_SURFACES_PER_PIXEL == 4)
    uint surfaceIds = 0x924924;
#else //  (STREAMING_MAX_SURFACES_PER_PIXEL < 4)
    uint surfaceIds = 0xFFFF;
#endif // (STREAMING_MAX_SURFACES_PER_PIXEL < 4)

    // array of closest depth values. this is slightly faster than using a float[8] as it avoids
    // dynaic indexing
    float4 closestDepths0 = float4(10000.0f, 10000.0f, 10000.0f, 10000.0f);
    float4 closestDepths1 = float4(10000.0f, 10000.0f, 10000.0f, 10000.0f);

    // 1. For each surface, loop over samples it covers.
    [unroll] for (uint j = 0; j < nodeCount; j++) {

        // 2. For each sample covered, check that the surface is the closest one at that sample
        [unroll] for (uint i = 0; i < MSAA_SAMPLES; i++) {

            // Load the surface from uav
            uint tempIndex = Get2BitsInByte(nodeList, j);
            MergeNode temp = GetMergeNode(coords, tempIndex);

            // Does surface j cover sample i?
            uint coveredSample = (1 << i);
            uint jCovered = GetDepthTestedCoverage(temp);
            uint jCoversi = coveredSample & jCovered;

            // 3. Track the closest surface at each sample
            if (jCoversi) {
                // Is surfwace j the closest surface at sample i?
                float sampleDepth = EstimateDepthAtSample(temp, i);
                bool  sampleCloser = false;

                // This is ugly, but it avoids the dynamic indexing.
                if (i < 4) {
                    sampleCloser = closestDepths0[i] > sampleDepth;
                    closestDepths0[i] = sampleCloser ? sampleDepth : closestDepths0[i];
                } else {
                    sampleCloser = closestDepths1[i%4] > sampleDepth;
                    closestDepths1[i % 4] = sampleCloser ? sampleDepth : closestDepths1[i % 4];
                }

                // Set surfaceIds[i] to tempIndex if this is the closest surface at this sample
                Set2BitsInByte(surfaceIds, i, sampleCloser ? tempIndex : Get2BitsInByte(surfaceIds, i));
            }
        }
    }

    // 4. Count the total number of samples where each surface is unoccludedwtt
    [unroll] for (uint i = 0; i < MSAA_SAMPLES; i++) {
        uint tempIndex = Get2BitsInByte(surfaceIds, i);
        [flatten] if (tempIndex < STREAMING_MAX_SURFACES_PER_PIXEL) {
            SetByteInUint(weights, tempIndex, GetByteInUint(weights, tempIndex) + 1);
            weightSum = weightSum + 1;
        }
    }
}


// If "merge" is completely occluded by "occluder" then set
// merge.depthTestedCoverage to 0 and returns true. otherwise returns false.
//-----------------------------------------------------------------------------
bool OcclusionCheck(in out MergeNode merge, in out float occluderStart,
                    in out float occluderEnd, in out uint occluderCoverage,
                    out uint newInfo)
{
    bool occluded = false;
    float mergeStart, mergeEnd; // depth range of merge
    GetDepthRange(merge, mergeStart, mergeEnd);

    // Use consts for coverage mask logic or hlsl compiler will screw it up... (wtf!?)
    const uint mergeCoverage = merge.coverage;
    const uint coverage = occluderCoverage;
    const uint mergeAndOccluderCoverage = mergeCoverage & coverage;
    const uint mergeDTC = GetDepthTestedCoverage(merge);
    const uint newDTC = mergeDTC ^ (mergeAndOccluderCoverage & mergeDTC);
    newInfo = newDTC;

    // Are the two fragments not interpenetrating?
    if (occluderStart < mergeStart && occluderEnd < mergeStart) {
        occluded = mergeAndOccluderCoverage == mergeCoverage;
        SetDepthTestedCoverage(merge, newDTC);
    }

    occluderStart = min(occluderStart, mergeStart);
    occluderEnd = max(occluderEnd, mergeEnd);
    occluderCoverage = mergeCoverage | coverage;
    return occluded;
}

// Performs occluder fusion from front-to-back of surfaces. If a surfaces is occluded
// its depth tested coverage is set to 0 and removedIndex points to it. If no surfaces
// are occluded, the surface with the smallest depth tested coverage is removed.
// Exactly one surface will be removed by occlusion or discard.
//-----------------------------------------------------------------------------
void OccluderFusion(in out MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL+1],
                    in out uint removedIndex, uint2 coords)
{
    // "occluder" is aggregate of all fragments up to a certain depth range.
    float occluderStart, occluderEnd;
    GetDepthRange(mergeNodes[0], occluderStart, occluderEnd);
    uint occluderCoverage = GetDepthTestedCoverage(mergeNodes[0]);

    // Track the smallest coverage for discarding
    uint minCoverageCount = countbits(occluderCoverage);
    uint minCoverage = occluderCoverage;
    uint minCoverageIndex = 0;
    bool needsDiscard = true;
    uint newInfo = 0;
    // Check if all Surfaces[0,...,i-1] and Current occlude Surfaces[i]
    [unroll] for (uint i = 1; i < STREAMING_MAX_SURFACES_PER_PIXEL+1; i++) {
        if (OcclusionCheck(mergeNodes[i], occluderStart, occluderEnd, occluderCoverage, newInfo)) {
            needsDiscard = false;
            removedIndex = i;
            break;
        }

    if (countbits(newInfo) <= minCoverageCount) {
            minCoverage = newInfo;
            minCoverageCount = countbits(newInfo);
            minCoverageIndex = i;
        }
    }

    // Discard smallest coverage
    if (needsDiscard) {
        SetDiscardedSamples(coords, 1);
        SetDepthTestedCoverage(mergeNodes[minCoverageIndex], 0);
        removedIndex = minCoverageIndex;
#if defined(STREAMING_DEBUG_OPTIONS)
        IncrementDiscardCount(coords);
#endif // defined(STREAMING_DEBUG_OPTIONS)
    }
}

//-----------------------------------------------------------------------------
void OccluderFusionResolve(in out MergeNode mergeNodes[STREAMING_MAX_SURFACES_PER_PIXEL],
                           uint2 coords)
{
    // "occluder" is aggregate of all fragments up to a certain depth range.
    float occluderStart, occluderEnd;
    GetDepthRange(mergeNodes[0], occluderStart, occluderEnd);
    uint occluderCoverage = GetDepthTestedCoverage(mergeNodes[0]);
    uint newInfo = 0;
    // Check if all Surfaces[0,...,i-1] and Current occlude Surfaces[i]
    [unroll] for (uint i = 1; i < STREAMING_MAX_SURFACES_PER_PIXEL; i++) {
        OcclusionCheck(mergeNodes[i], occluderStart, occluderEnd, occluderCoverage, newInfo);
    }
}

//-----------------------------------------------------------------------------
void UpdateMinCoverage(in uint currentCoverage, in uint currentPosition,
                       in out uint minCoverageCount, in out uint minCoveragePosition)
{
    uint currentCoverageCount = countbits(currentCoverage);
    if (currentCoverageCount <= minCoverageCount) {
        minCoverageCount = currentCoverageCount;
        minCoveragePosition = currentPosition;
    }
}

// Returns the removedPosition in the nodeList.
//-----------------------------------------------------------------------------
uint OccluderFusion(in out MergeNode incoming, in uint nodeList,
                    in uint incomingPosition, in uint nodeIndex,
                    in uint2 coords)
{
    float occluderStart = 0.0f;
    float occluderEnd = 0.0f;
    uint occluderCoverage = 0;
    uint minCoverageCount = countbits(0xFF);
    uint minCoveragePosition = 0;
    uint newInfo = 0;

    // Loop over all surfaces
    [loop][allow_uav_condition] for (uint i = 0; i < STREAMING_MAX_SURFACES_PER_PIXEL+1; i++) {

        // temp is the current surface we are trying to occlude
        MergeNode temp;
        uint tempIndex = 0;

        // set temp from memory or from the incoming surface
        if (i==incomingPosition) {
           temp = incoming;
        } else {
            tempIndex = Get2BitsInByte(nodeList, i);
            temp = GetMergeNode(coords, tempIndex);
        }

        // if a surface is entirely occluded then we've found a slot for incoming
        if (OcclusionCheck(temp, occluderStart, occluderEnd, occluderCoverage, newInfo)) {
            return i;
        }

        if (i!=incomingPosition) {
            SetMergeNode(coords, tempIndex, temp);
        }

        // track the surface that adds the least amount of new information to the pixel
        UpdateMinCoverage(newInfo, i, minCoverageCount, minCoveragePosition);
    }

#if defined(STREAMING_DEBUG_OPTIONS)
    IncrementDiscardCount(coords);
#endif // defined(STREAMING_DEBUG_OPTIONS)

    // nothing got entirely occluded. we need to throw away a surface
    SetDiscardedSamples(coords, 1);
    return minCoveragePosition;
}
#endif // DEPTHTESTS_HLSL
