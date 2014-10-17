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

#include "App.h"
#include "ColorUtil.h"
#include "ShaderDefines.h"
#include <limits>
#include <sstream>
#include <random>
#include <algorithm>

#include "Shaders/StreamingStructs.h"
#include "Shaders/StreamingDefines.h"

#include "DirectXTex\DirectXTex\DirectXTex.h"

using std::tr1::shared_ptr;

// NOTE: Must match layout of shader constant buffers

__declspec(align(16))
struct PerFrameConstants
{
    D3DXMATRIX mCameraWorldViewProj;
    D3DXMATRIX mCameraWorldView;
    D3DXMATRIX mCameraViewProj;
    D3DXMATRIX mCameraProj;
    D3DXVECTOR4 mCameraNearFar;

    unsigned int mFramebufferDimensionsX;
    unsigned int mFramebufferDimensionsY;
    unsigned int mFramebufferDimensionsZ;
    unsigned int mFramebufferDimensionsW;

    UIConstants mUI;
};


App::App(ID3D11Device *d3dDevice, unsigned int activeLights, unsigned int msaaSamples)
    : mMSAASamples(msaaSamples)
    , mTotalTime(0.0f)
    , mActiveLights(0)
    , mLightBuffer(0)
    , mDepthBufferReadOnlyDSV(0)
{
    std::string msaaSamplesStr;
    {
        std::ostringstream oss;
        oss << mMSAASamples;
        msaaSamplesStr = oss.str();
    }

    // Set up macros
    D3D10_SHADER_MACRO defines[] = {
        {"MSAA_SAMPLES", msaaSamplesStr.c_str()},
        {0, 0}
    };

    // Create shaders
    mGeometryVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "GeometryVS", defines);

    mGBufferPS = new PixelShader(d3dDevice, L"GBuffer.hlsl", "GBufferPS", defines);
    mGBufferAlphaTestPS = new PixelShader(d3dDevice, L"GBuffer.hlsl", "GBufferAlphaTestPS", defines);

    mForwardPS = new PixelShader(d3dDevice, L"Forward.hlsl", "ForwardPS", defines);
    mForwardAlphaTestPS = new PixelShader(d3dDevice, L"Forward.hlsl", "ForwardAlphaTestPS", defines);
    mForwardAlphaTestOnlyPS = new PixelShader(d3dDevice, L"Forward.hlsl", "ForwardAlphaTestOnlyPS", defines);

    mStreamingGBufferPS = new PixelShader(d3dDevice, L"Shaders/StreamingGBuffer.fx", "StreamingGBufferPS", defines);
    mStreamingResolvePS = new PixelShader(d3dDevice, L"Shaders/StreamingResolve.fx", "StreamingResolvePS", defines);

    mStreamingGBufferNdiPS = new PixelShader(d3dDevice, L"Shaders/StreamingGBufferNdi.fx", "StreamingGBufferPS", defines);

    mFullScreenTriangleVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "FullScreenTriangleVS", defines);

    mSkyboxVS = new VertexShader(d3dDevice, L"SkyboxToneMap.hlsl", "SkyboxVS", defines);
    mSkyboxPS = new PixelShader(d3dDevice, L"SkyboxToneMap.hlsl", "SkyboxPS", defines);
    mStreamingSkyboxPS = new PixelShader(d3dDevice, L"Shaders/StreamingSkyboxToneMap.fx", "SkyboxPS", defines);

    mRequiresPerSampleShadingPS = new PixelShader(d3dDevice, L"GBuffer.hlsl", "RequiresPerSampleShadingPS", defines);

    mBasicLoopPS = new PixelShader(d3dDevice, L"BasicLoop.hlsl", "BasicLoopPS", defines);
    mBasicLoopPerSamplePS = new PixelShader(d3dDevice, L"BasicLoop.hlsl", "BasicLoopPerSamplePS", defines);
    mComputeShaderTileCS = new ComputeShader(d3dDevice, L"ComputeShaderTile.hlsl", "ComputeShaderTileCS", defines);

    mGPUQuadVS = new VertexShader(d3dDevice, L"GPUQuad.hlsl", "GPUQuadVS", defines);
    mGPUQuadGS = new GeometryShader(d3dDevice, L"GPUQuad.hlsl", "GPUQuadGS", defines);
    mGPUQuadPS = new PixelShader(d3dDevice, L"GPUQuad.hlsl", "GPUQuadPS", defines);
    mGPUQuadPerSamplePS = new PixelShader(d3dDevice, L"GPUQuad.hlsl", "GPUQuadPerSamplePS", defines);

    mGPUQuadDLPS = new PixelShader(d3dDevice, L"GPUQuadDL.hlsl", "GPUQuadDLPS", defines);
    mGPUQuadDLPerSamplePS = new PixelShader(d3dDevice, L"GPUQuadDL.hlsl", "GPUQuadDLPerSamplePS", defines);

    mGPUQuadDLResolvePS = new PixelShader(d3dDevice, L"GPUQuadDL.hlsl", "GPUQuadDLResolvePS", defines);
    mGPUQuadDLResolvePerSamplePS = new PixelShader(d3dDevice, L"GPUQuadDL.hlsl", "GPUQuadDLResolvePerSamplePS", defines);

    // Create input layout
    {
        // We need the vertex shader bytecode for this... rather than try to wire that all through the
        // shader interface, just recompile the vertex shader.
        UINT shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
        ID3D10Blob *bytecode = 0;
        HRESULT hr = D3DX11CompileFromFile(L"Rendering.hlsl", defines, 0, "GeometryVS", "vs_5_0", shaderFlags, 0, 0, &bytecode, 0, 0);
        if (FAILED(hr)) {
            assert(false);      // It worked earlier...
        }

        const D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            {"position",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"normal",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"texCoord",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        
        d3dDevice->CreateInputLayout( 
            layout, ARRAYSIZE(layout), 
            bytecode->GetBufferPointer(),
            bytecode->GetBufferSize(), 
            &mMeshVertexLayout);

        bytecode->Release();
    }

    // Create standard rasterizer state
    {
        CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
        d3dDevice->CreateRasterizerState(&desc, &mRasterizerState);

        desc.CullMode = D3D11_CULL_NONE;
        d3dDevice->CreateRasterizerState(&desc, &mDoubleSidedRasterizerState);
    }
    
    {
        CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
        // NOTE: Complementary Z => GREATER test
        desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        d3dDevice->CreateDepthStencilState(&desc, &mDepthState);
    }

    // Stencil states for MSAA
    {
        CD3D11_DEPTH_STENCIL_DESC desc(
            FALSE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL,   // Depth
            TRUE, 0xFF, 0xFF,                                                     // Stencil
            D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS, // Front face stencil
            D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS  // Back face stencil
            );
        d3dDevice->CreateDepthStencilState(&desc, &mWriteStencilState);
    }
    {
        CD3D11_DEPTH_STENCIL_DESC desc(
            TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL,    // Depth
            TRUE, 0xFF, 0xFF,                                                     // Stencil
            D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL, // Front face stencil
            D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL  // Back face stencil
            );
        d3dDevice->CreateDepthStencilState(&desc, &mEqualStencilState);
    }

    // Create geometry phase blend state
    {
        CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
        d3dDevice->CreateBlendState(&desc, &mGeometryBlendState);
    }

    // Create lighting phase blend state
    {
        CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
        // Additive blending
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        d3dDevice->CreateBlendState(&desc, &mLightingBlendState);
    }

    // Create constant buffers
    {
        CD3D11_BUFFER_DESC desc(
            sizeof(PerFrameConstants),
            D3D11_BIND_CONSTANT_BUFFER,
            D3D11_USAGE_DYNAMIC,
            D3D11_CPU_ACCESS_WRITE);

        d3dDevice->CreateBuffer(&desc, 0, &mPerFrameConstants);
    }

    // Create sampler state
    {
        CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
        desc.Filter = D3D11_FILTER_ANISOTROPIC;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MaxAnisotropy = 16;
        d3dDevice->CreateSamplerState(&desc, &mDiffuseSampler);
    }

    // Create skybox mesh
    mSkyboxMesh.Create(d3dDevice, L"Media\\Skybox\\Skybox.sdkmesh");

    InitializeLightParameters(d3dDevice);
    SetActiveLights(d3dDevice, activeLights);

    // Create timer queries
    {
        D3D11_QUERY_DESC desc0 = {D3D11_QUERY_TIMESTAMP, 0};
        D3D11_QUERY_DESC desc1 = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        for (int i = 0; i < GPUQ_COUNT; i++) {
            d3dDevice->CreateQuery(&desc0, &mQuery[i][0]);
            d3dDevice->CreateQuery(&desc0, &mQuery[i][1]);
            d3dDevice->CreateQuery(&desc1, &mQuery[i][2]);
        }
    }
}

