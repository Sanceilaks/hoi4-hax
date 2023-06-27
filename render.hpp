#include <d3d11.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include "kiero.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void draw_menu();

namespace render {
    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

    HWND window = NULL;
    ID3D11Device* pDevice = NULL;
    ID3D11DeviceContext* pContext = NULL;
    ID3D11RenderTargetView* mainRenderTargetView;

    WNDPROC oWndProc;
    PresentFn oPresent;

    bool is_menu_open = false;

    auto init_imgui() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(pDevice, pContext);
    }

    LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
            is_menu_open = !is_menu_open;
        }

        auto imgui_handler = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        if (is_menu_open)
            return not imgui_handler;

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    bool init = false;
    HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        if (!init)
        {
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)& pDevice)))
            {
                pDevice->GetImmediateContext(&pContext);
                DXGI_SWAP_CHAIN_DESC sd;
                pSwapChain->GetDesc(&sd);
                window = sd.OutputWindow;
                ID3D11Texture2D* pBackBuffer;
                pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)& pBackBuffer);
                pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
                pBackBuffer->Release();
                oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
                init_imgui();
                init = true;
            }

            else
                return oPresent(pSwapChain, SyncInterval, Flags);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (is_menu_open) {
            draw_menu();
        }

        ImGui::Render();

        pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    inline auto hook() {
        kiero::init(kiero::RenderType::D3D11);
        kiero::bind(8, (void**)&oPresent, hkPresent);
    }

    inline auto unhook() {
        kiero::shutdown();
        ImGui::DestroyContext();
        mainRenderTargetView->Release();
        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        pContext->Release();
    }
}