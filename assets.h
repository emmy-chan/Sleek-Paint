#pragma once
#include <d3d11.h>
#include "imgui.h"
#include <map>
#include <string>
#include <functional>

// Forward declaration of BitmapFont to avoid circular dependency
struct BitmapFont;

class cAssets {
public:
    ID3D11ShaderResourceView* selection_texture = nullptr;
    ID3D11ShaderResourceView* bucket_texture = nullptr;
    ID3D11ShaderResourceView* wand_texture = nullptr;
    ID3D11ShaderResourceView* bandaid_texture = nullptr;

    bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
    void LoadAssets();
};

struct BitmapFont {
    ID3D11ShaderResourceView* texture;
    int charWidth;
    int charHeight;
    int columns;
    int rows;
    std::map<char, ImVec2> charUV;
    std::map<char, std::vector<std::vector<int>>> charBitmaps; // Add this to store character bitmaps

    BitmapFont(const char* filename, int charWidth, int charHeight, std::function<bool(const char*, ID3D11ShaderResourceView**, int*, int*)> loadTextureFunc);
    ~BitmapFont();

    void LoadCharacterBitmaps();
    void EnsureBitmapDimensions();
};

// Define global or member variables
inline BitmapFont* bitmapFont = nullptr;

extern cAssets g_assets;