App::~App() 
{
    mSkyboxMesh.Destroy();
    SAFE_RELEASE(mDepthBufferReadOnlyDSV);
    delete mLightBuffer;
    SAFE_RELEASE(mDiffuseSampler);
    SAFE_RELEASE(mPerFrameConstants);
    SAFE_RELEASE(mLightingBlendState);
    SAFE_RELEASE(mGeometryBlendState);
    SAFE_RELEASE(mEqualStencilState);
    SAFE_RELEASE(mWriteStencilState);
    SAFE_RELEASE(mDepthState);
    SAFE_RELEASE(mDoubleSidedRasterizerState);
    SAFE_RELEASE(mRasterizerState);
    SAFE_RELEASE(mMeshVertexLayout);
    delete mSkyboxPS;
    delete mSkyboxVS;
    delete mComputeShaderTileCS;
    delete mGPUQuadDLResolvePerSamplePS;
    delete mGPUQuadDLResolvePS;
    delete mGPUQuadDLPerSamplePS;
    delete mGPUQuadDLPS;
    delete mGPUQuadPerSamplePS;
    delete mGPUQuadPS;
    delete mGPUQuadGS;
    delete mGPUQuadVS;
    delete mRequiresPerSampleShadingPS;
    delete mBasicLoopPerSamplePS;
    delete mBasicLoopPS;
    delete mFullScreenTriangleVS;
    delete mForwardAlphaTestOnlyPS;
    delete mForwardAlphaTestPS;
    delete mForwardPS;
    delete mGBufferAlphaTestPS;
    delete mGBufferPS;
    delete mGeometryVS;
    delete mStreamingGBufferPS;
    delete mStreamingResolvePS;
    delete mStreamingSkyboxPS;
    for (int i = 0; i < GPUQ_COUNT; i++) {
        SAFE_RELEASE(mQuery[i][0]);
        SAFE_RELEASE(mQuery[i][1]);
        SAFE_RELEASE(mQuery[i][2]);
    }
    delete mStreamingGBufferNdiPS;
}


void App::OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice,
                                  const DXGI_SURFACE_DESC* backBufferDesc)
{
    mGBufferWidth = backBufferDesc->Width;
    mGBufferHeight = backBufferDesc->Height;

    // Create/recreate any textures related to screen size
    mGBuffer.resize(0);
    mGBufferRTV.resize(0);
    mGBufferSRV.resize(0);
    mLitBufferPS = 0;
    mLitBufferCS = 0;
    mDeferredLightingAccumBuffer = 0;
    mDepthBuffer = 0;
    mDepthBufferStreaming = 0;
    SAFE_RELEASE(mDepthBufferReadOnlyDSV);

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = mMSAASamples;
    sampleDesc.Quality = 0;

    // standard depth/stencil buffer
    mDepthBuffer = shared_ptr<Depth2D>(new Depth2D(
        d3dDevice, mGBufferWidth, mGBufferHeight,
        D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc,
        mMSAASamples > 1    // Include stencil if using MSAA
        ));

    mDepthBufferStreaming = shared_ptr<Depth2D>(new Depth2D(
        d3dDevice, mGBufferWidth, mGBufferHeight,
        D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc));

    // read-only depth stencil view
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        mDepthBuffer->GetDepthStencil()->GetDesc(&desc);
        desc.Flags = D3D11_DSV_READ_ONLY_DEPTH;

        d3dDevice->CreateDepthStencilView(mDepthBuffer->GetTexture(), &desc, &mDepthBufferReadOnlyDSV);
    }

    // NOTE: The next set of buffers are not all needed at the same time... a given technique really only needs one of them.
    // We allocate them all up front for quick swapping between techniques and to keep the code as simple as possible.

    // lit buffers
    mLitBufferPS = shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc));

    mLitBufferCS = shared_ptr< StructuredBuffer<FramebufferFlatElement> >(new StructuredBuffer<FramebufferFlatElement>(
        d3dDevice, mGBufferWidth * mGBufferHeight * mMSAASamples,
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE));

    // Streaming technique resolves into a single sample buffer before skybox and tonemap
    mLitBufferStreaming = shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));

    // deferred lighting accumulation buffer
    mDeferredLightingAccumBuffer = shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc));

    // G-Buffer

    // normal_specular
    mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc)));

    // albedo
    mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc)));

    // positionZgrad
    mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16_FLOAT,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        sampleDesc)));

    // Set up GBuffer resource list
    mGBufferRTV.resize(mGBuffer.size(), 0);
    mGBufferSRV.resize(mGBuffer.size() + 1, 0);
    for (std::size_t i = 0; i < mGBuffer.size(); ++i) {
        mGBufferRTV[i] = mGBuffer[i]->GetRenderTarget();
        mGBufferSRV[i] = mGBuffer[i]->GetShaderResource();
    }
    // Depth buffer is the last SRV that we use for reading
    mGBufferSRV.back() = mDepthBuffer->GetShaderResource();

    // Uav used for streaming SBAA
    mMergeUav = shared_ptr<StructuredBuffer<MergeNodePacked> >(new StructuredBuffer<MergeNodePacked>(
        d3dDevice, mGBufferWidth * mGBufferHeight * STREAMING_MAX_SURFACES_PER_PIXEL,
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE));

    mCountTexture = (shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R32_UINT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)));

