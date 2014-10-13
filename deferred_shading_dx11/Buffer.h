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

#include <d3d11.h>
#include <vector>

// NOTE: Ensure that T is exactly the same size/layout as the shader structure!
template <typename T>
class StructuredBuffer
{
public:
    // Construct a structured buffer
    StructuredBuffer(ID3D11Device* d3dDevice, int elements,
                     UINT bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                     bool dynamic = false);
    
    ~StructuredBuffer();

    ID3D11Buffer* GetBuffer() { return mBuffer; }
    ID3D11UnorderedAccessView* GetUnorderedAccess() { return mUnorderedAccess; }
    ID3D11ShaderResourceView* GetShaderResource() { return mShaderResource; }
    D3D11_MAPPED_SUBRESOURCE Map(ID3D11DeviceContext* d3dDeviceContext);

    // Only valid for dynamic buffers
    // TODO: Support NOOVERWRITE ring buffer?
    T* MapDiscard(ID3D11DeviceContext* d3dDeviceContext);
    void Unmap(ID3D11DeviceContext* d3dDeviceContext);

private:
    // Not implemented
    StructuredBuffer(const StructuredBuffer&);
    StructuredBuffer& operator=(const StructuredBuffer&);

    int mElements;
    ID3D11Buffer* mBuffer;
    ID3D11ShaderResourceView* mShaderResource;
    ID3D11UnorderedAccessView* mUnorderedAccess;
    ID3D11Buffer* mStagingBuffer;
    bool mMappedStaging, mMappedDiscard;
};


template <typename T>
StructuredBuffer<T>::StructuredBuffer(ID3D11Device* d3dDevice, int elements,
                                      UINT bindFlags, bool dynamic)
    : mElements(elements)
    , mShaderResource(0)
    , mUnorderedAccess(0)
    , mStagingBuffer(0)
    , mMappedDiscard(0)
    , mMappedStaging(0)
{
    CD3D11_BUFFER_DESC desc(sizeof(T) * elements, bindFlags,
        dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
        dynamic ? D3D11_CPU_ACCESS_WRITE : 0,
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
        sizeof(T));

    d3dDevice->CreateBuffer(&desc, 0, &mBuffer);

    if (bindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        d3dDevice->CreateUnorderedAccessView(mBuffer, 0, &mUnorderedAccess);
    }

    if (bindFlags & D3D11_BIND_SHADER_RESOURCE) {
        d3dDevice->CreateShaderResourceView(mBuffer, 0, &mShaderResource);
    }
}


template <typename T>
StructuredBuffer<T>::~StructuredBuffer()
{
    if (mUnorderedAccess) mUnorderedAccess->Release();
    if (mShaderResource) mShaderResource->Release();
    if (mStagingBuffer) mStagingBuffer->Release();
    mBuffer->Release();
}


template <typename T>
T* StructuredBuffer<T>::MapDiscard(ID3D11DeviceContext* d3dDeviceContext)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3dDeviceContext->Map(mBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    mMappedDiscard = true;
    return static_cast<T*>(mappedResource.pData);
}


template <typename T>
void StructuredBuffer<T>::Unmap(ID3D11DeviceContext* d3dDeviceContext)
{
    if(mMappedDiscard) {
        d3dDeviceContext->Unmap(mBuffer, 0);
    }

    if(mMappedStaging) {
        d3dDeviceContext->Unmap(mStagingBuffer, 0);
    }
}


template <typename T>
D3D11_MAPPED_SUBRESOURCE StructuredBuffer<T>::Map(ID3D11DeviceContext* d3dDeviceContext)
{
    D3D11_MAPPED_SUBRESOURCE subresource;
    HRESULT hr;
    ID3D11Device* d3dDevice;

    d3dDeviceContext->GetDevice(&d3dDevice);

    // Create a staging buffer
    if(!mStagingBuffer) {
        D3D11_BUFFER_DESC desc;
        mBuffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = 0;
        hr = d3dDevice->CreateBuffer(&desc, NULL, &mStagingBuffer);
        assert(SUCCEEDED(hr));
    }

    // Copy into staging buffer
    d3dDeviceContext->CopyResource(mStagingBuffer, mBuffer);
    hr = d3dDeviceContext->Map(mStagingBuffer, 0, D3D11_MAP_READ_WRITE, 0, &subresource);
    assert(SUCCEEDED(hr));

    mMappedStaging = true;
    SAFE_RELEASE(d3dDevice);

    return subresource;
}
// TODO: Constant buffers
