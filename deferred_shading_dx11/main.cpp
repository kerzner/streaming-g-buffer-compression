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

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "App.h"
#include "ShaderDefines.h"
#include <sstream>
#include "Shaders\StreamingDefines.h"
#include "CameraPath.h"

// Constants
static const float kLightRotationSpeed = 0.05f;
static const float kSliderFactorResolution = 10000.0f;


enum SCENE_SELECTION {
    POWER_PLANT_SCENE,
    SPONZA_SCENE,
    TEAPOT_SCENE,
    GRASS_SCENE
};

enum {
    UI_TOGGLEFULLSCREEN,
    UI_TOGGLEWARP,
    UI_CHANGEDEVICE,
    UI_ANIMATELIGHT,
    UI_FACENORMALS,
    UI_SELECTEDSCENE,
    UI_VISUALIZELIGHTCOUNT,
    UI_VISUALIZEPERSAMPLESHADING,
    UI_LIGHTINGONLY,
    UI_LIGHTS,
    UI_LIGHTSTEXT,
    UI_LIGHTSPERPASS,
    UI_LIGHTSPERPASSTEXT,
    UI_CULLTECHNIQUE,
    UI_MSAA,
    UI_CAMERASPEEDTEXT,
    UI_CAMERASPEED,
    UI_SHOWMEMORY,
#if defined(STREAMING_DEBUG_OPTIONS)
    UI_EXECUTIONCOUNT,
    UI_MERGECOSTHETA,
    UI_STATS,
    UI_MERGEMETRIC,
    UI_AVERAGENORMALS,
    UI_AVERAGECOLOR,
    UI_DERIVATIVES,
    UI_DEPTH,
    UI_COMPARECOVERAGE,
    UI_COMPARENORMALS,
    UI_COMPAREDEPTH
#endif // defined(STREAMING_DEBUG_OPTIONS)
};

// List these top to bottom, since it is also the reverse draw order
enum {
    HUD_GENERIC = 0,
    HUD_PARTITIONS,
    HUD_FILTERING,
    HUD_EXPERT,
    HUD_NUM,
};


App* gApp = 0;

CFirstPersonCamera gViewerCamera;

CDXUTSDKMesh gMeshOpaque;
CDXUTSDKMesh gMeshAlpha;
D3DXMATRIXA16 gWorldMatrix;
ID3D11ShaderResourceView* gSkyboxSRV = 0;

// DXUT GUI stuff
CDXUTDialogResourceManager gDialogResourceManager;
CD3DSettingsDlg gD3DSettingsDlg;
CDXUTDialog gHUD[HUD_NUM];
CDXUTCheckBox* gAnimateLightCheck = 0;
CDXUTComboBox* gMSAACombo = 0;
CDXUTComboBox* gSceneSelectCombo = 0;
CDXUTComboBox* gCullTechniqueCombo = 0;
CDXUTSlider* gLightsSlider = 0;
CDXUTTextHelper* gTextHelper = 0;
CDXUTSlider* gCameraSpeedSlider = 0;
#if defined(STREAMING_DEBUG_OPTIONS)
CDXUTComboBox* gSlotCombo = 0;
CDXUTComboBox* gExecutionCombo = 0;
CDXUTSlider* gCosThetaSlider = 0;
CDXUTComboBox* gStatsCombo = 0;
CDXUTComboBox* gMergeMetricCombo = 0;
CDXUTComboBox* gDerivativesCombo = 0;
CDXUTComboBox* gDepthCombo = 0;
#endif // defined(STREAMING_DEBUG_OPTIONS)

float gAspectRatio;
bool gDisplayUI = true;
bool gZeroNextFrameTime = true;
int gNumCaptures = 0;
int gPrevLightCullTechnique;

// Any UI state passed directly to rendering shaders
UIConstants gUIConstants;

int gPrevMouse[2];

CameraPath* gCameraPath = 0;
bool gRecording = false;

bool gShowMemory = false;

bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext);
void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
                         void* userContext);
void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext);
void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
                                     void* userContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
                                         const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* userContext);
void CALLBACK OnD3D11DestroyDevice(void* userContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
                                 float elapsedTime, void* userContext);
void CALLBACK OnMouse(bool leftButtonDown, bool rightButtonDown, bool middleButtonDown, bool sideButton1Down,
                      bool sideButton2Down, int mouseWheelDelta, int xPos, int yPos, void* userContext);