#if defined(STREAMING_USE_LIST_TEXTURE)
    mListTexture = (shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R32_UINT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)));
#endif // defined(STREAMING_USE_LIST_TEXTURE)

#if defined(STREAMING_DEBUG_OPTIONS)
    mStatsUav = shared_ptr<StructuredBuffer<PixelStats> >(new StructuredBuffer<PixelStats>(
        d3dDevice, mGBufferWidth * mGBufferHeight, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE));
#endif // defined(STREAMING_DEBUG_OPTIONS)
}


void App::InitializeLightParameters(ID3D11Device* d3dDevice)
{
    mPointLightParameters.resize(MAX_LIGHTS);
    mLightInitialTransform.resize(MAX_LIGHTS);
    mPointLightPositionWorld.resize(MAX_LIGHTS);

    // Use a constant seed for consistency
    std::tr1::mt19937 rng(1337);

    std::tr1::uniform_real<float> radiusNormDist(0.0f, 1.0f);
    const float maxRadius = 100.0f;
    std::tr1::uniform_real<float> angleDist(0.0f, 2.0f * D3DX_PI); 
    std::tr1::uniform_real<float> heightDist(0.0f, 20.0f);
    std::tr1::uniform_real<float> animationSpeedDist(2.0f, 20.0f);
    std::tr1::uniform_int<int> animationDirection(0, 1);
    std::tr1::uniform_real<float> hueDist(0.0f, 1.0f);
    std::tr1::uniform_real<float> intensityDist(0.1f, 0.5f);
    std::tr1::uniform_real<float> attenuationDist(2.0, 150.0f);
    const float attenuationStartFactor = 0.8f;

    // NOTE(ebk) - this is a hack to get light #0 always shining on the teapot.
    PointLight &params = mPointLightParameters[0];
    PointLightInitTransform &init = mLightInitialTransform[0];
    init.radius = std::sqrt(radiusNormDist(rng)) * maxRadius;
    init.angle = angleDist(rng);
    init.height = heightDist(rng);
    init.animationSpeed = (animationDirection(rng) * 2 - 1) * animationSpeedDist(rng) / init.radius;

    params.color = intensityDist(rng) * HueToRGB(hueDist(rng));
    params.attenuationEnd = 100.0f;
    params.attenuationBegin = 0.8 * 100.0f;

    for (unsigned int i = 1; i < MAX_LIGHTS; ++i) {
        PointLight& params = mPointLightParameters[i];
        PointLightInitTransform& init = mLightInitialTransform[i];

        init.radius = std::sqrt(radiusNormDist(rng)) * maxRadius;
        init.angle = angleDist(rng);
        init.height = heightDist(rng);
        // Normalize by arc length
        init.animationSpeed = (animationDirection(rng) * 2 - 1) * animationSpeedDist(rng) / init.radius;
        
        // HSL->RGB, vary light hue
        params.color = intensityDist(rng) * HueToRGB(hueDist(rng));
        params.attenuationEnd = attenuationDist(rng);
        params.attenuationBegin = attenuationStartFactor * params.attenuationEnd;
    }
}


void App::SetActiveLights(ID3D11Device* d3dDevice, unsigned int activeLights)
{
    mActiveLights = activeLights;

    delete mLightBuffer;
    mLightBuffer = new StructuredBuffer<PointLight>(d3dDevice, activeLights, D3D11_BIND_SHADER_RESOURCE, true);
    
    // Make sure all the active lights are set up
    Move(0.0f);
}


void App::Move(float elapsedTime)
{
    mTotalTime += elapsedTime;

    // Update positions of active lights
    for (unsigned int i = 0; i < mActiveLights; ++i) {
        const PointLightInitTransform& initTransform = mLightInitialTransform[i];
        float angle = initTransform.angle + mTotalTime * initTransform.animationSpeed;
        mPointLightPositionWorld[i] = D3DXVECTOR3(
            initTransform.radius * std::cos(angle),
            initTransform.height,
            initTransform.radius * std::sin(angle));
    }
}


void App::Render(ID3D11DeviceContext* d3dDeviceContext, 
                 ID3D11RenderTargetView* backBuffer,
                 CDXUTSDKMesh& mesh_opaque,
                 CDXUTSDKMesh& mesh_alpha,
                 ID3D11ShaderResourceView* skybox,
                 const D3DXMATRIXA16& worldMatrix,
                 const CFirstPersonCamera* viewerCamera,
                 const D3D11_VIEWPORT* viewport,
                 const UIConstants* ui)
{
    D3DXMATRIXA16 cameraProj = *viewerCamera->GetProjMatrix();
    D3DXMATRIXA16 cameraView = *viewerCamera->GetViewMatrix();
    
    D3DXMATRIXA16 cameraViewInv;
    D3DXMatrixInverse(&cameraViewInv, 0, &cameraView);
        
    // Compute composite matrices
    D3DXMATRIXA16 cameraViewProj = cameraView * cameraProj;
    D3DXMATRIXA16 cameraWorldViewProj = worldMatrix * cameraViewProj;

    // Fill in frame constants
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

        constants->mCameraWorldViewProj = cameraWorldViewProj;
        constants->mCameraWorldView = worldMatrix * cameraView;
        constants->mCameraViewProj = cameraViewProj;
        constants->mCameraProj = cameraProj;
        // NOTE: Complementary Z => swap near/far back
        constants->mCameraNearFar = D3DXVECTOR4(viewerCamera->GetFarClip(), viewerCamera->GetNearClip(), 0.0f, 0.0f);

        constants->mFramebufferDimensionsX = mGBufferWidth;
        constants->mFramebufferDimensionsY = mGBufferHeight;
        constants->mFramebufferDimensionsZ = 0;     // Unused
        constants->mFramebufferDimensionsW = 0;     // Unused

        constants->mUI = *ui;

        d3dDeviceContext->Unmap(mPerFrameConstants, 0);
    }
    // Geometry phase
    if (mesh_opaque.IsLoaded()) {
        mesh_opaque.ComputeInFrustumFlags(cameraWorldViewProj);
    }
    if (mesh_alpha.IsLoaded()) {
        mesh_alpha.ComputeInFrustumFlags(cameraWorldViewProj);
    }

    // Setup lights
    ID3D11ShaderResourceView *lightBufferSRV = SetupLights(d3dDeviceContext, cameraView);
    // Forward rendering takes a different path here
    if (ui->lightCullTechnique == CULL_FORWARD_NONE) {
        StartTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);
        RenderForward(d3dDeviceContext, mesh_opaque, mesh_alpha, lightBufferSRV, viewerCamera, viewport, ui, false);
        StopTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);
    } else if (ui->lightCullTechnique == CULL_FORWARD_PREZ_NONE) {
        StartTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);
        RenderForward(d3dDeviceContext, mesh_opaque, mesh_alpha, lightBufferSRV, viewerCamera, viewport, ui, true);
        StopTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);
    } else if (ui->lightCullTechnique == CULL_STREAMING_SBAA ||
               ui->lightCullTechnique == CULL_STREAMING_SBAA_NDI) {

        PixelShader *gBufferPS = ui->lightCullTechnique == CULL_STREAMING_SBAA ? mStreamingGBufferPS :
                                                           mStreamingGBufferNdiPS;

        PixelShader *resolvePS = mStreamingResolvePS;

        StartTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);
        RenderGBufferStreaming(d3dDeviceContext, mesh_opaque, mesh_alpha, viewerCamera, viewport, ui, gBufferPS);
        StopTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);

        StartTimer(d3dDeviceContext, mQuery[GPUQ_RESOLVE]);
        ComputeLightingStreaming(d3dDeviceContext, backBuffer, lightBufferSRV, skybox, viewport, ui, resolvePS);
        StopTimer(d3dDeviceContext, mQuery[GPUQ_RESOLVE]);

        return;
    } else {
        StartTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);
        RenderGBuffer(d3dDeviceContext, mesh_opaque, mesh_alpha, viewerCamera, viewport, ui);
        StopTimer(d3dDeviceContext, mQuery[GPUQ_FORWARD]);

        StartTimer(d3dDeviceContext, mQuery[GPUQ_LIGHTING]);
        ComputeLighting(d3dDeviceContext, lightBufferSRV, viewport, ui);
        StopTimer(d3dDeviceContext, mQuery[GPUQ_LIGHTING]);
    }

    // Render skybox and tonemap
    StartTimer(d3dDeviceContext, mQuery[GPUQ_RESOLVE]);
    RenderSkyboxAndToneMap(d3dDeviceContext, backBuffer, skybox,
        mDepthBuffer->GetShaderResource(), viewport, ui);
    StopTimer(d3dDeviceContext, mQuery[GPUQ_RESOLVE]);
}


