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

#ifndef RENDERING_HLSL
#define RENDERING_HLSL

#include "FullScreenTriangle.hlsl"
#include "PerFrameConstants.hlsl"

//--------------------------------------------------------------------------------------
// Utility
//--------------------------------------------------------------------------------------
float linstep(float min, float max, float v)
{
    return saturate((v - min) / (max - min));
}


//--------------------------------------------------------------------------------------
// Geometry phase
//--------------------------------------------------------------------------------------
Texture2D gDiffuseTexture : register(t0);
SamplerState gDiffuseSampler : register(s0);

struct GeometryVSIn
{
    float3 position : position;
    float3 normal   : normal;
    float2 texCoord : texCoord;
};

struct GeometryVSOut
{
    float4 position     : SV_Position;
    float3 positionView : positionView;      // View space position
    float3 normal       : normal;
    float2 texCoord     : texCoord;
};

GeometryVSOut GeometryVS(GeometryVSIn input)
{
    GeometryVSOut output;

    output.position     = mul(float4(input.position, 1.0f), mCameraWorldViewProj);
    output.positionView = mul(float4(input.position, 1.0f), mCameraWorldView).xyz;
    output.normal       = mul(float4(input.normal, 0.0f), mCameraWorldView).xyz;
    output.texCoord     = input.texCoord;
    return output;
}

float3 ComputeFaceNormal(float3 position)
{
    return cross(ddx_coarse(position), ddy_coarse(position));
}

// Data that we can read or derive from the surface shader outputs
struct SurfaceData
{
    float3 positionView;         // View space position
    float3 positionViewDX;       // Screen space derivatives
    float3 positionViewDY;       // of view space position
    float3 normal;               // View space normal
    float4 albedo;
    float specularAmount;        // Treated as a multiplier on albedo
    float specularPower;
};

SurfaceData ComputeSurfaceDataFromGeometry(GeometryVSOut input)
{
    SurfaceData surface;
    surface.positionView = input.positionView;

    // These arguably aren't really useful in this path since they are really only used to
    // derive shading frequencies and composite derivatives but might as well compute them
    // in case they get used for anything in the future.
    // (Like the rest of these values, they will get removed by dead code elimination if
    // they are unused.)
    surface.positionViewDX = ddx_coarse(surface.positionView);
    surface.positionViewDY = ddy_coarse(surface.positionView);

    // Optionally use face normal instead of shading normal
    float3 faceNormal = ComputeFaceNormal(input.positionView);
    surface.normal = normalize(mUI.faceNormals ? faceNormal : input.normal);
    
    surface.albedo = gDiffuseTexture.Sample(gDiffuseSampler, input.texCoord);
    surface.albedo.rgb = mUI.lightingOnly ? float3(1.0f, 1.0f, 1.0f) : surface.albedo.rgb;

    // Map NULL diffuse textures to white
    uint2 textureDim;
    gDiffuseTexture.GetDimensions(textureDim.x, textureDim.y);
    surface.albedo = (textureDim.x == 0U ? float4(1.0f, 1.0f, 1.0f, 1.0f) : surface.albedo);

    // We don't really have art asset-related values for these, so set them to something
    // reasonable for now... the important thing is that they are stored in the G-buffer for
    // representative performance measurement.
    surface.specularAmount = 0.9f;
    surface.specularPower = 25.0f;

    return surface;
}


//--------------------------------------------------------------------------------------
// Lighting phase utilities
//--------------------------------------------------------------------------------------
struct PointLight
{
    float3 positionView;
    float attenuationBegin;
    float3 color;
    float attenuationEnd;
};

StructuredBuffer<PointLight> gLight : register(t5);

// As below, we separate this for diffuse/specular parts for convenience in deferred lighting
void AccumulatePhongBRDF(float3 normal,
                         float3 lightDir,
                         float3 viewDir,
                         float3 lightContrib,
                         float specularPower,
                         inout float3 litDiffuse,
                         inout float3 litSpecular)
{
    // Simple Phong
    float NdotL = dot(normal, lightDir);
    [flatten] if (NdotL > 0.0f) {
        float3 r = reflect(lightDir, normal);
        float RdotV = max(0.0f, dot(r, viewDir));
        float specular = pow(RdotV, specularPower);

        litDiffuse += lightContrib * NdotL;
        litSpecular += lightContrib * specular;
    }
}

// Accumulates separate "diffuse" and "specular" components of lighting from a given
// This is not possible for all BRDFs but it works for our simple Phong example here
// and this separation is convenient for deferred lighting.
// Uses an in-out for accumulation to avoid returning and accumulating 0
void AccumulateBRDFDiffuseSpecular(SurfaceData surface, PointLight light,
                                   inout float3 litDiffuse,
                                   inout float3 litSpecular)
{
    float3 directionToLight = light.positionView - surface.positionView;
    float distanceToLight = length(directionToLight);

    [branch] if (distanceToLight < light.attenuationEnd) {
        float attenuation = linstep(light.attenuationEnd, light.attenuationBegin, distanceToLight);
        directionToLight *= rcp(distanceToLight);       // A full normalize/RSQRT might be as fast here anyways...
        
        AccumulatePhongBRDF(surface.normal, directionToLight, normalize(surface.positionView),
            attenuation * light.color, surface.specularPower, litDiffuse, litSpecular);
    }
}

// Uses an in-out for accumulation to avoid returning and accumulating 0
void AccumulateBRDF(SurfaceData surface, PointLight light,
                    inout float3 lit)
{
    float3 directionToLight = light.positionView - surface.positionView;
    float distanceToLight = length(directionToLight);

    [branch] if (distanceToLight < light.attenuationEnd) {
        float attenuation = linstep(light.attenuationEnd, light.attenuationBegin, distanceToLight);
        directionToLight *= rcp(distanceToLight);       // A full normalize/RSQRT might be as fast here anyways...

        float3 litDiffuse = float3(0.0f, 0.0f, 0.0f);
        float3 litSpecular = float3(0.0f, 0.0f, 0.0f);
        AccumulatePhongBRDF(surface.normal, directionToLight, normalize(surface.positionView),
            attenuation * light.color, surface.specularPower, litDiffuse, litSpecular);
        
        lit += surface.albedo.rgb * (litDiffuse + surface.specularAmount * litSpecular);
    }
}


#endif // RENDERING_HLSL