void LoadSkybox(LPCWSTR fileName);

void InitApp(ID3D11Device* d3dDevice);
void DestroyApp();
void InitScene(ID3D11Device* d3dDevice);
void DestroyScene();

void InitUI();
void UpdateUIState();

void SaveCameraToFile();
void LoadCameraFromFile();
void PlayBackCameraPath(bool capture);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackFrameMove(OnFrameMove);
    DXUTSetCallbackMouse(OnMouse);

    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
    
    DXUTSetIsInGammaCorrectMode(true);

    DXUTInit(true, true, 0);
    InitUI();

    DXUTSetCursorSettings(true, true);
    DXUTSetHotkeyHandling(true, true, false);
    DXUTCreateWindow(L"Deferred Shading");
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 720);
    DXUTMainLoop();

    return DXUTGetExitCode();
}


void InitUI()
{
    // Setup default UI state
    // NOTE: All of these are made directly available in the shader constant buffer
    // This is convenient for development purposes.
    gUIConstants.lightingOnly = 0;
    gUIConstants.faceNormals = 0;
    gUIConstants.visualizeLightCount = 0;
    gUIConstants.visualizePerSampleShading = 0;
    gUIConstants.lightCullTechnique = CULL_COMPUTE_SHADER_TILE;
#if defined(STREAMING_DEBUG_OPTIONS)
    gUIConstants.executionCount = 0;
    gUIConstants.mergeCosTheta = 0.8f;
    gUIConstants.mergeMetric = MERGEMETRIC_NORMALS;
    gUIConstants.averageNormals = 1;
    gUIConstants.averageShade = 1;
    gUIConstants.derivatives = DERIVATIVES_AVERAGE;
    gUIConstants.depth = DEPTH_AVERAGE;
    gUIConstants.compareCoverage = 1;
    gUIConstants.compareDepth = 1;
    gUIConstants.compareNormals = 1;
#endif // defined(STREAMING_DEBUG_OPTIONS)
    gD3DSettingsDlg.Init(&gDialogResourceManager);

    for (int i = 0; i < HUD_NUM; ++i) {
        gHUD[i].Init(&gDialogResourceManager);
        gHUD[i].SetCallback(OnGUIEvent);
    }

    int width = 200;

    // Generic HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_GENERIC];
        int y = 0;

        HUD->AddButton(UI_TOGGLEFULLSCREEN, L"Toggle full screen", 0, y, width, 23);
        y += 26;

        // Warp doesn't support DX11 yet
        //HUD->AddButton(UI_TOGGLEWARP, L"Toggle WARP (F3)", 0, y, width, 23, VK_F3);
        //y += 26;

        HUD->AddButton(UI_CHANGEDEVICE, L"Change device (F2)", 0, y, width, 23, VK_F2);
        y += 26;

        HUD->AddComboBox(UI_MSAA, 0, y, width, 23, 0, false, &gMSAACombo);
        y += 26;
        gMSAACombo->AddItem(L"No MSAA", ULongToPtr(1));
        gMSAACombo->AddItem(L"2x MSAA", ULongToPtr(2));
        gMSAACombo->AddItem(L"4x MSAA", ULongToPtr(4));
        gMSAACombo->AddItem(L"8x MSAA", ULongToPtr(8));
        gMSAACombo->SetSelectedByData(ULongToPtr(8));

        HUD->AddComboBox(UI_SELECTEDSCENE, 0, y, width, 23, 0, false, &gSceneSelectCombo);
        y += 26;
        gSceneSelectCombo->AddItem(L"Power Plant", ULongToPtr(POWER_PLANT_SCENE));
        gSceneSelectCombo->AddItem(L"Sponza", ULongToPtr(SPONZA_SCENE));
        gSceneSelectCombo->AddItem(L"Teapot", ULongToPtr(TEAPOT_SCENE));
        gSceneSelectCombo->AddItem(L"Grass", ULongToPtr(GRASS_SCENE));
        gSceneSelectCombo->SetSelectedByIndex(2);

        HUD->AddCheckBox(UI_ANIMATELIGHT, L"Animate Lights", 0, y, width, 23, false, VK_SPACE, false, &gAnimateLightCheck);
        y += 26;

        HUD->AddCheckBox(UI_LIGHTINGONLY, L"Lighting Only", 0, y, width, 23, gUIConstants.lightingOnly != 0);
        y += 26;

        HUD->AddCheckBox(UI_FACENORMALS, L"Face Normals", 0, y, width, 23, gUIConstants.faceNormals != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZELIGHTCOUNT, L"Visualize Light Count", 0, y, width, 23, gUIConstants.visualizeLightCount != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZEPERSAMPLESHADING, L"Visualize Shading Freq.", 0, y, width, 23, gUIConstants.visualizePerSampleShading != 0);
        y += 26;

        HUD->AddStatic(UI_LIGHTSTEXT, L"Lights:", 0, y, width, 23);
        y += 26;
        HUD->AddSlider(UI_LIGHTS, 0, y, width, 23, 0, MAX_LIGHTS_POWER, MAX_LIGHTS_POWER, false, &gLightsSlider);
        gLightsSlider->SetValue(0);
        y += 26;

        HUD->AddStatic(UI_CAMERASPEEDTEXT, L"Camera speed:", 0, y, width, 23);
        y += 26;
        HUD->AddSlider(UI_CAMERASPEED, 0, y, width, 23, 1, 1500, 1, false, &gCameraSpeedSlider);
        gCameraSpeedSlider->SetValue(100);
        y += 26;

        HUD->AddComboBox(UI_CULLTECHNIQUE, 0, y, width, 23, 0, false, &gCullTechniqueCombo);
        y += 26;
        gCullTechniqueCombo->AddItem(L"No Cull Forward", ULongToPtr(CULL_FORWARD_NONE));
        gCullTechniqueCombo->AddItem(L"No Cull Pre-Z", ULongToPtr(CULL_FORWARD_PREZ_NONE));
        gCullTechniqueCombo->AddItem(L"No Cull Deferred", ULongToPtr(CULL_DEFERRED_NONE));
        gCullTechniqueCombo->AddItem(L"Quad", ULongToPtr(CULL_QUAD));
        gCullTechniqueCombo->AddItem(L"Quad Deferred Light", ULongToPtr(CULL_QUAD_DEFERRED_LIGHTING));
        gCullTechniqueCombo->AddItem(L"Compute Shader Tile", ULongToPtr(CULL_COMPUTE_SHADER_TILE));
        gCullTechniqueCombo->AddItem(L"Streaming SBAA", ULongToPtr(CULL_STREAMING_SBAA));
        gCullTechniqueCombo->AddItem(L"Streaming SBAA NDI", ULongToPtr(CULL_STREAMING_SBAA_NDI));
        gUIConstants.lightCullTechnique = CULL_STREAMING_SBAA_NDI;
        gCullTechniqueCombo->SetSelectedByData(ULongToPtr(gUIConstants.lightCullTechnique));

        HUD->AddCheckBox(UI_SHOWMEMORY, L"Show memory:", 0, y, width, 23, gShowMemory);
        y += 26;
#if defined(STREAMING_DEBUG_OPTIONS)

        HUD->AddComboBox(UI_EXECUTIONCOUNT, 0, y, width, 23, 0, false, &gExecutionCombo);
        gExecutionCombo->AddItem(L"All iter", LongToPtr(0));
        gExecutionCombo->AddItem(L"1 iter", LongToPtr(1));
        gExecutionCombo->AddItem(L"2 iter", LongToPtr(2));
        gExecutionCombo->AddItem(L"3 iter", LongToPtr(3));
        gExecutionCombo->AddItem(L"4 iter", LongToPtr(4));
        gExecutionCombo->AddItem(L"5 iter", LongToPtr(5));
        gExecutionCombo->AddItem(L"6 iter", LongToPtr(6));
        gExecutionCombo->AddItem(L"7 iter", LongToPtr(7));
        gExecutionCombo->AddItem(L"8 iter", LongToPtr(8));
        gExecutionCombo->AddItem(L"9 iter", LongToPtr(9));
        y += 26;

        HUD->AddStatic(UI_LIGHTSTEXT, L"Cos theta:", 0, y, width, 15);
        y += 26;
        HUD->AddSlider(UI_MERGECOSTHETA, 0, y, width, 23, 0, 1001, (int) (STREAMING_COS_THETA * 1000.0f), false, &gCosThetaSlider);
        y += 26;

        HUD->AddComboBox(UI_STATS, 0, y, width, 23, 0, false, &gStatsCombo);
        gStatsCombo->AddItem(L"No stats", LongToPtr(VISUALIZE_NONE));
        gStatsCombo->AddItem(L"Merges", LongToPtr(VISUALIZE_MERGES));
        gStatsCombo->AddItem(L"Discards", LongToPtr(VISUALIZE_DISCARDS));
        gStatsCombo->AddItem(L"Surfaces shaded", LongToPtr(VISUALIZE_SHADING));
        gStatsCombo->AddItem(L"Compression executions", LongToPtr(VISUALIZE_EXECUTIONS));

        y += 26;
        HUD->AddComboBox(UI_MERGEMETRIC, 0, y, width, 23, 0, false, &gMergeMetricCombo);
        gMergeMetricCombo->AddItem(L"Compare normals", LongToPtr(MERGEMETRIC_NORMALS));
        gMergeMetricCombo->AddItem(L"Compare derivatives", LongToPtr(MERGEMETRIC_DERIVATIVES));
        gMergeMetricCombo->AddItem(L"Compare both", LongToPtr(MERGEMETRIC_BOTH));

        y += 26;
        HUD->AddCheckBox(UI_AVERAGENORMALS, L"Avg shading normals", 0, y, width, 23, 1);

        y += 26;
        HUD->AddCheckBox(UI_AVERAGECOLOR, L"Avg color", 0, y, width, 23, 1);

        y += 26;
        HUD->AddComboBox(UI_DERIVATIVES, 0, y, width, 23, 0, false, &gDerivativesCombo);
        gDerivativesCombo->AddItem(L"Avg derivatives", LongToPtr(DERIVATIVES_AVERAGE));
        gDerivativesCombo->AddItem(L"Min derivatives", LongToPtr(DERIVATIVES_MIN));
        gDerivativesCombo->AddItem(L"Existing derivatives", LongToPtr(DERIVATIVES_NONE));

        y += 26;
        HUD->AddComboBox(UI_DEPTH, 0, y, width, 23, 0, false, &gDepthCombo);
        gDepthCombo->AddItem(L"Avg depth", LongToPtr(DEPTH_AVERAGE));
        gDepthCombo->AddItem(L"Max depth", LongToPtr(DEPTH_MAX));
        gDepthCombo->AddItem(L"Min depth", LongToPtr(DEPTH_MIN));
        gDepthCombo->AddItem(L"Existing depth", LongToPtr(DEPTH_NONE));

        y += 26;
        HUD->AddCheckBox(UI_COMPARECOVERAGE, L"Compare coverage", 0, y, width, 23, 1);
        y += 26;
        HUD->AddCheckBox(UI_COMPAREDEPTH, L"Compare depth", 0, y, width, 23, 1);
        y += 26;
        HUD->AddCheckBox(UI_COMPARENORMALS, L"Compare normals", 0, y, width, 23, 1);

#endif // defined(STREAMING_DEBUG_OPTIONS)

        HUD->SetSize(width, y);
    }

    // Expert HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_EXPERT];
        int y = 0;
    
        HUD->SetSize(width, y);

        // Initially hidden
        HUD->SetVisible(false);
    }
    
    UpdateUIState();
}