ID3D11ShaderResourceView * App::SetupLights(ID3D11DeviceContext* d3dDeviceContext,
                                            const D3DXMATRIXA16& cameraView)
{
    // Transform light world positions into view space and store in our parameters array
    D3DXVec3TransformCoordArray(&mPointLightParameters[0].positionView, sizeof(PointLight),
        &mPointLightPositionWorld[0], sizeof(D3DXVECTOR3), &cameraView, mActiveLights);

    // NOTE(ebk) - this is a hack to get light #0 always shining on the teapot.
    mPointLightPositionWorld[0] = D3DXVECTOR3(4.744, 3.208, -4.43);
    
    // Copy light list into shader buffer
    {
        PointLight* light = mLightBuffer->MapDiscard(d3dDeviceContext);
        for (unsigned int i = 0; i < mActiveLights; ++i) {
            light[i] = mPointLightParameters[i];
        }
        mLightBuffer->Unmap(d3dDeviceContext);
    }
    
    return mLightBuffer->GetShaderResource();
}


ID3D11ShaderResourceView * App::RenderForward(ID3D11DeviceContext* d3dDeviceContext,
                                              CDXUTSDKMesh& mesh_opaque,
                                              CDXUTSDKMesh& mesh_alpha,
                                              ID3D11ShaderResourceView *lightBufferSRV,
                                              const CFirstPersonCamera* viewerCamera,
                                              const D3D11_VIEWPORT* viewport,
                                              const UIConstants* ui,
                                              bool doPreZ)
{
    // Clear lit and depth buffer
    const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dDeviceContext->ClearRenderTargetView(mLitBufferPS->GetRenderTarget(), zeros);
    // NOTE: Complementary Z buffer: clear to 0 (far)!
    d3dDeviceContext->ClearDepthStencilView(mDepthBuffer->GetDepthStencil(), D3D11_CLEAR_DEPTH, 0.0f, 0);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mGeometryVS->GetShader(), 0, 0);
    
    d3dDeviceContext->GSSetShader(0, 0, 0);

    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetShaderResources(5, 1, &lightBufferSRV);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    // Diffuse texture set per-material by DXUT mesh routines

    d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
    
    // Pre-Z pass if requested
    if (doPreZ) {
        d3dDeviceContext->OMSetRenderTargets(0, 0, mDepthBuffer->GetDepthStencil());
            
        // Render opaque geometry
        if (mesh_opaque.IsLoaded()) {
            d3dDeviceContext->RSSetState(mRasterizerState);
            d3dDeviceContext->PSSetShader(0, 0, 0);
            mesh_opaque.Render(d3dDeviceContext, 0);
        }
        // Render alpha tested geometry
        if (mesh_alpha.IsLoaded()) {
            d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
            // NOTE: Use simplified alpha test shader that only clips
            d3dDeviceContext->PSSetShader(mForwardAlphaTestOnlyPS->GetShader(), 0, 0);
            mesh_alpha.Render(d3dDeviceContext, 0);
        }
    }

    // Set up render targets
    ID3D11RenderTargetView *renderTargets[1] = {mLitBufferPS->GetRenderTarget()};
    d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBuffer->GetDepthStencil());
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    // Render opaque geometry
    if (mesh_opaque.IsLoaded()) {
        d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->PSSetShader(mForwardPS->GetShader(), 0, 0);
        mesh_opaque.Render(d3dDeviceContext, 0);
    }
    // Render alpha tested geometry
    if (mesh_alpha.IsLoaded()) {
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mForwardAlphaTestPS->GetShader(), 0, 0);
        mesh_alpha.Render(d3dDeviceContext, 0);
    }
    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);

    return mLitBufferPS->GetShaderResource();
}


void App::RenderGBuffer(ID3D11DeviceContext* d3dDeviceContext,
                        CDXUTSDKMesh& mesh_opaque,
                        CDXUTSDKMesh& mesh_alpha,
                        const CFirstPersonCamera* viewerCamera,
                        const D3D11_VIEWPORT* viewport,
                        const UIConstants* ui)
{
    // Clear GBuffer
    // NOTE: We actually only need to clear the depth buffer here since we replace unwritten (i.e. far plane) samples
    // with the skybox. We use the depth buffer to reconstruct position and only in-frustum positions are shaded.
    // NOTE: Complementary Z buffer: clear to 0 (far)!
    d3dDeviceContext->ClearDepthStencilView(mDepthBuffer->GetDepthStencil(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mGeometryVS->GetShader(), 0, 0);
    
    d3dDeviceContext->GSSetShader(0, 0, 0);

    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    // Diffuse texture set per-material by DXUT mesh routines

    // Set up render GBuffer render targets
    d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
    d3dDeviceContext->OMSetRenderTargets(static_cast<UINT>(mGBufferRTV.size()), &mGBufferRTV.front(), mDepthBuffer->GetDepthStencil());
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    // Render opaque geometry
    if (mesh_opaque.IsLoaded()) {
        d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->PSSetShader(mGBufferPS->GetShader(), 0, 0);
        mesh_opaque.Render(d3dDeviceContext, 0);
    }

    // Render alpha tested geometry
    if (mesh_alpha.IsLoaded()) {
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mGBufferAlphaTestPS->GetShader(), 0, 0);
        mesh_alpha.Render(d3dDeviceContext, 0);
    }

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
}


