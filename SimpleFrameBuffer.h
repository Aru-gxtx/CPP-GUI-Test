#pragma once
#include <d3d11.h>
#include <stdio.h>

class SimpleFrameBuffer {
public:
    ID3D11RenderTargetView* RenderTargetView = nullptr;
    ID3D11ShaderResourceView* ShaderResourceView = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
    D3D11_VIEWPORT Viewport = {};

    SimpleFrameBuffer(ID3D11Device* device, int width, int height) {
        Resize(device, width, height);
    }

    void Resize(ID3D11Device* device, int width, int height) {
        if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
        if (ShaderResourceView) { ShaderResourceView->Release(); ShaderResourceView = nullptr; }
        if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D* pTexture = nullptr;
        device->CreateTexture2D(&texDesc, nullptr, &pTexture);

        device->CreateRenderTargetView(pTexture, nullptr, &RenderTargetView);
        device->CreateShaderResourceView(pTexture, nullptr, &ShaderResourceView);
        pTexture->Release();

        D3D11_TEXTURE2D_DESC depthDesc = texDesc;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ID3D11Texture2D* pDepthTexture = nullptr;
        device->CreateTexture2D(&depthDesc, nullptr, &pDepthTexture);
        device->CreateDepthStencilView(pDepthTexture, nullptr, &DepthStencilView);
        pDepthTexture->Release();

        Viewport.TopLeftX = 0; Viewport.TopLeftY = 0;
        Viewport.Width = (float)width; Viewport.Height = (float)height;
        Viewport.MinDepth = 0.0f; Viewport.MaxDepth = 1.0f;
    }

    void Bind(ID3D11DeviceContext* context) {
        context->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
        context->RSSetViewports(1, &Viewport);
    }

    void Unbind(ID3D11DeviceContext* context, ID3D11RenderTargetView* mainRTV) {
        context->OMSetRenderTargets(1, &mainRTV, nullptr);
    }
};