void UpdateUIState()
{
    //int technique = PtrToUint(gCullTechniqueCombo->GetSelectedData());
}


void InitApp(ID3D11Device* d3dDevice)
{
    DestroyApp();
    
    // Get current UI settings
    unsigned int msaaSamples = PtrToUint(gMSAACombo->GetSelectedData());
    gApp = new App(d3dDevice, 1 << gLightsSlider->GetValue(), msaaSamples);

    // Initialize with the current surface description
    gApp->OnD3D11ResizedSwapChain(d3dDevice, DXUTGetDXGIBackBufferSurfaceDesc());

    // Zero out the elapsed time for the next frame
    gZeroNextFrameTime = true;
}


void DestroyApp()
{
    SAFE_DELETE(gApp);
}


void LoadSkybox(ID3D11Device* d3dDevice, LPCWSTR fileName)
{
    ID3D11Resource* resource = 0;
    HRESULT hr;
    hr = D3DX11CreateTextureFromFile(d3dDevice, fileName, 0, 0, &resource, 0);
    assert(SUCCEEDED(hr));

    d3dDevice->CreateShaderResourceView(resource, 0, &gSkyboxSRV);
    resource->Release();
}


void InitScene(ID3D11Device* d3dDevice)
{
    DestroyScene();

    D3DXVECTOR3 cameraEye(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 cameraAt(0.0f, 0.0f, 0.0f);
    float sceneScaling = 1.0f;
    D3DXVECTOR3 sceneTranslation(0.0f, 0.0f, 0.0f);
    bool zAxisUp = false;

    SCENE_SELECTION scene = static_cast<SCENE_SELECTION>(PtrToUlong(gSceneSelectCombo->GetSelectedData()));
    switch (scene) {
        case POWER_PLANT_SCENE: {
            gMeshOpaque.Create(d3dDevice, L"..\\media\\powerplant\\powerplant.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\Clouds.dds");
            sceneScaling = 1.0f;
            cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;

        case SPONZA_SCENE: {
            gMeshOpaque.Create(d3dDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\Clouds.dds");
            sceneScaling = 0.05f;
            cameraEye = sceneScaling * D3DXVECTOR3(1200.0f, 200.0f, 100.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;

        case TEAPOT_SCENE: {
            gMeshOpaque.Create(d3dDevice, L"..\\media\\Teapot\\Teapot.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\Clouds.dds");
            sceneScaling = 1.0f;
            cameraEye = sceneScaling * D3DXVECTOR3(5.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;

        case GRASS_SCENE: {
            gMeshOpaque.Create(d3dDevice, L"..\\media\\Grass\\Grass.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\Clouds.dds");
            sceneScaling = 1.0f;
            cameraEye = sceneScaling * D3DXVECTOR3(5.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;
    };
    
    D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
    if (zAxisUp) {
        D3DXMATRIXA16 m;
        D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
        gWorldMatrix *= m;
    }
    {
        D3DXMATRIXA16 t;
        D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
        gWorldMatrix *= t;
    }

    gViewerCamera.SetViewParams(&cameraEye, &cameraAt);
    gViewerCamera.SetScalers(0.01f, 1.0f);
    gViewerCamera.FrameMove(0.0f);

    // Zero out the elapsed time for the next frame
    gZeroNextFrameTime = true;
}


void DestroyScene()
{
    gMeshOpaque.Destroy();
    gMeshAlpha.Destroy();
    SAFE_RELEASE(gSkyboxSRV);
}


bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext)
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        if (deviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE) {
            DXUTDisplaySwitchingToREFWarning(deviceSettings->ver);
        }
    }

    // We don't currently support framebuffer MSAA
    // Requires multi-frequency shading wrt. the GBuffer that is not yet implemented
    deviceSettings->d3d11.sd.SampleDesc.Count = 1;
    deviceSettings->d3d11.sd.SampleDesc.Quality = 0;

    // Also don't need a depth/stencil buffer... we'll manage that ourselves
    deviceSettings->d3d11.AutoCreateDepthStencil = false;

    return true;
}


void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext)
{
    if (gZeroNextFrameTime) {
        elapsedTime = 0.0f;
    }
    
    // Update the camera's position based on user input
    gViewerCamera.FrameMove(elapsedTime);

    // If requested, animate scene
    if (gApp && gAnimateLightCheck->GetChecked()) {
        gApp->Move(elapsedTime);
    }
}


LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
                          void* userContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *noFurtherProcessing = gDialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam );
    if (*noFurtherProcessing) {
        return 0;
    }

    // Pass messages to settings dialog if its active
    if (gD3DSettingsDlg.IsActive()) {
        gD3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    for (int i = 0; i < HUD_NUM; ++i) {
        *noFurtherProcessing = gHUD[i].MsgProc(hWnd, uMsg, wParam, lParam);
        if(*noFurtherProcessing) {
            return 0;
        }
    }

    // Pass all remaining windows messages to camera so it can respond to user input
    gViewerCamera.HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}


void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext)
{
    if(keyDown) {
        switch (character) {
        case VK_F8:
            // Toggle visibility of expert HUD
            gHUD[HUD_EXPERT].SetVisible(!gHUD[HUD_EXPERT].GetVisible());
            break;
        case VK_F9:
            // Toggle display of UI on/off
            gDisplayUI = !gDisplayUI;
            break;
        case 'K' :
            SaveCameraToFile();
            break;
        case 'L':
            LoadCameraFromFile();
            break;
        case 'J':
            gApp->HandleMouseEvent(DXUTGetD3D11DeviceContext(), gPrevMouse[0], gPrevMouse[1]);
            break;
        case 'P':
            char filename[128];
            sprintf_s(filename, "capture%d.tga", gNumCaptures++, sizeof(filename));
            gApp->SaveBackbufferToFile(DXUTGetD3D11DeviceContext(),
                                       DXUTGetD3D11RenderTargetView(),
                                       filename,
                                       DXGI_FORMAT_UNKNOWN);
            break;
        case 'T':
            gUIConstants.lightCullTechnique = gPrevLightCullTechnique;
            gPrevLightCullTechnique = static_cast<unsigned int>(PtrToUlong(gCullTechniqueCombo->GetSelectedData()));
            gCullTechniqueCombo->SetSelectedByData(ULongToPtr(gUIConstants.lightCullTechnique));
            break;
        case 'Z':
            if (!gRecording && !gCameraPath) {
                gCameraPath = new CameraPath();
                gRecording = true;
            } else if (!gRecording && gCameraPath) {
                SAFE_DELETE(gCameraPath);
                gCameraPath = new CameraPath();
                gRecording = true;
            } else {
                gRecording = false;
            }
            gCameraPath->SetActiveLights(gLightsSlider->GetValue());
            break;
        case 'X':
            PlayBackCameraPath(false);
            break;
        case 'C':
            gCameraPath->Save("camera.path");
            break;
        case 'V':
            if(!gCameraPath) {
                gCameraPath = new CameraPath();
            }
            gCameraPath->Load("camera.path");
            break;
        case 'B':
            PlayBackCameraPath(true);
            break;
        }
    }
}


void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext)
{
    switch (controlID) {
        case UI_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case UI_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case UI_CHANGEDEVICE:
            gD3DSettingsDlg.SetActive(!gD3DSettingsDlg.IsActive()); break;
        case UI_LIGHTINGONLY:
            gUIConstants.lightingOnly = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_FACENORMALS:
            gUIConstants.faceNormals = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_VISUALIZELIGHTCOUNT:
            gUIConstants.visualizeLightCount = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_VISUALIZEPERSAMPLESHADING:
            gUIConstants.visualizePerSampleShading = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_SELECTEDSCENE:
            DestroyScene(); break;
        case UI_LIGHTS:
            gApp->SetActiveLights(DXUTGetD3D11Device(), 1 << gLightsSlider->GetValue()); break;
        case UI_CULLTECHNIQUE:
            gPrevLightCullTechnique = gUIConstants.lightCullTechnique;
            gUIConstants.lightCullTechnique = static_cast<unsigned int>(PtrToUlong(gCullTechniqueCombo->GetSelectedData())); break;
        case UI_SHOWMEMORY:
            gShowMemory = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
#if defined(STREAMING_DEBUG_OPTIONS)
        case UI_EXECUTIONCOUNT:
            gUIConstants.executionCount = static_cast<int>(PtrToLong(gExecutionCombo->GetSelectedData())); break;
        case UI_MERGECOSTHETA:
            gUIConstants.mergeCosTheta = (float) gCosThetaSlider->GetValue() / 1000.f; break;
        case UI_STATS:
            gUIConstants.stats = static_cast<int>(PtrToLong(gStatsCombo->GetSelectedData())); break;
        case UI_MERGEMETRIC:
            gUIConstants.mergeMetric = static_cast<int>(PtrToLong(gMergeMetricCombo->GetSelectedData())); break;
        case UI_AVERAGECOLOR:
            gUIConstants.averageShade = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_AVERAGENORMALS:
            gUIConstants.averageNormals = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_DERIVATIVES:
            gUIConstants.derivatives = static_cast<int>(PtrToLong(gDerivativesCombo->GetSelectedData())); break;
        case UI_DEPTH:
            gUIConstants.depth = static_cast<int>(PtrToLong(gDepthCombo->GetSelectedData())); break;
        case UI_COMPARECOVERAGE:
            gUIConstants.compareCoverage = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_COMPAREDEPTH:
            gUIConstants.compareDepth = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_COMPARENORMALS:
            gUIConstants.compareNormals = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
#endif // defined(STREAMING_DEBUG_OPTIONS)
        case UI_CAMERASPEED:
            gViewerCamera.SetScalers(0.01f, gCameraSpeedSlider->GetValue()/100.0f); break;

        // These controls all imply changing parameters to the App constructor
        // (i.e. recreating resources and such), so we'll just clean up the app here and let it be
        // lazily recreated next render.
        case UI_MSAA:
            DestroyApp(); break;

        default:
            break;
    }

    UpdateUIState();
}


void CALLBACK OnD3D11DestroyDevice(void* userContext)
{
    DestroyApp();
    DestroyScene();
    
    gDialogResourceManager.OnD3D11DestroyDevice();
    gD3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(gTextHelper);
    SAFE_DELETE(gCameraPath);
}


HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
                                     void* userContext)
{    
    ID3D11DeviceContext* d3dDeviceContext = DXUTGetD3D11DeviceContext();
    gDialogResourceManager.OnD3D11CreateDevice(d3dDevice, d3dDeviceContext);
    gD3DSettingsDlg.OnD3D11CreateDevice(d3dDevice);
    gTextHelper = new CDXUTTextHelper(d3dDevice, d3dDeviceContext, &gDialogResourceManager, 15);
    
    gViewerCamera.SetRotateButtons(true, false, false);
    gViewerCamera.SetDrag(true);
    gViewerCamera.SetEnableYAxisMovement(true);

    return S_OK;
}


HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
                                          const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext)
{
    HRESULT hr;

    V_RETURN(gDialogResourceManager.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));
    V_RETURN(gD3DSettingsDlg.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));

    gAspectRatio = backBufferSurfaceDesc->Width / (float)backBufferSurfaceDesc->Height;

    // NOTE: Complementary Z (1-z) buffer used here, so swap near/far!
    gViewerCamera.SetProjParams(D3DX_PI / 4.0f, gAspectRatio, 300.0f, 0.05f);

    // Standard HUDs
    const int border = 20;
    int y = border;
    for (int i = 0; i < HUD_EXPERT; ++i) {
        gHUD[i].SetLocation(backBufferSurfaceDesc->Width - gHUD[i].GetWidth() - border, y);
        y += gHUD[i].GetHeight() + border;
    }

    // Expert HUD
    gHUD[HUD_EXPERT].SetLocation(border, 80);

    // If there's no app, it'll pick this up when it gets lazily created so just ignore it
    if (gApp) {
        gApp->OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc);
    }

    return S_OK;
}


