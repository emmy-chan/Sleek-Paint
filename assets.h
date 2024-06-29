#pragma once
#include <d3d11.h>

class cAssets {
public:
	ID3D11ShaderResourceView* selection_texture = NULL;
	ID3D11ShaderResourceView* bucket_texture = NULL;
	ID3D11ShaderResourceView* wand_texture = NULL;
	void LoadAssets();
};

extern cAssets g_assets;