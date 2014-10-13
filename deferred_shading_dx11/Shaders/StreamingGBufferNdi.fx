#include "../Rendering.hlsl"
#include "../GBuffer.hlsl"
#include "StreamingStructs.h"
#include "IntelExtensions.hlsl"
#include "StreamingBuffers.hlsl"
#include "Merge.hlsl"

[earlydepthstencil]
void StreamingGBufferPS(GeometryVSOut input, uint coverage : SV_Coverage, out float4 dummy : SV_Target0)
{
    IntelExt_Init();
    dummy = float4(0.0f, 0.0f, 0.0f, 1.0f);

    // Get all g-buffer data. Most of this stuff is copied from Lauritzen's rendering.hlsl
    MergeNode merge = GetEmptyMergeNode();
    SetCoverage(merge, coverage);
    SetDepthTestedCoverage(merge, coverage);
    merge.zView = input.positionView.z;
    merge.zViewDerivatives = float2(ddx_fine(input.positionView.z),
                                    ddy_fine(input.positionView.z));

    // Optionally use face normal instead of shading normal
    float3 faceNormal = ComputeFaceNormal(input.positionView);
    merge.normal = EncodeSphereMap(normalize(mUI.faceNormals ? faceNormal : input.normal));

    merge.shade.albedo = gDiffuseTexture.Sample(gDiffuseSampler, input.texCoord);
    merge.shade.albedo.rgb = mUI.lightingOnly ? float3(1.0f, 1.0f, 1.0f) : merge.shade.albedo.rgb;

    // Map NULL diffuse textures to white
    uint2 textureDim;
    gDiffuseTexture.GetDimensions(textureDim.x, textureDim.y);
    merge.shade.albedo = (textureDim.x == 0U ? float4(1.0f, 1.0f, 1.0f, 1.0f) : merge.shade.albedo);
    merge.shade.specular = float2(0.9f, 25.0f); // hard coded to match values from Rendering.hlsl

    IntelExt_BeginPixelShaderOrdering();

    uint nodeIndex = GetNodeIndex(input.position.xy);
    uint nodeCount = GetNodeCount(input.position.xy);

#if defined(STREAMING_DEBUG_OPTIONS)
    int executions = GetExecutionCount(input.position.xy);
    if (mUI.executionCount > 0 && executions >= mUI.executionCount-1) {
        dummy = float4(0.0f, coverage, 0.0f, 0.0f);
        return;
    }

    SetExecutionCount(input.position.xy, executions+1);
#endif // defined(STREAMING_DEBUG_OPTIONS)

    // If this is the first iteration then save this fragment and exit.
    if (nodeCount == 0) {
        SetNodeCount(input.position.xy, nodeCount + 1);
        SetMergeNode(input.position.xy, 0, merge);
        return;
    }

#if !defined(STREAMING_FIRST_ITERATION_ONLY)
    // nodeList stores order of gMergeBuffer using 2bit indices.
    uint nodeList = GetNodeList(input.position.xy);
    uint incomingIndex = nodeCount;
    uint incomingPosition = nodeCount;
    bool needsDiscard = nodeCount == STREAMING_MAX_SURFACES_PER_PIXEL;

    // Occluder fusion variables
    float occluderStart = 0.0f;
    float occluderEnd = 0.0f;
    uint occluderCoverage = 0;
    uint minCoverageCount = countbits(0xFF);
    float minCoverageIndex = 0;
    bool completeOcclusion = false;
    uint removedIndex = 0;
    uint removedPosition = 3;

    // Compute the depth range of incoming fragment
    float incomingMin = 0.0f;
    float incomingMax = 0.0f;
    GetDepthRange(merge, incomingMin, incomingMax);

    // Main shader loop
    // For each temp in nodes:
    //  1. try to merge temp with incoming fragment. if merge succeeds then exit
    //  2. if incoming is in front of temp, then save incoming's Position as temp's position
    //  3. if we have less than the max number of nodes, continue. no need to do occluder fusion
    //  4. if incoming is in front of temp, perform occluder fusion on incoming
    //  5. perform occluder fusion on temp
    //  6. write back modified temp
    [loop][allow_uav_condition] for (uint i = 0; i < nodeCount; i++) {

        // tempIndex is the node we're trying to merge with.
        uint tempIndex = Get2BitsInByte(nodeList, i);
        MergeNode temp = GetMergeNode(input.position.xy, tempIndex);

        //  1. try to merge temp with incoming fragment. if merge succeeds then exit
        if (Merge(temp, merge, incomingMin, incomingMax)) {
#if defined(STREAMING_DEBUG_OPTIONS)
            if (mUI.averageShade) {
                AverageShadeNodes(temp.shade, merge.shade);
            }
            IncrementMergeCount(input.position.xy);
#else // !defined(STREAMING_DEBUG_OPTIONS)
            AverageShadeNodes(temp.shade, merge.shade);
#endif // !defined(STREAMING_DEBUG_OPTIONS)
            SetMergeNode(input.position.xy, tempIndex, temp);
            return;
        }

        //  2. if incoming is in front of temp, then save incoming's Position as temp's position
        if (temp.zView - merge.zView > 0.0f && incomingPosition == nodeCount) {
            incomingPosition = i;
        }

        //  3. if we have less than the max number of nodes, continue
        if (!needsDiscard) {
            continue;
        }

        //  4. if incoming is in front of temp, perform occluder fusion on incoming
        uint currentCoverage;
        if (incomingPosition == i) {
            OcclusionCheck(merge, occluderStart, occluderEnd, occluderCoverage, currentCoverage);
            UpdateMinCoverage(currentCoverage, i, minCoverageCount, removedPosition);
        }

        //  5. perform occluder fusion on temp
        uint tempCoverage = temp.coverage;
        if (OcclusionCheck(temp, occluderStart, occluderEnd, occluderCoverage, currentCoverage)) {
            removedPosition = i;
            minCoverageIndex = i;
            completeOcclusion = true;
        }

        UpdateMinCoverage(currentCoverage, i, minCoverageCount, removedPosition);

        //  6. write back modified temp
        if (tempCoverage != temp.coverage) {
            SetDepthTestedCoverage(gMergeBuffer[GetNodeIndex(input.position.xy, tempIndex)], GetDepthTestedCoverage(temp));
        }
    }

    if (incomingPosition == nodeCount && needsDiscard) {
        uint currentCoverage;
        OcclusionCheck(merge, occluderStart, occluderEnd, occluderCoverage, currentCoverage);
        UpdateMinCoverage(currentCoverage, incomingPosition, minCoverageCount, removedPosition);
    }

    // Next, update the lists.
    // 1. prepare to remove the surface from nodeList[removedPosition]
    // 2. insert incomingIndex into nodeList[incomingPosition]--pushing back entries behind it
    // 3. if we need to discard a surface, and we threw away something with valid samples
    //      save that we've discarded samples.
    // 4. create a new list with incoming inserted into the correct location and the removed
    //      node deleted.
    // 5. save the nodeList and the incoming node.

    // 1. prepare to remove the surface from nodeList[removedPosition]
    removedIndex = Get2BitsInByte(nodeList, removedPosition);

    // 2. insert incomingIndex into nodeList[incomingPosition]--pushing back entries behind it
    uint tempIndex = Get2BitsInByte(nodeList, incomingPosition);
    Set2BitsInByte(nodeList, incomingPosition, incomingIndex);
    [loop][allow_uav_condition] for (uint j = incomingPosition+1; j < nodeCount+1; j++) {
        // The removed position changes as we update nodeList. keep it up to date
        removedPosition = (tempIndex == removedIndex) ? j : removedPosition;
        uint temp2 = Get2BitsInByte(nodeList, j);
        Set2BitsInByte(nodeList, j, tempIndex);
        tempIndex = temp2;
    }

    if (needsDiscard) {

        // 3. if we need to discard a surface, and we threw away something with valid samples
        //      save that we've discarded samples.
        if (!completeOcclusion) {
#if defined(STREAMING_DEBUG_OPTIONS)
            IncrementDiscardCount(input.position.xy);
#endif // defined(STREAMING_DEBUG_OPTIONS)
            SetDiscardedSamples(input.position.xy, 1);
        }

        // this condition is pointless, BUT the hlsl compiler breaks if I remove it:
        if (needsDiscard && nodeCount <= STREAMING_MAX_SURFACES_PER_PIXEL) {

            // 4. create a new list with incoming inserted into the correct location
            // and the removed node overwritten.
            incomingIndex = Get2BitsInByte(nodeList, removedPosition);
            Set2BitsInByte(nodeList, incomingPosition, incomingIndex);

            j = 0;
            uint newList = 0;
            [loop][allow_uav_condition] for (i = 0; i < nodeCount+1; i++) {
                if (i != removedPosition) {
                    Set2BitsInByte(newList, j++, Get2BitsInByte(nodeList, i));
                }
            }
            // 5. save the nodeList and the incoming node.
            nodeList = newList;
            SetNodeList(input.position.xy, nodeList);
            SetMergeNode(input.position.xy, incomingIndex, merge);
        }
    } else {
        // 5. save the nodeList and the incoming node.
        nodeCount = nodeCount + 1;
        SetNodeCount(input.position.xy, nodeCount);
        SetNodeList(input.position.xy, nodeList);
        SetMergeNode(input.position.xy, incomingIndex, merge);
    }

#if defined(STREAMING_DEBUG_OPTIONS)
    SetLastMerge(input.position.xy, merge);
#endif // defined(STREAMING_DEBUG_OPTIONS

#endif // !defined(STREAMING_FIRST_ITERATION_ONLY)
}