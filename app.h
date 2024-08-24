#pragma once
#include <memory>
#include <d3d11.h>
#include "imgui.h"

class cState {
public:
	virtual void Update() { };
};

class cApp {
public:
	// Data
	HWND windowHandle = NULL;
	ID3D11Device* g_pd3dDevice = NULL;
	//ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
	//IDXGISwapChain* g_pSwapChain = NULL;
	//ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
	//float elapsed_time;

	//std::unique_ptr<cState> main_state;
	std::unique_ptr<cState> ui_state;
};

extern cApp g_app;