#ifndef STREAMINGDEFINES_H
#define STREAMINGDEFINES_H

// #define STREAMING_DEBUG_OPTIONS 1

// #define STREAMING_FIRST_ITERATION_ONLY 1

// The way we store the per-pixel node list was broken by Intel driver version 15.33.22.3621.
// Enable this to store the per-pixel node list in a separate texture.
#define STREAMING_USE_LIST_TEXTURE 1

// Streaming debug options stores the nodeList is a separate UAV so there is no
// need to use a separate texture to store the list.
#if defined(STREAMING_USE_LIST_TEXTURE) && defined(STREAMING_DEBUG_OPTIONS)
#undef STREAMING_USE_LIST_TEXTURE
#endif // defined(STREAMING_USE_LIST_TEXTURE) && defined(STREAMING_DEBUG_OPTIONS)

#define STREAMING_TILED_ADDRESSING 1
#define STREAMING_MAX_SURFACES_PER_PIXEL 3
#define STREAMING_COS_THETA 0.78539816339f // pi / 4

#define MERGENODE_COVERAGE_BYTE 0
#define MERGENODE_DEPTHTESTEDCOVERAGE_BYTE 1

#define MERGEMETRIC_NORMALS 0
#define MERGEMETRIC_DERIVATIVES 1
#define MERGEMETRIC_BOTH 2

#define VISUALIZE_NONE 0
#define VISUALIZE_MERGES 1
#define VISUALIZE_DISCARDS 2
#define VISUALIZE_SHADING 3
#define VISUALIZE_EXECUTIONS 4

#define DERIVATIVES_NONE 0
#define DERIVATIVES_AVERAGE 1
#define DERIVATIVES_MIN 2

#define DEPTH_NONE 0
#define DEPTH_AVERAGE 1
#define DEPTH_MAX 2
#define DEPTH_MIN 3
#endif // STREAMINGDEFINES_H