void App::ComputeLighting(ID3D11DeviceContext* d3dDeviceContext,
                          ID3D11ShaderResourceView *lightBufferSRV,
                          const D3D11_VIEWPORT* viewport,
                          const UIConstants* ui)
{
    // TODO: Clean up the branchiness here a bit... refactor into small functions
         
    switch (ui->lightCullTechnique) {

    case CULL_COMPUTE_SHADER_TILE:
    {
        // No need to clear, we write all pixels
        // Compute shader setup (always does all the lights at once)
        d3dDeviceContext->CSSetConstantBuffers(0, 1, &mPerFrameConstants);
        d3dDeviceContext->CSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
        d3dDeviceContext->CSSetShaderResources(5, 1, &lightBufferSRV);

        ID3D11UnorderedAccessView *litBufferUAV = mLitBufferCS->GetUnorderedAccess();
        d3dDeviceContext->CSSetUnorderedAccessViews(1, 1, &litBufferUAV, 0);
        d3dDeviceContext->CSSetShader(mComputeShaderTileCS->GetShader(), 0, 0);

        // Dispatch
        unsigned int dispatchWidth = (mGBufferWidth + COMPUTE_SHADER_TILE_GROUP_DIM - 1) / COMPUTE_SHADER_TILE_GROUP_DIM;
        unsigned int dispatchHeight = (mGBufferHeight + COMPUTE_SHADER_TILE_GROUP_DIM - 1) / COMPUTE_SHADER_TILE_GROUP_DIM;
        d3dDeviceContext->Dispatch(dispatchWidth, dispatchHeight, 1);
    }
    break;

    case CULL_QUAD:
    case CULL_QUAD_DEFERRED_LIGHTING: {
        bool deferredLighting = (ui->lightCullTechnique == CULL_QUAD_DEFERRED_LIGHTING);
        std::tr1::shared_ptr<Texture2D> &accumulateBuffer = deferredLighting ? mDeferredLightingAccumBuffer : mLitBufferPS;

        // Clear
        const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        d3dDeviceContext->ClearRenderTargetView(accumulateBuffer->GetRenderTarget(), zeros);
        
        if (mMSAASamples > 1) {
            // Full screen triangle setup
            d3dDeviceContext->IASetInputLayout(0);
            d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

            d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);
            d3dDeviceContext->GSSetShader(0, 0, 0);

            d3dDeviceContext->RSSetState(mRasterizerState);
            d3dDeviceContext->RSSetViewports(1, viewport);

            d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
            d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
            d3dDeviceContext->PSSetShaderResources(5, 1, &lightBufferSRV);

            // Set stencil mask for samples that require per-sample shading
            d3dDeviceContext->PSSetShader(mRequiresPerSampleShadingPS->GetShader(), 0, 0);
            d3dDeviceContext->OMSetDepthStencilState(mWriteStencilState, 1);
            d3dDeviceContext->OMSetRenderTargets(0, 0, mDepthBufferReadOnlyDSV);
            d3dDeviceContext->Draw(3, 0);
        }

        // Point primitives expanded into quads in the geometry shader
        d3dDeviceContext->IASetInputLayout(0);
        d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

        d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
        d3dDeviceContext->VSSetShaderResources(5, 1, &lightBufferSRV);
        d3dDeviceContext->VSSetShader(mGPUQuadVS->GetShader(), 0, 0);

        d3dDeviceContext->GSSetShader(mGPUQuadGS->GetShader(), 0, 0);

        d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->RSSetViewports(1, viewport);

        d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
        d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
        d3dDeviceContext->PSSetShaderResources(5, 1, &lightBufferSRV);

        // Additively blend into lit buffer        
        ID3D11RenderTargetView * renderTargets[1] = {accumulateBuffer->GetRenderTarget()};
        // Use depth buffer for culling but no writes (use the read-only DSV)
        d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
        d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);
        
        // Dispatch one point per light

        // Do pixel frequency shading
        d3dDeviceContext->PSSetShader(deferredLighting ? mGPUQuadDLPS->GetShader() : mGPUQuadPS->GetShader(), 0, 0);
        d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 0);
        d3dDeviceContext->Draw(mActiveLights, 0);

        if (mMSAASamples > 1) {
            // Do sample frequency shading
            d3dDeviceContext->PSSetShader(deferredLighting ? mGPUQuadDLPerSamplePS->GetShader() : mGPUQuadPerSamplePS->GetShader(), 0, 0);
            d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 1);
            d3dDeviceContext->Draw(mActiveLights, 0);
        }

        if (deferredLighting) {
            // Final screen-space pass to combine diffuse and specular
            d3dDeviceContext->IASetInputLayout(0);
            d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

            d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);
            d3dDeviceContext->GSSetShader(0, 0, 0);
            
            ID3D11RenderTargetView * resolveRenderTargets[1] = {mLitBufferPS->GetRenderTarget()};
            d3dDeviceContext->OMSetRenderTargets(1, resolveRenderTargets, mDepthBufferReadOnlyDSV);
            d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

            ID3D11ShaderResourceView * accumulateBufferSRV = accumulateBuffer->GetShaderResource();
            d3dDeviceContext->PSSetShaderResources(7, 1, &accumulateBufferSRV);

            // Do pixel frequency resolve
            d3dDeviceContext->PSSetShader(mGPUQuadDLResolvePS->GetShader(), 0, 0);
            d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 0);
            d3dDeviceContext->Draw(3, 0);

            if (mMSAASamples > 1) {
                // Do sample frequency resolve
                d3dDeviceContext->PSSetShader(mGPUQuadDLResolvePerSamplePS->GetShader(), 0, 0);
                d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 1);
                d3dDeviceContext->Draw(3, 0);
            }
        }
    }
    break;
    case CULL_DEFERRED_NONE:
    {
        // Clear
        const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        d3dDeviceContext->ClearRenderTargetView(mLitBufferPS->GetRenderTarget(), zeros);
        
        // Full screen triangle setup
        d3dDeviceContext->IASetInputLayout(0);
        d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

        d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);
        d3dDeviceContext->GSSetShader(0, 0, 0);

        d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->RSSetViewports(1, viewport);

        d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
        d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
        d3dDeviceContext->PSSetShaderResources(5, 1, &lightBufferSRV);

        if (mMSAASamples > 1) {
            // Set stencil mask for samples that require per-sample shading
            d3dDeviceContext->PSSetShader(mRequiresPerSampleShadingPS->GetShader(), 0, 0);
            d3dDeviceContext->OMSetDepthStencilState(mWriteStencilState, 1);
            d3dDeviceContext->OMSetRenderTargets(0, 0, mDepthBufferReadOnlyDSV);
            d3dDeviceContext->Draw(3, 0);
        }

        // Additively blend into back buffer
        ID3D11RenderTargetView * renderTargets[1] = {mLitBufferPS->GetRenderTarget()};
        d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
        d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

        // Do pixel frequency shading
        d3dDeviceContext->PSSetShader(mBasicLoopPS->GetShader(), 0, 0);
        d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 0);
        d3dDeviceContext->Draw(3, 0);

        if (mMSAASamples > 1) {
            // Do sample frequency shading
            d3dDeviceContext->PSSetShader(mBasicLoopPerSamplePS->GetShader(), 0, 0);
            d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 1);
            d3dDeviceContext->Draw(3, 0);
        }
    }
    break;

    };  // switch

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->VSSetShader(0, 0, 0);
    d3dDeviceContext->GSSetShader(0, 0, 0);
    d3dDeviceContext->PSSetShader(0, 0, 0);
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
    d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
    d3dDeviceContext->CSSetShaderResources(0, 8, nullSRV);
    ID3D11UnorderedAccessView *nullUAV[1] = {0};
    d3dDeviceContext->CSSetUnorderedAccessViews(1, 1, nullUAV, 0);
}


