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

#pragma once

#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"
#include "Texture2D.h"
#include "Shader.h"
#include "Buffer.h"
#include <vector>
#include <memory>
#include "Shaders\StreamingStructs.h"

#define BYTES_TO_MB(x) x / 131072.0f

enum LightCullTechnique {
    CULL_FORWARD_NONE = 0,
    CULL_FORWARD_PREZ_NONE,
    CULL_DEFERRED_NONE,
    CULL_QUAD,
    CULL_QUAD_DEFERRED_LIGHTING,
    CULL_COMPUTE_SHADER_TILE,
    CULL_STREAMING_SBAA,
    CULL_STREAMING_SBAA_NDI
};

// NOTE: Must match shader equivalent structure
__declspec(align(16))
struct UIConstants
{
    unsigned int lightingOnly;
    unsigned int faceNormals;
    unsigned int visualizeLightCount;
    unsigned int visualizePerSampleShading;
    unsigned int lightCullTechnique;
#if defined(STREAMING_DEBUG_OPTIONS)
    int executionCount;
    float mergeCosTheta;
    int stats;
    int mergeMetric;
    int averageNormals;
    int averageShade;
    int derivatives;
    int depth;
    unsigned int compareCoverage;
    unsigned int compareDepth;
    unsigned int compareNormals;
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

// NOTE: Must match shader equivalent structure
struct PointLight
{
    D3DXVECTOR3 positionView;
    float attenuationBegin;
    D3DXVECTOR3 color;
    float attenuationEnd;
};

// Host-side world-space initial transform data
struct PointLightInitTransform
{
    // Cylindrical coordinates
    float radius;
    float angle;
    float height;
    float animationSpeed;
};

// Flat framebuffer RGBA16-encoded
struct FramebufferFlatElement
{
    unsigned int rb;
    unsigned int ga;
};

// Different queries used
enum GPUQuery
{
    GPUQ_FORWARD  = 0,
    GPUQ_LIGHTING = 1,
    GPUQ_RESOLVE  = 2,
    GPUQ_COUNT    = 3
};

class App
{
public:
    App(ID3D11Device* d3dDevice, unsigned int activeLights, unsigned int msaaSamples);

    ~App();
    
    void OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice,
                                 const DXGI_SURFACE_DESC* backBufferDesc);

    void Move(float elapsedTime);

    void Render(ID3D11DeviceContext* d3dDeviceContext,
                ID3D11RenderTargetView* backBuffer,
                CDXUTSDKMesh& mesh_opaque,
                CDXUTSDKMesh& mesh_alpha,
                ID3D11ShaderResourceView* skybox,
                const D3DXMATRIXA16& worldMatrix,
                const CFirstPersonCamera* viewerCamera,
                const D3D11_VIEWPORT* viewport,
                const UIConstants* ui);
    
    void SetActiveLights(ID3D11Device* d3dDevice, unsigned int activeLights);
    unsigned int GetActiveLights() const { return mActiveLights; }
    void HandleMouseEvent(ID3D11DeviceContext* d3dDeviceContext, int xPos, int yPos);

    void SaveBackbufferToFile(ID3D11DeviceContext* d3dDeviceContext,
                              ID3D11RenderTargetView* backBuffer,
                              const char* fileName,
                              int format);

    std::wostringstream GetFrameTimes(ID3D11DeviceContext* d3dDeviceContext,
                                      const UIConstants* ui,
                                      bool labels = true);
    std::wostringstream App::GetFrameTimesHeader(const UIConstants *ui);

    std::wostringstream GetFrameMemoryUsage(const UIConstants *ui, unsigned &total);

    void StartTimer(ID3D11DeviceContext *d3dDeviceContext, ID3D11Query *queries[3]);
    void StopTimer(ID3D11DeviceContext *d3dDeviceContext, ID3D11Query *queries[3]);
    float GetTime(ID3D11DeviceContext *d3dDeviceContext, ID3D11Query *queries[3]);

private:
    void InitializeLightParameters(ID3D11Device* d3dDevice);

    // Notes: 
    // - Most of these functions should all be called after initializing per frame/pass constants, etc.
    //   as the shaders that they invoke bind those constant buffers.

    // Set up shader light buffer
    ID3D11ShaderResourceView * SetupLights(ID3D11DeviceContext* d3dDeviceContext,
                                           const D3DXMATRIXA16& cameraView);

    // Forward rendering of geometry into
    ID3D11ShaderResourceView * RenderForward(ID3D11DeviceContext* d3dDeviceContext,
                                             CDXUTSDKMesh& mesh_opaque,
                                             CDXUTSDKMesh& mesh_alpha,
                                             ID3D11ShaderResourceView *lightBufferSRV,
                                             const CFirstPersonCamera* viewerCamera,
                                             const D3D11_VIEWPORT* viewport,
                                             const UIConstants* ui,
                                             bool doPreZ);
    
    // Draws geometry into G-buffer
    void RenderGBuffer(ID3D11DeviceContext* d3dDeviceContext,
                       CDXUTSDKMesh& mesh_opaque,
                       CDXUTSDKMesh& mesh_alpha,
                       const CFirstPersonCamera* viewerCamera,
                       const D3D11_VIEWPORT* viewport,
                       const UIConstants* ui);

    // Draws geometry into a g-buffer using streaming sbaa
    void RenderGBufferStreaming(ID3D11DeviceContext* d3dDeviceContext,
                                CDXUTSDKMesh& mesh_opaque,
                                CDXUTSDKMesh& mesh_alpha,
                                const CFirstPersonCamera* viewerCamera,
                                const D3D11_VIEWPORT* viewport,
                                const UIConstants* ui,
                                PixelShader* pixelShader);