void CALLBACK OnD3D11ReleasingSwapChain(void* userContext)
{
    gDialogResourceManager.OnD3D11ReleasingSwapChain();
}


void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
                                 float elapsedTime, void* userContext)
{
    if (gZeroNextFrameTime) {
        elapsedTime = 0.0f;
    }
    gZeroNextFrameTime = false;

    if (gD3DSettingsDlg.IsActive()) {
        gD3DSettingsDlg.OnRender(elapsedTime);
        return;
    }

    // Lazily create the application if need be
    if (!gApp) {
        InitApp(d3dDevice);
    }

    // Lazily load scene
    if (!gMeshOpaque.IsLoaded() && !gMeshAlpha.IsLoaded()) {
        InitScene(d3dDevice);
    }

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    
    D3D11_VIEWPORT viewport;
    viewport.Width    = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
    viewport.Height   = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    gApp->Render(d3dDeviceContext, pRTV, gMeshOpaque, gMeshAlpha, gSkyboxSRV,
        gWorldMatrix, &gViewerCamera, &viewport, &gUIConstants);

    if (gDisplayUI) {
        d3dDeviceContext->RSSetViewports(1, &viewport);

        // Render HUDs in reverse order
        d3dDeviceContext->OMSetRenderTargets(1, &pRTV, 0);
        for (int i = HUD_NUM - 1; i >= 0; --i) {
            gHUD[i].OnRender(elapsedTime);
        }

        // Render text
        gTextHelper->Begin();

        gTextHelper->SetInsertionPos(2, 0);
        gTextHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
        gTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        //gTextHelper->DrawTextLine(DXUTGetDeviceStats());

        // Output frame time
        {
            std::wostringstream oss;
            oss << 1000.0f / DXUTGetFPS() << " ms / frame";
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        // Output light info
        {
            std::wostringstream oss;
            oss << "Lights: " << gApp->GetActiveLights();
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        // Output recording status
        if (gRecording) {
            std::wostringstream oss;
            oss << "Recording camera path frame: " << gCameraPath->GetFrameCount();
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        // Output detailed times
        {
            std::wostringstream oss;
            oss = gApp->GetFrameTimes(d3dDeviceContext, &gUIConstants);
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        if (gShowMemory) {
            gTextHelper->SetInsertionPos(0, DXUTGetWindowHeight() - 200);
            std::wostringstream oss;
            unsigned total;
            oss = gApp->GetFrameMemoryUsage(&gUIConstants, total);
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        gTextHelper->End();
    }

    if (gRecording) {
        D3DXVECTOR3 eye = *gViewerCamera.GetEyePt();
        D3DXVECTOR3 at = *gViewerCamera.GetLookAtPt();
        gCameraPath->AddFrame(eye, at);
    }
}


void SaveCameraToFile()
{
    FILE* file;
    fopen_s(&file, "camera.txt", "w");

    D3DXVECTOR3 eye = *gViewerCamera.GetEyePt();
    D3DXVECTOR3 at = *gViewerCamera.GetLookAtPt();
    fprintf(file, "eye = %f, %f, %f\n", eye.x, eye.y, eye.z);
    fprintf(file, "at = %f, %f, %f\n", at.x, at.y, at.z);
    fprintf(file, "mouse = %i, %i\n", gPrevMouse[0], gPrevMouse[1]);
    fprintf(file, "samples = %i\n", gMSAACombo->GetSelectedIndex());
    fprintf(file, "lights = %i\n", gLightsSlider->GetValue());
    fclose(file);
}


void LoadCameraFromFile()
{
    FILE* file;
    fopen_s(&file, "camera.txt", "r");

    D3DXVECTOR3 eye, at;
    int msaaIndex, lights;
    fscanf_s(file, "eye = %f, %f, %f\n", &eye.x, &eye.y, &eye.z);
    fscanf_s(file, "at = %f, %f, %f\n", &at.x, &at.y, &at.z);
    fscanf_s(file, "mouse = %i, %i\n", &gPrevMouse[0], &gPrevMouse[1]);
    fscanf_s(file, "samples = %i\n", &msaaIndex);
    fscanf_s(file, "lights = %i\n", &lights);
    fclose(file);

    if (gMSAACombo->GetSelectedIndex() != msaaIndex) {
        gMSAACombo->SetSelectedByIndex(msaaIndex);
        DestroyApp();
    }

    gLightsSlider->SetValue(lights);
    gApp->SetActiveLights(DXUTGetD3D11Device(), 1 << gLightsSlider->GetValue());
    gViewerCamera.SetViewParams(&eye, &at);
}


void CALLBACK OnMouse(bool leftButtonDown, bool rightButtonDown, bool middleButtonDown, bool sideButton1Down,
                      bool sideButton2Down, int mouseWheelDelta, int xPos, int yPos, void* userContext)
{
    if (middleButtonDown) {
        gPrevMouse[0] = xPos;
        gPrevMouse[1] = yPos;
        gApp->HandleMouseEvent(DXUTGetD3D11DeviceContext(), gPrevMouse[0], gPrevMouse[1]);
    }
}


void PlayBackCameraPath(bool capture)
{
    gDisplayUI = false;
    gApp->SetActiveLights(DXUTGetD3D11Device(), 1 << gCameraPath->GetActiveLights());
    gLightsSlider->SetValue(gCameraPath->GetActiveLights());

    // File for recording frame times.
    FILE *statsFile = 0;
    fopen_s(&statsFile, "stats.txt", "w");
    std::wostringstream oss = gApp->GetFrameTimesHeader(&gUIConstants);
    char temp[128];
    size_t check;
    wcstombs_s(&check, temp, oss.str().c_str(), sizeof(temp));
    fprintf(statsFile, "%s", temp);

    int frame = 0;
    for(unsigned i = 0; i < gCameraPath->GetFrameCount(); i++) {
        CameraParams params = gCameraPath->GetFrame(i);
        gViewerCamera.SetViewParams(&params.eye, &params.at);
        DXUTRender3DEnvironment();

        if (capture) {
            char filename[128];
            sprintf_s(filename, "video/frame%d.tga", frame++, sizeof(filename));
            gApp->SaveBackbufferToFile(DXUTGetD3D11DeviceContext(),
                                       DXUTGetD3D11RenderTargetView(),
                                       filename,
                                       DXGI_FORMAT_UNKNOWN);

            oss = gApp->GetFrameTimes(DXUTGetD3D11DeviceContext(),
                                      &gUIConstants,
                                      false);
            wcstombs_s(&check, temp, oss.str().c_str(), sizeof(temp));
            fprintf(statsFile, "%s", temp);
        }
    }
    fclose(statsFile);
    gDisplayUI = true;
}