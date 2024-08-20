#include "assets.h"
#include "app.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"

cAssets g_assets = cAssets();

// Simple helper function to load an image into a DX11 texture with common settings
bool cAssets::LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    // Load from disk into a raw RGBA buffer
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_app.g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_app.g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    // Set the texture dimensions
    if (out_width) *out_width = image_width;
    if (out_height) *out_height = image_height;

    return true;
}

BitmapFont::BitmapFont(const char* filename, int charWidth, int charHeight, std::function<bool(const char*, ID3D11ShaderResourceView**, int*, int*)> loadTextureFunc)
    : charWidth(charWidth), charHeight(charHeight) {
    int textureWidth, textureHeight;
    bool result = loadTextureFunc(filename, &texture, &textureWidth, &textureHeight);
    if (!result) {
        // Handle the error, texture loading failed
    }
    this->columns = 16; // 16 columns for your 16x16 character grid
    this->rows = 3;     // 3 rows for upper and lowercase characters

    // Calculate UV coordinates for each character
    for (int i = 0; i < 52; ++i) { // 26 uppercase + 26 lowercase characters
        int x = i % columns;
        int y = i / columns;
        char c = (i < 26) ? ('A' + i) : ('a' + (i - 26));
        charUV[c] = ImVec2(x * charWidth / (float)textureWidth, y * charHeight / (float)textureHeight);
    }

    LoadCharacterBitmaps(); // Load character bitmaps
    EnsureBitmapDimensions(); // Ensure bitmaps have correct dimensions
}

BitmapFont::~BitmapFont() {
    if (texture) {
        texture->Release();
    }
}

void BitmapFont::LoadCharacterBitmaps() {
    // Uppercase characters
    charBitmaps['A'] = {
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['B'] = {
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0}
    };

    charBitmaps['C'] = {
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['D'] = {
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0}
    };

    charBitmaps['E'] = {
        {1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1}
    };

    charBitmaps['F'] = {
        {1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0}
    };

    charBitmaps['G'] = {
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 1, 1, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 1}
    };

    charBitmaps['H'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['I'] = {
        {1, 1, 1, 1, 1, 1},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {1, 1, 1, 1, 1, 1}
    };

    charBitmaps['J'] = {
        {0, 0, 0, 0, 1, 1},
        {0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['K'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0},
        {1, 0, 0, 1, 0, 0},
        {1, 0, 1, 0, 0, 0},
        {1, 0, 0, 1, 0, 0},
        {1, 0, 0, 0, 1, 0}
    };

    charBitmaps['L'] = {
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1}
    };

    charBitmaps['M'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 1, 0, 0, 1, 1},
        {1, 0, 1, 1, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['N'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 1, 0, 0, 0, 1},
        {1, 0, 1, 0, 0, 1},
        {1, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 1, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['O'] = {
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['P'] = {
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0}
    };

    charBitmaps['Q'] = {
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 1},
        {1, 0, 0, 1, 0, 1},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['R'] = {
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 1, 0, 0},
        {1, 0, 0, 0, 1, 0},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['S'] = {
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0},
        {0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['T'] = {
        {1, 1, 1, 1, 1, 1},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0}
    };

    charBitmaps['U'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['V'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 0, 1, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0, 0}
    };

    charBitmaps['W'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 1, 0, 1},
        {1, 0, 1, 0, 1, 0},
        {1, 1, 0, 0, 1, 0},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['X'] = {
        {1, 0, 0, 0, 0, 1},
        {0, 1, 0, 0, 1, 0},
        {0, 1, 0, 0, 1, 0},
        {0, 0, 1, 1, 0, 0},
        {0, 1, 0, 0, 1, 0},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['Y'] = {
        {1, 0, 0, 0, 0, 1},
        {0, 1, 0, 0, 1, 0},
        {0, 0, 1, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0}
    };

    charBitmaps['Z'] = {
        {1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 1, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 1, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1}
    };

    // Lowercase characters
    charBitmaps['a'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['b'] = {
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0}
    };

    charBitmaps['c'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 1, 0},
        {0, 1, 0, 0, 0, 1},
        {0, 1, 0, 0, 0, 0},
        {0, 1, 0, 0, 0, 1},
        {0, 0, 1, 1, 1, 0}
    };

    charBitmaps['d'] = {
        {0, 0, 0, 0, 0, 1},
        {0, 0, 1, 1, 1, 1},
        {0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 1}
    };

    charBitmaps['e'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 1, 0},
        {0, 1, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 1},
        {0, 1, 0, 0, 0, 0},
        {0, 0, 1, 1, 1, 0}
    };

    charBitmaps['f'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0}
    };

    charBitmaps['g'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 1, 1},
        {0, 1, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0}
    };

    charBitmaps['h'] = {
        {1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['i'] = {
        {0, 0, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['j'] = {
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {1, 1, 1, 0, 0, 0}
    };

    charBitmaps['k'] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0},
        {1, 0, 0, 1, 0, 0},
        {1, 0, 1, 0, 0, 0},
        {1, 0, 0, 1, 0, 0},
        {1, 0, 0, 0, 1, 0}
    };

    charBitmaps['l'] = {
        {0, 0, 1, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
        {0, 1, 1, 1, 1, 0}
    };

    charBitmaps['m'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 1, 0, 1, 0, 0},
        {1, 0, 1, 0, 1, 0},
        {1, 0, 1, 0, 1, 0},
        {1, 0, 1, 0, 1, 0},
        {1, 0, 1, 0, 1, 0}
    };

    charBitmaps['n'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['o'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 1, 0},
        {0, 1, 0, 0, 0, 1},
        {0, 1, 0, 0, 0, 1},
        {0, 1, 0, 0, 0, 1},
        {0, 0, 1, 1, 1, 0}
    };

    charBitmaps['p'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0}
    };

    charBitmaps['q'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 1}
    };

    charBitmaps['r'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0}
    };

    charBitmaps['s'] = {
        {0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0},
        {1, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0},
        {0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0}
    };

    charBitmaps['t'] = {
        {0, 0, 1, 0, 0, 0},
        {0, 1, 1, 1, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 0, 1, 1, 1, 0}
    };

    charBitmaps['u'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 1}
    };

    charBitmaps['v'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 0, 1, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0, 0}
    };

    charBitmaps['w'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 1, 0, 1},
        {1, 0, 1, 0, 1, 1},
        {0, 1, 0, 0, 0, 1}
    };

    charBitmaps['x'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 0, 0, 1, 0},
        {0, 0, 1, 1, 0, 0},
        {0, 1, 0, 0, 1, 0},
        {1, 0, 0, 0, 0, 1}
    };

    charBitmaps['y'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0}
    };

    charBitmaps['z'] = {
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 1, 0},
        {0, 0, 1, 1, 0, 0},
        {0, 1, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1}
    };
}