void App::RenderSkyboxAndToneMap(ID3D11DeviceContext* d3dDeviceContext,
                                 ID3D11RenderTargetView* backBuffer,
                                 ID3D11ShaderResourceView* skybox,
                                 ID3D11ShaderResourceView* depthSRV,
                                 const D3D11_VIEWPORT* viewport,
                                 const UIConstants* ui)
{
    D3D11_VIEWPORT skyboxViewport(*viewport);
    skyboxViewport.MinDepth = 1.0f;
    skyboxViewport.MaxDepth = 1.0f;

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mSkyboxVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
    d3dDeviceContext->RSSetViewports(1, &skyboxViewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    d3dDeviceContext->PSSetShader(mSkyboxPS->GetShader(), 0, 0);

    d3dDeviceContext->PSSetShaderResources(5, 1, &skybox);
    d3dDeviceContext->PSSetShaderResources(6, 1, &depthSRV);

    // Bind the appropriate lit buffer depending on the technique
    ID3D11ShaderResourceView* litViews[2] = {0, 0};
    switch (ui->lightCullTechnique) {
    // Compute-shader based techniques use the flattened MSAA buffer
    case CULL_COMPUTE_SHADER_TILE:
    case CULL_STREAMING_SBAA:
    case CULL_STREAMING_SBAA_NDI:
        litViews[1] = mLitBufferCS->GetShaderResource();
        break;
    default:
        litViews[0] = mLitBufferPS->GetShaderResource();
        break;
    }
    d3dDeviceContext->PSSetShaderResources(7, 2, litViews);

    d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    mSkyboxMesh.Render(d3dDeviceContext);

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullViews[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->PSSetShaderResources(0, 10, nullViews);
}


void App::RenderGBufferStreaming(ID3D11DeviceContext* d3dDeviceContext,
                                CDXUTSDKMesh& mesh_opaque,
                                CDXUTSDKMesh& mesh_alpha,
                                const CFirstPersonCamera* viewerCamera,
                                const D3D11_VIEWPORT* viewport,
                                const UIConstants* ui,
                                PixelShader* pixelShader)
{
    // Clear GBuffer
    // NOTE: We actually only need to clear the depth buffer here since we replace unwritten (i.e. far plane) samples
    // with the skybox. We use the depth buffer to reconstruct position and only in-frustum positions are shaded.
    // NOTE: Complementary Z buffer: clear to 0 (far)!
    d3dDeviceContext->ClearDepthStencilView(mDepthBufferStreaming->GetDepthStencil(), D3D11_CLEAR_DEPTH, 0.0f, 0);
    const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dDeviceContext->ClearRenderTargetView(mGBufferRTV.front(), zeros);
    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mGeometryVS->GetShader(), 0, 0);
    d3dDeviceContext->GSSetShader(0, 0, 0);

    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);

#if defined(STREAMING_USE_LIST_TEXTURE)
    int uavCount = 3;
    ID3D11UnorderedAccessView* unorderedAccessViews[3] = {
        mMergeUav->GetUnorderedAccess(),
        mCountTexture->GetUnorderedAccess(),
        mListTexture->GetUnorderedAccess() };
#else // !defined(STREAMING_USE_LIST_TEXTURE)
#if defined(STREAMING_DEBUG_OPTIONS)
    int uavCount = 3;
    ID3D11UnorderedAccessView* unorderedAccessViews[3] = {
        mMergeUav->GetUnorderedAccess(),
        mCountTexture->GetUnorderedAccess(),
        mStatsUav->GetUnorderedAccess() };
#else // !defined(STREAMING_DEBUG_OPTIONS)
    int uavCount = 2;
    ID3D11UnorderedAccessView* unorderedAccessViews[2] = {
        mMergeUav->GetUnorderedAccess(),
        mCountTexture->GetUnorderedAccess() };
#endif // !defined(STREAMING_DEBUG_OPTIONS)
#endif // !defined(STREAMING_USE_LIST_TEXTURE)

    d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
    d3dDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &mGBufferRTV.at(1), mDepthBufferStreaming->GetDepthStencil(), 3, uavCount, unorderedAccessViews, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Render opaque geometry
    if (mesh_opaque.IsLoaded()) {
        d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->PSSetShader(pixelShader->GetShader(), 0, 0);
        mesh_opaque.Render(d3dDeviceContext, 0);
    }

    // Render alpha tested geometry
    if (mesh_alpha.IsLoaded()) {
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(pixelShader->GetShader(), 0, 0);
        mesh_alpha.Render(d3dDeviceContext, 0);
    }

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, 0, 0, 0, 0, 0, 0);
}


void App::ComputeLightingStreaming(ID3D11DeviceContext* d3dDeviceContext,
                          ID3D11RenderTargetView* backBuffer,
                          ID3D11ShaderResourceView *lightBufferSRV,
                          ID3D11ShaderResourceView* skybox,
                          const D3D11_VIEWPORT* viewport,
                          const UIConstants* ui,
                          PixelShader *pixelShader)
{
    // Clear
    const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dDeviceContext->ClearRenderTargetView(backBuffer, zeros);

    d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mSkyboxVS->GetShader(), 0, 0);

    d3dDeviceContext->GSSetShader(0, 0, 0);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetShaderResources(6, 1, &skybox);
    d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
    d3dDeviceContext->PSSetShaderResources(5, 1, &lightBufferSRV);

