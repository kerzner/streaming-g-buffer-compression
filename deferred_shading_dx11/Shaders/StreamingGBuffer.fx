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

    // merge incoming fragment with existing nodes. traverse nodes in depth order and
    // save the position where incoming fits in the list
    float incomingMin = 0.0f;
    float incomingMax = 0.0f;
    GetDepthRange(merge, incomingMin, incomingMax);
    [loop][allow_uav_condition] for (uint i = 0; i < nodeCount; i++) {

        uint tempIndex = Get2BitsInByte(nodeList, i);
        MergeNode temp = GetMergeNode(input.position.xy, tempIndex);

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
        if (temp.zView - merge.zView > 0.0f && incomingPosition == nodeCount) {
            incomingPosition = i;
        }
    }

    // at this point, the merge failed. if the incoming position is not at the end of the list
    // then insert incoming index and push everything back one slot.
    uint tempIndex = Get2BitsInByte(nodeList, incomingPosition);
    Set2BitsInByte(nodeList, incomingPosition, incomingIndex);
    [loop][allow_uav_condition] for (uint j = incomingPosition+1; j < nodeCount+1; j++) {
        uint temp2 = Get2BitsInByte(nodeList, j);
        Set2BitsInByte(nodeList, j, tempIndex);
        tempIndex = temp2;
    }

    // if we've run out of room...
    if (nodeCount == STREAMING_MAX_SURFACES_PER_PIXEL) {
        // we are going to throw away whatever surface is at nodeList[3]
        // this is the surface with the farthest distance from the eye.
        uint removedPosition = OccluderFusion(merge, nodeList,
            incomingPosition,
            nodeIndex,
            input.position.xy);
        // if the surface we're throwing away is not the incoming surface
        if (removedPosition != incomingPosition) {

            // get the index of the surface we're throwing away. this will be
            // used for overwriting data in gMergeBuffer
            incomingIndex = Get2BitsInByte(nodeList, removedPosition);
            Set2BitsInByte(nodeList, incomingPosition, incomingIndex);

            // this is the array compaction
            uint j = 0;
            uint newList = 0;
            [loop][allow_uav_condition] for (uint i = 0; i < nodeCount+1; i++) {
                if (i != removedPosition) {
                    Set2BitsInByte(newList, j++, Get2BitsInByte(nodeList, i));
                }
            }

            // save the incoming fragment
            nodeList = newList;
            SetNodeList(input.position.xy, nodeList);
            SetMergeNode(input.position.xy, incomingIndex, merge);
        }
    } else {
        nodeCount = nodeCount + 1;
        // save the incoming fragment
        SetNodeCount(input.position.xy, nodeCount);
        SetNodeList(input.position.xy, nodeList);
        SetMergeNode(input.position.xy, incomingIndex, merge);
    }

#if defined(STREAMING_DEBUG_OPTIONS)
    SetLastMerge(input.position.xy, merge);
#endif // defined(STREAMING_DEBUG_OPTIONS

#endif // !defined(STREAMING_FIRST_ITERATION_ONLY)
}