void BitmapFont::EnsureBitmapDimensions() {
    for (auto& pair : charBitmaps) {
        auto& bitmap = pair.second;
        int currentCharHeight = bitmap.size();
        int currentCharWidth = bitmap.empty() ? 0 : bitmap[0].size();

        // Adjust rows if necessary
        while (currentCharHeight < charHeight) {
            bitmap.push_back(std::vector<int>(currentCharWidth, 0));
            currentCharHeight++;
        }
        while (currentCharHeight > charHeight) {
            bitmap.pop_back();
            currentCharHeight--;
        }

        // Adjust columns if necessary
        for (auto& row : bitmap) {
            while (row.size() < currentCharWidth) {
                row.push_back(0);
            }
            while (row.size() > currentCharWidth) {
                row.pop_back();
            }
        }
    }
}

void cAssets::LoadAssets()
{
    bool ret = LoadTextureFromFile("gfx/selection.png", &g_assets.selection_texture, NULL, NULL);
    IM_ASSERT(ret);

    bool ret2 = LoadTextureFromFile("gfx/wand.png", &g_assets.wand_texture, NULL, NULL);
    IM_ASSERT(ret2);

    bool ret3 = LoadTextureFromFile("gfx/bucket2.png", &g_assets.bucket_texture, NULL, NULL);
    IM_ASSERT(ret3);

    bool ret4 = LoadTextureFromFile("gfx/bandaid.png", &g_assets.bandaid_texture, NULL, NULL);
    IM_ASSERT(ret4);

    cAssets assets;
    bitmapFont = new BitmapFont("gfx/fontsmall1.png", 6, 6, std::bind(&cAssets::LoadTextureFromFile, &assets, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
}