#if defined(STREAMING_USE_LIST_TEXTURE)
    int uavCount = 3;
    ID3D11UnorderedAccessView* unorderedAccessViews[3] = {
        mMergeUav->GetUnorderedAccess(),
        mCountTexture->GetUnorderedAccess(),
        mListTexture->GetUnorderedAccess() };
#else // !defined(STREAMING_USE_LIST_TEXTURE)
#if defined(STREAMING_DEBUG_OPTIONS)
    int uavCount = 3;
    ID3D11UnorderedAccessView* unorderedAccessViews[4] = {
        mMergeUav->GetUnorderedAccess(),
        mCountTexture->GetUnorderedAccess(),
        mStatsUav->GetUnorderedAccess() };
#else // !defined(STREAMING_DEBUG_OPTIONS)
    int uavCount = 2;
    ID3D11UnorderedAccessView* unorderedAccessViews[3] = {
        mMergeUav->GetUnorderedAccess(),
        mCountTexture->GetUnorderedAccess() };
#endif // !defined(STREAMING_DEBUG_OPTIONS)
#endif // !defined(STREAMING_USE_LIST_TEXTURE)

    d3dDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &backBuffer, 0, 3, uavCount, unorderedAccessViews, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Do pixel frequency shading
    d3dDeviceContext->PSSetShader(pixelShader->GetShader(), 0, 0);
    mSkyboxMesh.Render(d3dDeviceContext);

    d3dDeviceContext->VSSetShader(0, 0, 0);
    d3dDeviceContext->GSSetShader(0, 0, 0);
    d3dDeviceContext->PSSetShader(0, 0, 0);
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
    d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
    d3dDeviceContext->CSSetShaderResources(0, 8, nullSRV);
}


void App::HandleMouseEvent(ID3D11DeviceContext* d3dDeviceContext, int xPos, int yPos)
{
#if defined(STREAMING_DEBUG_OPTIONS)
    D3D11_MAPPED_SUBRESOURCE mergeMap = mMergeUav->Map(d3dDeviceContext);
    MergeNodePacked* merge = (MergeNodePacked*) mergeMap.pData;

    D3D11_MAPPED_SUBRESOURCE statsMap = mStatsUav->Map(d3dDeviceContext);
    PixelStats* stats = (PixelStats*) statsMap.pData;

    const unsigned tileLogX = 0;
    const unsigned tileLogY = 1;
    const unsigned surfaceWidth = mGBufferWidth >> tileLogX;
    unsigned tileAddr2D[2]  = {xPos >> tileLogX, yPos >> tileLogY};
    unsigned tileAddr1D  = (tileAddr2D[0] + surfaceWidth * tileAddr2D[1]) << (tileLogX + tileLogY);
    unsigned pixelAddr2D[2] = {xPos & ((1 << tileLogX) - 1), yPos & ((1 << tileLogY) - 1)};
    unsigned pixelAddr1D = (pixelAddr2D[1] << tileLogX) | pixelAddr2D[0];
    unsigned offsetInTile = tileAddr1D | pixelAddr1D;

    MergeNodePacked *mergeNodes[3];
    for (unsigned i = 0; i < 3; i++) {
        mergeNodes[i] = merge + offsetInTile + (mGBufferHeight * mGBufferWidth * i);
    }

    stats = stats + (mGBufferWidth * yPos) + xPos;;

    mMergeUav->Unmap(d3dDeviceContext);
    mStatsUav->Unmap(d3dDeviceContext);
#endif // defined(STREAMING_DEBUG_OPTIONS)
}


void App::SaveBackbufferToFile(ID3D11DeviceContext* d3dDeviceContext,
                               ID3D11RenderTargetView* backBuffer,
                               const char* filename,
                               int format)
{
    ID3D11Device* d3dDevice;
    d3dDeviceContext->GetDevice(&d3dDevice);

    ID3D11Resource* resource;
    backBuffer->GetResource(&resource);

    DirectX::ScratchImage imageResult;
    ID3D11Texture2D* texture = static_cast<ID3D11Texture2D*>(resource);
    HRESULT hr = DirectX::CaptureTexture(d3dDevice, d3dDeviceContext, texture, imageResult);
    assert(SUCCEEDED(hr) && "Failed to capture texture");

    const DirectX::Image* img = imageResult.GetImages();
    assert(img  && "Failed to retrieve images");

    size_t nimg = imageResult.GetImageCount();
    assert(nimg > 0 && "No images retrieved");

    DirectX::TexMetadata metaData = imageResult.GetMetadata();
    if ((DXGI_FORMAT) format != DXGI_FORMAT_UNKNOWN) {
        metaData.format = (DXGI_FORMAT) format;
    }

    WCHAR file[128];
    MultiByteToWideChar(0, 0, filename, -1, file, 128);
    hr = SaveToTGAFile(*imageResult.GetImage( 0, 0, 0 ), file);
    assert(SUCCEEDED(hr) && ("Failed save image to file"));

    SAFE_RELEASE(resource);
    SAFE_RELEASE(d3dDevice);
}

std::wostringstream App::GetFrameTimesHeader(const UIConstants *ui)
{
    std::wostringstream oss;

    switch (ui->lightCullTechnique) {
    case CULL_FORWARD_NONE:
    case CULL_FORWARD_PREZ_NONE:
        oss << "forward, resolve" << std::endl;
        break;
    case CULL_DEFERRED_NONE:
    case CULL_QUAD:
    case CULL_QUAD_DEFERRED_LIGHTING:
    case CULL_COMPUTE_SHADER_TILE:
        oss << "forward, lighting, resolve" << std::endl;
        break;
    case CULL_STREAMING_SBAA:
    case CULL_STREAMING_SBAA_NDI:
    default:
        oss << "forward, lighting + resolve" << std::endl;
        break;
    }

    return oss;
}