    // Handles skybox, tone mapping, etc
    void RenderSkyboxAndToneMap(ID3D11DeviceContext* d3dDeviceContext,
                                ID3D11RenderTargetView* backBuffer,
                                ID3D11ShaderResourceView* skybox,
                                ID3D11ShaderResourceView* depthSRV,
                                const D3D11_VIEWPORT* viewport,
                                const UIConstants* ui);

    void ComputeLighting(ID3D11DeviceContext* d3dDeviceContext,
                         ID3D11ShaderResourceView *lightBufferSRV,
                         const D3D11_VIEWPORT* viewport,
                         const UIConstants* ui);

    void ComputeLightingStreaming(ID3D11DeviceContext* d3dDeviceContext,
                                  ID3D11RenderTargetView* backBuffer,
                                  ID3D11ShaderResourceView *lightBufferSRV,
                                  ID3D11ShaderResourceView* skybox,
                                  const D3D11_VIEWPORT* viewport,
                                  const UIConstants* ui,
                                  PixelShader* pixelShader);

    unsigned int mMSAASamples;
    float mTotalTime;

    ID3D11InputLayout* mMeshVertexLayout;

    VertexShader* mGeometryVS;

    PixelShader* mGBufferPS;
    PixelShader* mGBufferAlphaTestPS;

    PixelShader* mForwardPS;
    PixelShader* mForwardAlphaTestPS;
    PixelShader* mForwardAlphaTestOnlyPS;

    PixelShader *mStreamingGBufferPS;
    PixelShader *mStreamingResolvePS;
    PixelShader *mStreamingSkyboxPS;

    PixelShader *mStreamingGBufferNdiPS;

    CDXUTSDKMesh mSkyboxMesh;
    VertexShader* mSkyboxVS;
    PixelShader* mSkyboxPS;
    
    VertexShader* mFullScreenTriangleVS;

    PixelShader* mRequiresPerSampleShadingPS;

    PixelShader* mBasicLoopPS;
    PixelShader* mBasicLoopPerSamplePS;

    ComputeShader* mComputeShaderTileCS;

    VertexShader* mGPUQuadVS;
    GeometryShader* mGPUQuadGS;
    PixelShader* mGPUQuadPS;
    PixelShader* mGPUQuadPerSamplePS;

    PixelShader* mGPUQuadDLPS;
    PixelShader* mGPUQuadDLPerSamplePS;

    PixelShader* mGPUQuadDLResolvePS;
    PixelShader* mGPUQuadDLResolvePerSamplePS;
    
    ID3D11Buffer* mPerFrameConstants;
    
    ID3D11RasterizerState* mRasterizerState;
    ID3D11RasterizerState* mDoubleSidedRasterizerState;

    ID3D11DepthStencilState* mDepthState;
    ID3D11DepthStencilState* mWriteStencilState;
    ID3D11DepthStencilState* mEqualStencilState;

    ID3D11BlendState* mGeometryBlendState;
    ID3D11BlendState* mLightingBlendState;

    ID3D11SamplerState* mDiffuseSampler;

    std::vector< std::tr1::shared_ptr<Texture2D> > mGBuffer;
    // Handy cache of list of RT pointers for G-buffer
    std::vector<ID3D11RenderTargetView*> mGBufferRTV;
    // Handy cache of list of SRV pointers for the G-buffer
    std::vector<ID3D11ShaderResourceView*> mGBufferSRV;
    unsigned int mGBufferWidth;
    unsigned int mGBufferHeight;

    // We use a different lit buffer (different bind flags and MSAA handling) depending on whether we
    // write to it from the pixel shader (render target) or compute shader (UAV)
    std::tr1::shared_ptr<Texture2D> mLitBufferPS;
    std::tr1::shared_ptr<StructuredBuffer<FramebufferFlatElement> > mLitBufferCS;
    std::tr1::shared_ptr<Texture2D> mLitBufferStreaming;

    // A temporary accumulation buffer used for deferred lighting
    std::tr1::shared_ptr<Texture2D> mDeferredLightingAccumBuffer;

    std::tr1::shared_ptr<Depth2D> mDepthBuffer;
    std::tr1::shared_ptr<Depth2D> mDepthBufferStreaming;
    // We also need a read-only depth stencil view for techniques that read the G-buffer while also using Z-culling
    ID3D11DepthStencilView* mDepthBufferReadOnlyDSV;

    // Lighting state
    unsigned int mActiveLights;
    std::vector<PointLightInitTransform> mLightInitialTransform;
    std::vector<PointLight> mPointLightParameters;
    std::vector<D3DXVECTOR3> mPointLightPositionWorld;

    StructuredBuffer<PointLight>* mLightBuffer;

    // UAVs used for streaming SBAA
    std::tr1::shared_ptr<StructuredBuffer<MergeNodePacked> > mMergeUav;    // per-pixel merge data
    std::tr1::shared_ptr<Texture2D> mCountTexture;                         // per-pixel node count

#if defined(STREAMING_DEBUG_OPTIONS)
    std::tr1::shared_ptr<StructuredBuffer<PixelStats> > mStatsUav;         // per-pixel stats
#endif // defined(STREAMING_DEBUG_OPTIONS)

    ID3D11Query* mQuery[GPUQ_COUNT][3];
};