std::wostringstream App::GetFrameTimes(ID3D11DeviceContext* d3dDeviceContext,
                                       const UIConstants* ui,
                                       bool labels)
{
    std::wostringstream oss;

    // If we're printing times to the screen then label them. Otherwise,
    // we're writing them to a file as a CSV.
    if (labels) {
        switch (ui->lightCullTechnique) {
        case CULL_FORWARD_NONE:
        case CULL_FORWARD_PREZ_NONE:
            oss << "Forward:" << GetTime(d3dDeviceContext, mQuery[GPUQ_FORWARD]) << std::endl;
            oss << "Resolve:" << GetTime(d3dDeviceContext, mQuery[GPUQ_RESOLVE]) << std::endl;
            break;
        case CULL_DEFERRED_NONE:
        case CULL_QUAD:
        case CULL_QUAD_DEFERRED_LIGHTING:
        case CULL_COMPUTE_SHADER_TILE:
            oss << "Forward:" << GetTime(d3dDeviceContext, mQuery[GPUQ_FORWARD]) << std::endl;
            oss << "Lighting:" << GetTime(d3dDeviceContext, mQuery[GPUQ_LIGHTING]) << std::endl;
            oss << "Resolve:" << GetTime(d3dDeviceContext, mQuery[GPUQ_RESOLVE]) << std::endl;
            break;
        case CULL_STREAMING_SBAA:
        case CULL_STREAMING_SBAA_NDI:
        default:
            oss << "Forward:" << GetTime(d3dDeviceContext, mQuery[GPUQ_FORWARD]) << std::endl;
            oss << "Lighting + resolve:" << GetTime(d3dDeviceContext, mQuery[GPUQ_RESOLVE]) << std::endl;
            break;
        }
    } else {
        switch (ui->lightCullTechnique) {
        case CULL_FORWARD_NONE:
        case CULL_FORWARD_PREZ_NONE:
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_FORWARD]) << ", ";
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_RESOLVE]) << std::endl;
            break;
        case CULL_DEFERRED_NONE:
        case CULL_QUAD:
        case CULL_QUAD_DEFERRED_LIGHTING:
        case CULL_COMPUTE_SHADER_TILE:
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_FORWARD]) << ", ";
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_LIGHTING]) << ", ";
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_RESOLVE]) << std::endl;
            break;
        case CULL_STREAMING_SBAA:
        case CULL_STREAMING_SBAA_NDI:
        default:
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_FORWARD]) << ", ";
            oss << GetTime(d3dDeviceContext, mQuery[GPUQ_RESOLVE]) << std::endl;
            break;
        }
    }

    return oss;
}


void App::StartTimer(ID3D11DeviceContext *d3dDeviceContext, ID3D11Query *queries[3])
{
    d3dDeviceContext->Begin(queries[2]);
    d3dDeviceContext->End(queries[0]);
}


void App::StopTimer(ID3D11DeviceContext *d3dDeviceContext, ID3D11Query *queries[3])
{
    d3dDeviceContext->End(queries[2]);
    d3dDeviceContext->End(queries[1]);
}


float App::GetTime(ID3D11DeviceContext *d3dDeviceContext, ID3D11Query *queries[3])
{
    float time = 0.0f;
    UINT64 queryTimeA, queryTimeB;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT queryTimeData;
    while ((S_OK != d3dDeviceContext->GetData(queries[0], &queryTimeA, sizeof(UINT64), 0))
        || (S_OK != d3dDeviceContext->GetData(queries[1], &queryTimeB, sizeof(UINT64), 0))
        || (S_OK != d3dDeviceContext->GetData(queries[2], &queryTimeData,
                                              sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0))) {
        Sleep(1);
    };

    if (0 == queryTimeData.Disjoint) {
        UINT64 deltaTimeTicks = queryTimeB - queryTimeA;
        time = (float)deltaTimeTicks * 1000.0f / (float)queryTimeData.Frequency;
    } else {
        time= 0.0f;
    }

    return time;
}


std::wostringstream App::GetFrameMemoryUsage(const UIConstants *ui, unsigned &total)
{
    std::wostringstream oss;
    oss << "Memory usage in Mb" << std::endl;
    total = 0; // memory usage in bytes.

    // Add the depth buffer to total.
    // Our technique, Lauritzen's forward and compute shaders use an MSAA 4 byte float for depth.
    // The deferred techniques use an MSAA 4-byte depth + 1 byte stencil + 3 byte unused.
    D3D11_TEXTURE2D_DESC desc;
    switch (ui->lightCullTechnique) {
    case CULL_STREAMING_SBAA:
    case CULL_STREAMING_SBAA_NDI:
    case CULL_COMPUTE_SHADER_TILE:
    case CULL_FORWARD_NONE:
    case CULL_FORWARD_PREZ_NONE:
        mDepthBufferStreaming->GetTexture()->GetDesc(&desc);
        break;
    default:
        mDepthBuffer->GetTexture()->GetDesc(&desc);
        break;
    }

    unsigned numElements = desc.Width * desc.Height * desc.SampleDesc.Count;
    unsigned depthSize = numElements * Texture2D::GetFormatSize(desc.Format);
    oss << "Depth buffer: " << BYTES_TO_MB(depthSize) << std::endl;
    total += depthSize;

    // Add the intermediate results buffer to total.
    // Our technique and the forward techniques do not have an intermediate buffer,
    //  as they output directly to the backbuffer.
    // Lauritzen's techniques output per-sample values to an MSAA "litBuffer."
    unsigned litSize = 0;
    switch (ui->lightCullTechnique) {
    case CULL_QUAD:
    case CULL_DEFERRED_NONE:
    case CULL_QUAD_DEFERRED_LIGHTING:
        litSize = mDeferredLightingAccumBuffer->GetSizeInBytes();
        break;
    case CULL_COMPUTE_SHADER_TILE:
        D3D11_BUFFER_DESC desc;
        mLitBufferCS->GetBuffer()->GetDesc(&desc);
        litSize = desc.ByteWidth;
        break;
    case CULL_STREAMING_SBAA:
    case CULL_STREAMING_SBAA_NDI:
    default:
        break;
    }

    if (litSize > 0) {
        oss << "Lit buffer: " << BYTES_TO_MB(litSize) << std::endl;
        total += litSize;
    }

    // Add the g-buffers to total.
    // Lauritzens deferred techniques all use the same g-buffer structures.
    // Our technique uses the merge node buffer and the count texture.
    switch (ui->lightCullTechnique) {
        case CULL_FORWARD_NONE:
        case CULL_FORWARD_PREZ_NONE:
            break;
        case CULL_DEFERRED_NONE:
        case CULL_QUAD:
        case CULL_QUAD_DEFERRED_LIGHTING:
        case CULL_COMPUTE_SHADER_TILE:
            for (unsigned i = 0; i < mGBuffer.size(); i++) {
                unsigned gbufferSize = mGBuffer.at(i)->GetSizeInBytes();
                oss << "Gbuffer" << i << ": " << BYTES_TO_MB(gbufferSize) << std::endl;
                total += gbufferSize;
            }
            break;
        case CULL_STREAMING_SBAA:
        case CULL_STREAMING_SBAA_NDI:
            D3D11_BUFFER_DESC desc;
            mMergeUav->GetBuffer()->GetDesc(&desc);
            unsigned mergeUavSize = desc.ByteWidth;
            total += mergeUavSize;
            oss << "Merge uav: " << BYTES_TO_MB(mergeUavSize) << std::endl;
            unsigned nodeCountSize = mCountTexture->GetSizeInBytes();
            total += nodeCountSize;
            oss << "Count texture: " << BYTES_TO_MB(nodeCountSize) << std::endl;
            break;
    }

    // Output final results.
    oss << "Total: " << BYTES_TO_MB(total) << std::endl;
    oss << "Memory per pixel: " << total / (mGBufferWidth * mGBufferHeight) << " (bytes)" << std::endl;
    total = total - litSize;

    return oss;
}