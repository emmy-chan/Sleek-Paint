#include "canvas.h"
#include "camera.h"
#include "utils.h"

//Get Key State -_-
#include <WinUser.h>

#include <fstream>
#include <string>
#include "data_manager.h"

std::vector<cCanvas> g_canvas = std::vector<cCanvas>();
uint8_t g_cidx = uint8_t();

#include <utility> // for std::pair
#include <set>
#include "keystate.h"
#include "assets.h"
#include <iostream>
#include <execution>
#include "ui_states.h"

void cCanvas::CreateCanvasTexture(ID3D11Device* device, uint32_t width, uint32_t height) {
    if (!device) {
        printf("Error: Device is null.\n"); return;
    }

    // Release existing resources if they exist
    if (canvasTexture) {
        canvasTexture->Release(); canvasTexture = nullptr;
    }
    if (canvasSRV) {
        canvasSRV->Release(); canvasSRV = nullptr;
    }

    // Ensure valid dimensions
    if (width == 0 || height == 0) {
        printf("Error: Invalid texture dimensions.\n"); return;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateTexture2D(&desc, NULL, &canvasTexture);
    if (FAILED(hr) || !canvasTexture) {
        printf("Failed to create texture. HRESULT: 0x%08X\n", hr); return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(canvasTexture, &srvDesc, &canvasSRV);
    if (FAILED(hr) || !canvasSRV) {
        printf("Failed to create shader resource view. HRESULT: 0x%08X\n", hr); return;
    }
}

void cCanvas::Initialize(const std::vector<ImU32>& initial_data, const uint16_t& new_width, const uint16_t& new_height, const ImU32& color) {
    CreateCanvasTexture(g_app.g_pd3dDevice, new_width, new_height);

    // Initialize canvas dimensions
    width = new_width;
    height = new_height;

    // Create new layer
    NewLayer("Layer 1", initial_data, color);

    // Zoom out if the image is larger than our display
    while (TILE_SIZE > 1 && width * TILE_SIZE > ImGui::GetIO().DisplaySize.x || TILE_SIZE > 1 && height * TILE_SIZE > ImGui::GetIO().DisplaySize.y)
        TILE_SIZE--;

    // Calculate the canvas size in pixels
    const float canvasWidthInPixels = float(width * TILE_SIZE);
    const float canvasHeightInPixels = float(height * TILE_SIZE);

    // Adjust camera centering logic based on the actual window size
    const float windowWidth = ImGui::GetIO().DisplaySize.x;
    const float windowHeight = ImGui::GetIO().DisplaySize.y;

    // Define the left UI panel width
    const float leftUIPanelWidth = 197.0f; // Adjust this value based on your actual left panel width

    // Add space for scrollbar thickness (adjust based on your scrollbar size)
    const float horizontalScrollbarThickness = 60.0f;
    const float verticalScrollbarThickness = 20.0f;

    // Calculate horizontal and vertical middle points, accounting for the left UI panel and scrollbars
    const float horizontalCenter = -(canvasWidthInPixels - (windowWidth + leftUIPanelWidth) + horizontalScrollbarThickness) / 2.0f;
    const float verticalCenter = -(canvasHeightInPixels - windowHeight - verticalScrollbarThickness) / 2.0f;

    // Set the camera to the calculated centered position
    g_cam.x = horizontalCenter;
    g_cam.y = verticalCenter;
}

void cCanvas::NewLayer(std::string name, const std::vector<ImU32>& initial_data, ImU32 color) {
    // Check if the number of layers is less than 100
    if (tiles.size() >= 99) {
        printf("Max number of layers exceeded!");
        return;
    }

    std::vector<ImU32> layer0;

    // If initial data is provided, use it to initialize the layer
    if (!initial_data.empty())
        layer0 = initial_data;
    else for (int i = 0; i < width * height; i++) layer0.push_back(color); // Create a blank canvas

    tiles.push_back(layer0);
    layerVisibility.push_back(true);
    const std::string layerName = name.empty() ? "Layer " + std::to_string(tiles.size()) : name;
    layerNames.push_back(layerName);
    layerOpacity.push_back(255);
}

void cCanvas::Clear() {
    for (auto& tile : g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex]) tile = IM_COL32(255, 255, 255, 0);
}

void cCanvas::AdaptNewSize(int width, int height) {
    // Create a temporary container for the resized image
    std::vector<std::vector<ImU32>> newLayers(g_canvas[g_cidx].tiles.size(), std::vector<ImU32>(width * height, IM_COL32_BLACK_TRANS));

    // Calculate scaling factors
    const float scaleX = static_cast<float>(width) / g_canvas[g_cidx].width, scaleY = static_cast<float>(height) / g_canvas[g_cidx].height;

    // Iterate over all layers and adapt the content
    for (size_t layerIndex = 0; layerIndex < g_canvas[g_cidx].tiles.size(); ++layerIndex) {
        const auto& oldLayer = g_canvas[g_cidx].tiles[layerIndex];
        auto& newLayer = newLayers[layerIndex];

        // Map old pixels to new positions with scaling
        for (int y = 0; y < height; ++y) {
            #pragma omp simd
            for (int x = 0; x < width; ++x) {
                const int oldX = static_cast<int>(x / scaleX), oldY = static_cast<int>(y / scaleY);
                if (oldX < g_canvas[g_cidx].width && oldY < g_canvas[g_cidx].height) {
                    const int oldIndex = oldX + oldY * g_canvas[g_cidx].width, newIndex = x + y * width;

                    if (oldIndex < oldLayer.size() && newIndex < newLayer.size())
                        newLayer[newIndex] = oldLayer[oldIndex];
                }
            }
        }
    }

    // Replace old layers with new layers
    g_canvas[g_cidx].tiles = std::move(newLayers);
    g_canvas[g_cidx].width = width;
    g_canvas[g_cidx].height = height;
    g_canvas[g_cidx].CreateCanvasTexture(g_app.g_pd3dDevice, g_canvas[g_cidx].width, g_canvas[g_cidx].height);
}

// Function to update the canvas history
void cCanvas::UpdateCanvasHistory() {
    // Ensure g_cidx is valid
    if (g_cidx >= g_canvas.size()) {
        printf("Error: Canvas index out of bounds. g_cidx: %d, g_canvas size: %zu\n", g_cidx, g_canvas.size());
        return;
    }

    // Ensure tiles is initialized and selLayerIndex is valid
    if (tiles.empty() || g_canvas[g_cidx].selLayerIndex >= tiles.size()) {
        printf("Error: Tiles not properly initialized or selLayerIndex out of bounds. Tiles size: %zu, selLayerIndex: %d\n", tiles.size(), g_canvas[g_cidx].selLayerIndex);
        return;
    }

    // Ensure selected layer is not empty
    if (tiles[g_canvas[g_cidx].selLayerIndex].empty()) {
        printf("Error: Selected layer is empty. Layer index: %d\n", g_canvas[g_cidx].selLayerIndex);
        return;
    }

    // Check if there are previous states and the current state index is valid
    if (previousCanvases.size() > 1 && previousCanvases.size() > canvas_idx)
        previousCanvases.resize(canvas_idx + 1);

    // Only add the current state to history if it's different from the last saved state
    if (previousCanvases.empty() || (!tiles[g_canvas[g_cidx].selLayerIndex].empty() && !previousCanvases.empty() &&
        !g_util.IsTilesEqual(tiles[g_canvas[g_cidx].selLayerIndex], g_util.DecompressCanvasDataRLE(previousCanvases.back())))) {

        // Compress current canvas data
        auto compressedData = g_util.CompressCanvasDataZlib(g_util.ConvertLayerToByteArray(tiles[g_canvas[g_cidx].selLayerIndex]));
        size_t originalSize = tiles[g_canvas[g_cidx].selLayerIndex].size() * sizeof(ImU32);

        // Add the new state to history
        previousCanvases.push_back(compressedData);
        canvas_idx = (uint8_t)previousCanvases.size() - 1;

        printf("Canvas state created with RLE compression.\n");
    }
    else
        printf("No changes detected; canvas state not updated.\n");
}

void cCanvas::LoadColorPalette(std::string input) {
    myCols.clear();

    // Load color palette!
    DataManager data;
    data.LoadColorData(input, myCols);
}

void cCanvas::DestroyCanvas() {
    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex].clear();
    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex].shrink_to_fit();
    g_canvas[g_cidx].myCols.clear();
    g_canvas[g_cidx].myCols.shrink_to_fit();
    g_canvas.erase(g_canvas.begin() + g_cidx);
    g_canvas.shrink_to_fit();
    if (g_cidx == g_canvas.size()) g_cidx--; // Prevent our canvas index from going out of bounds
}

// Function to calculate the bounding box of the selected indexes
ImVec2 GetTilePos(int64_t index, float tileSize, float camX, float camY, int width, int height) {
    // Calculate the total number of tiles
    const int64_t totalTiles = int64_t(width) * int64_t(height);

    // Clamp the index to ensure it is within valid bounds
    index = glm::clamp(index, int64_t(0), totalTiles - 1);

    // Calculate the X and Y position of the tile
    return ImVec2(camX + float(index % width) * tileSize, camY + float(index / width) * tileSize);
}

void DrawSelectionRectangle(const std::vector<int>& indexes, float tileSize, float camX, float camY, int width, int height, ImU32 col, uint8_t thickness) {
    if (indexes.empty()) return;

    std::unordered_map<uint64_t, ImVec2> tilePositions;
    tilePositions.reserve(indexes.size());  // Reserve space in advance to avoid resizing

    // Get the positions of all selected tiles
    for (uint64_t index : indexes)
        tilePositions[index] = GetTilePos(index, tileSize, camX, camY, width, height);

    // Check if a tile is at the edge of the canvas
    auto isBorderTile = [&](uint64_t index, int dx, int dy) {
        // Calculate the neighboring index
        const int neighborX = int(index % width) + dx, neighborY = int(index / width) + dy;

        // Ensure the neighbor is within canvas bounds
        if (neighborX < 0 || neighborX >= width || neighborY < 0 || neighborY >= height)
            return true;  // Tile is at the canvas edge

        // Check if the neighboring tile is part of the selection
        const uint64_t neighborIndex = index + dx + dy * width;
        return std::find(indexes.begin(), indexes.end(), neighborIndex) == indexes.end();
    };

    std::vector<std::pair<ImVec2, ImVec2>> borderLines;  // Store border line pairs

    // For each tile, add borders based on its neighbors
    for (const auto& [index, pos] : tilePositions) {
        ImVec2 topLeft = ImVec2(std::floor(pos.x), std::floor(pos.y));
        ImVec2 bottomRight = ImVec2(std::floor(pos.x + tileSize), std::floor(pos.y + tileSize));

        // Add lines for the borders if there are no neighboring tiles in that direction
        if (isBorderTile(index, -1, 0))  // Left border
            borderLines.emplace_back(topLeft, ImVec2(topLeft.x, bottomRight.y));

        if (isBorderTile(index, 1, 0))  // Right border
            borderLines.emplace_back(ImVec2(bottomRight.x, topLeft.y), bottomRight);

        if (isBorderTile(index, 0, -1))  // Top border
            borderLines.emplace_back(topLeft, ImVec2(bottomRight.x, topLeft.y));

        if (isBorderTile(index, 0, 1))  // Bottom border
            borderLines.emplace_back(ImVec2(topLeft.x, bottomRight.y), bottomRight);
    }

    // Draw all borders in one pass
    for (const auto& [p1, p2] : borderLines) {
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2(std::round(p1.x), std::round(p1.y)), ImVec2(std::round(p2.x), std::round(p2.y)), IM_COL32_BLACK, float(thickness * 2));  // Outline
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2(std::round(p1.x), std::round(p1.y)), ImVec2(std::round(p2.x), std::round(p2.y)), col, float(thickness));  // Main color
    }
}

// Check if there is an image available in the clipboard
bool cCanvas::IsImageInClipboard() {
    if (!OpenClipboard(nullptr)) return false; // Open the clipboard
    const bool hasImage = IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5);
    CloseClipboard(); // Close the clipboard
    return hasImage;
}

std::vector<ImU32> GetImageFromClipboard(uint16_t& outWidth, uint16_t& outHeight) {
    std::vector<ImU32> imageData;

    if (!OpenClipboard(nullptr)) return imageData;

    HANDLE hClipboardData = GetClipboardData(CF_DIB);
    if (hClipboardData == nullptr) {
        CloseClipboard();
        return imageData;
    }

    BITMAPINFO* pBitmapInfo = (BITMAPINFO*)GlobalLock(hClipboardData);
    if (pBitmapInfo == nullptr) {
        CloseClipboard();
        return imageData;
    }

    outWidth = pBitmapInfo->bmiHeader.biWidth;
    outHeight = abs(pBitmapInfo->bmiHeader.biHeight);
    const bool isBottomUp = (pBitmapInfo->bmiHeader.biHeight > 0);

    BYTE* pPixels = (BYTE*)pBitmapInfo + pBitmapInfo->bmiHeader.biSize + pBitmapInfo->bmiHeader.biClrUsed * sizeof(RGBQUAD);

    imageData.resize(outWidth * outHeight);

    #pragma omp simd
    for (int y = 0; y < outHeight; y++) {
        int sourceY = isBottomUp ? (outHeight - 1 - y) : y;
        #pragma omp simd
        for (int x = 0; x < outWidth; x++) {
            BYTE* pPixel = pPixels + (sourceY * outWidth + x) * 4; // Assuming 32-bit DIB
            const BYTE r = pPixel[2];
            const BYTE g = pPixel[1];
            const BYTE b = pPixel[0];
            const BYTE a = pPixel[3];

            imageData[y * outWidth + x] = IM_COL32(r, g, b, a);
        }
    }

    GlobalUnlock(hClipboardData);
    CloseClipboard();
    return imageData;
}

void cCanvas::PasteImageFromClipboard() {
    uint16_t imageWidth, imageHeight;
    const std::vector<ImU32> imageData = GetImageFromClipboard(imageWidth, imageHeight);

    if (imageData.empty()) {
        printf("No image data available in clipboard.\n");
        return;
    }

    if (imageWidth > width || imageHeight > height) {
        printf("Image from clipboard is too large for the canvas.\n");
        return;
    }

    printf("Pasting image of width %d and height %d onto canvas of width %d and height %d.\n", imageWidth, imageHeight, width, height);

    // Create a new layer for the image data
    std::vector<ImU32> imageLayer(width * height, IM_COL32_BLACK_TRANS); // Initialize with transparent color

    // Instead of manually copying pixel by pixel, we can use std::copy
    #pragma omp simd
    for (int y = 0; y < imageHeight; y++) {
        const int imageStartIndex = y * imageWidth, canvasStartIndex = y * width;

        // Copy each row of image data to the corresponding location in the canvas
        std::copy(imageData.begin() + imageStartIndex, imageData.begin() + imageStartIndex + imageWidth, imageLayer.begin() + canvasStartIndex);
    }

    // Add the new layer with the image data
    tiles.push_back(imageLayer);
    layerVisibility.push_back(true);
    const std::string layerName = "Layer " + std::to_string(tiles.size());
    layerNames.push_back(layerName);
    layerOpacity.push_back(255);
    g_canvas[g_cidx].selLayerIndex = g_canvas[g_cidx].tiles.size() - (size_t)1; // Set to the newly created layer

    // Update canvas history for undo functionality
    UpdateCanvasHistory();
    paintToolSelected = TOOL_MOVE;
    printf("Image pasted successfully.\n");
}

void cCanvas::PasteSelection() {
    // Ensure there is something to paste
    if (copiedTiles.empty()) {
        printf("PasteSelection: No copied tiles to paste.\n");
        return;
    }

    // Create a new layer for pasting
    NewLayer();
    g_canvas[g_cidx].selLayerIndex = g_canvas[g_cidx].tiles.size() - (size_t)1; // Set to the newly created layer

    std::vector<int> newSelectedIndexes;
    for (const auto& tile : copiedTiles) {
        const int selectX = tile.first % g_canvas[g_cidx].width, selectY = tile.first / g_canvas[g_cidx].width;

        // Ensure the coordinates are within the canvas boundaries
        if (selectX >= 0 && selectX < g_canvas[g_cidx].width && selectY >= 0 && selectY < g_canvas[g_cidx].height) {
            const uint16_t newIndex = selectX + selectY * g_canvas[g_cidx].width;

            // Decompress the color data before pasting
            const ImU32 color = g_util.DecompressColorRLE(tile.second);
            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][newIndex] = color;
            newSelectedIndexes.push_back(newIndex);
        }
    }

    selectedIndexes = newSelectedIndexes;
    g_canvas[g_cidx].UpdateCanvasHistory();
}

void cCanvas::CopySelection() {
    copiedTiles.clear();
    if (selectedIndexes.empty()) return;

    for (const auto& index : selectedIndexes) {
        const ImU32 color = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
        copiedTiles[index] = g_util.CompressColorRLE(color);
    }
}

void cCanvas::DeleteSelection() {
    if (selectedIndexes.empty()) return;
    for (auto& index : selectedIndexes) g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32_BLACK_TRANS;

    selectedIndexes.clear();
    g_canvas[g_cidx].UpdateCanvasHistory();
}

void DrawLineOnCanvas(int x0, int y0, int x1, int y1, ImU32 color, bool preview = false) {
    const int dx = abs(x1 - x0), dy = abs(y1 - y0), sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    // Calculate the half-thickness for offset calculations
    const int halfThickness = static_cast<int>(line_size / 2.0f);

    while (true) {
        for (int offsetX = -halfThickness; offsetX <= halfThickness; ++offsetX) {
            for (int offsetY = -halfThickness; offsetY <= halfThickness; ++offsetY) {
                const int drawX = x0 + offsetX, drawY = y0 + offsetY;

                if (drawX >= 0 && drawX < g_canvas[g_cidx].width && drawY >= 0 && drawY < g_canvas[g_cidx].height) {
                    if (preview) {
                        // Draw the tile preview
                        const ImVec2 topLeft = { g_cam.x + drawX * TILE_SIZE, g_cam.y + drawY * TILE_SIZE };
                        const ImVec2 bottomRight = { topLeft.x + TILE_SIZE, topLeft.y + TILE_SIZE };
                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
                    }
                    else
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][drawY * g_canvas[g_cidx].width + drawX] = color;
                }
            }
        }

        if (x0 == x1 && y0 == y1) break;

        const int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void DrawCircleOnCanvas(int startX, int startY, int endX, int endY, ImU32 color, bool preview = false) {
    const int radiusX = (std::abs(endX - startX) + 1) / 2, radiusY = (std::abs(endY - startY) + 1) / 2;
    const int centerX = (startX + endX + 1) / 2, centerY = (startY + endY + 1) / 2;

    auto plotEllipsePoints = [&](int x, int y) {
        const int points[4][2] = {
            {centerX + x, centerY + y}, {centerX - x, centerY + y},
            {centerX + x, centerY - y}, {centerX - x, centerY - y}
        };

        for (auto& point : points) {
            const int px = point[0], py = point[1];

            if (px >= 0 && px < g_canvas[g_cidx].width && py >= 0 && py < g_canvas[g_cidx].height) {
                if (preview) {
                    const ImVec2 topLeft = { g_cam.x + px * TILE_SIZE, g_cam.y + py * TILE_SIZE };
                    const ImVec2 bottomRight = { topLeft.x + TILE_SIZE, topLeft.y + TILE_SIZE };
                    ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
                }
                else
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][py * g_canvas[g_cidx].width + px] = color;
            }
        }
    };

    int x = 0, y = radiusY;
    int p1 = static_cast<int>(radiusY * radiusY - radiusX * radiusX * radiusY + 0.25 * radiusX * radiusX);
    int dx = 2 * radiusY * radiusY * x, dy = 2 * radiusX * radiusX * y;

    // Region 1
    while (dx < dy) {
        plotEllipsePoints(x, y);
        x++;
        dx += 2 * radiusY * radiusY;

        if (p1 < 0)
            p1 += dx + radiusY * radiusY;
        else {
            y--;
            dy -= 2 * radiusX * radiusX;
            p1 += dx - dy + radiusY * radiusY;
        }
    }

    // Region 2
    int p2 = static_cast<int>(radiusY * radiusY * (x + 0.5) * (x + 0.5) + radiusX * radiusX * (y - 1) * (y - 1) - radiusX * radiusX * radiusY * radiusY);
    while (y >= 0) {
        plotEllipsePoints(x, y);
        y--;
        dy -= 2 * radiusX * radiusX;

        if (p2 > 0)
            p2 += radiusX * radiusX - dy;
        else {
            x++;
            dx += 2 * radiusY * radiusY;
            p2 += dx - dy + radiusX * radiusX;
        }
    }
}

void DrawRectangleOnCanvas(int x0, int y0, int x1, int y1, ImU32 color, bool preview = false) {
    const int startX = std::min(x0, x1), endX = std::max(x0, x1);
    const int startY = std::min(y0, y1), endY = std::max(y0, y1);

    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            // Check if the current pixel is on the boundary
            if (x == startX || x == endX || y == startY || y == endY) {
                if (x >= 0 && x < g_canvas[g_cidx].width && y >= 0 && y < g_canvas[g_cidx].height) {
                    if (preview) {
                        const ImVec2 topLeft = { g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE };
                        const ImVec2 bottomRight = { topLeft.x + TILE_SIZE, topLeft.y + TILE_SIZE };
                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
                    }
                    else
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][y * g_canvas[g_cidx].width + x] = color;
                }
            }
        }
    }
}

void DrawTextOnCanvasFreeType(FT_Face& face, const std::string& text, int mouseX, int mouseY, ImU32 color, int fontSize) {
    // Canvas dimensions
    int canvasWidth = g_canvas[g_cidx].width, canvasHeight = g_canvas[g_cidx].height;

    // Convert mouse position to canvas coordinates considering zoom (TILE_SIZE) and camera offset
    int startX = static_cast<int>((mouseX - g_cam.x) / TILE_SIZE), startY = static_cast<int>((mouseY - g_cam.y) / TILE_SIZE);

    // Ensure text starts within the canvas boundaries
    startX = std::max(startX, 0);
    startY = std::max(startY, 0);

    // Clamp to canvas bounds
    startX = glm::clamp(startX, 0, canvasWidth - 1);
    startY = glm::clamp(startY, 0, canvasHeight - 1);

    printf("Calculated text position: (%d, %d), Canvas size: (%d, %d), Camera position: (%.6f, %.6f), TILE_SIZE: (%.2f)\n",
        startX, startY, canvasWidth, canvasHeight, g_cam.x, g_cam.y, TILE_SIZE);

    // Set the desired font size using FreeType
    if (FT_Set_Pixel_Sizes(face, 0, fontSize)) {
        printf("Failed to set pixel sizes\n");
        return;
    }

    int x = startX, y = startY;  // Start drawing at the calculated canvas position
    int baselineY = y + (face->size->metrics.ascender >> 6); // Calculate the baseline

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        // Load the glyph for the current character
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            printf("Failed to load glyph for character: %c\n", c);
            continue;
        }

        FT_GlyphSlot g = face->glyph;

        // Calculate the glyph position
        int glyph_x = x + g->bitmap_left, glyph_y = baselineY - g->bitmap_top;

        // Ensure the glyph is within the canvas bounds
        if (glyph_x < 0 || glyph_y < 0 || glyph_x + g->bitmap.width > canvasWidth || glyph_y + g->bitmap.rows > canvasHeight) {
            printf("Glyph position (%d, %d) is out of canvas bounds for character '%c'\n", glyph_x, glyph_y, c);
            continue;
        }

        // Render glyph within canvas bounds
        for (int row = 0; row < g->bitmap.rows; ++row) {
            for (int col = 0; col < g->bitmap.width; ++col) {
                int posX = glyph_x + col, posY = glyph_y + row;

                if (posX >= 0 && posX < canvasWidth && posY >= 0 && posY < canvasHeight) {
                    const uint8_t alpha = g->bitmap.buffer[row * g->bitmap.width + col];
                    if (alpha > 0) {
                        const ImU32 blendedColor = g_util.BlendColor(color, alpha);
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][posY * canvasWidth + posX] = blendedColor;
                    }
                }
            }
        }

        // Move the cursor to the next character's position
        x += (g->advance.x >> 6);

        // If text exceeds canvas width, move to the next line
        if (x + g->bitmap.width >= canvasWidth) {
            x = startX; // Reset x to start
            baselineY += fontSize; // Move baseline down by the font size

            // Stop rendering if the text exceeds the canvas height
            if (baselineY + (face->size->metrics.ascender >> 6) >= canvasHeight) {
                printf("Text exceeds canvas height; truncating further rendering.\n");
                break;
            }
        }
    }
}

void applyBrushEffect(const ImVec2& lastMousePos, int x, int y, const ImU32& col) {
    const float brushRadius = brush_size / 2.0f + (pen_pressure * 4);
    const uint8_t brushRadiusInt = static_cast<int>(brushRadius);

    // Check if there is a valid previous position
    if (lastMousePos.x >= 0 && lastMousePos.y >= 0) {
        const int lastX = static_cast<int>((lastMousePos.x - g_cam.x) / TILE_SIZE), lastY = static_cast<int>((lastMousePos.y - g_cam.y) / TILE_SIZE);

        // Calculate the distance between the previous and current mouse positions
        const float distX = float(x - lastX), distY = float(y - lastY), distance = glm::sqrt(distX * distX + distY * distY);
        const int steps = static_cast<int>(distance) + 1; // Number of steps to interpolate

        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const int brushX = static_cast<int>(lastX + t * distX), brushY = static_cast<int>(lastY + t * distY);

            // Apply brush effect at the interpolated position
            if (selectedIndexes.empty() || std::find(selectedIndexes.begin(), selectedIndexes.end(), brushX + brushY * g_canvas[g_cidx].width) != selectedIndexes.end()) {
                for (int offsetY = -brushRadiusInt; offsetY <= brushRadiusInt; ++offsetY) {
                    for (int offsetX = -brushRadiusInt; offsetX <= brushRadiusInt; ++offsetX) {
                        const int finalX = brushX + offsetX, finalY = brushY + offsetY;
                        if (finalX >= 0 && finalX < g_canvas[g_cidx].width && finalY >= 0 && finalY < g_canvas[g_cidx].height) {
                            if (ImGui::GetIO().MouseDown[0])
                                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][finalX + finalY * g_canvas[g_cidx].width] = col;
                            else if (ImGui::GetIO().MouseDown[1])
                                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][finalX + finalY * g_canvas[g_cidx].width] = IM_COL32_BLACK_TRANS;
                        }
                    }
                }
            }
        }
    }

    // Collect border points for brush visualization
    std::unordered_set<uint64_t> brushIndexes;
    for (int offsetY = -brushRadiusInt; offsetY <= brushRadiusInt; ++offsetY) {
        for (int offsetX = -brushRadiusInt; offsetX <= brushRadiusInt; ++offsetX) {
            const int brushX = x + offsetX, brushY = y + offsetY;
            if (brushX >= 0 && brushX < g_canvas[g_cidx].width && brushY >= 0 && brushY < g_canvas[g_cidx].height) {
                ImGui::GetBackgroundDrawList()->AddRectFilled({ g_cam.x + brushX * TILE_SIZE, g_cam.y + brushY * TILE_SIZE },
                    { g_cam.x + brushX * TILE_SIZE + TILE_SIZE, g_cam.y + brushY * TILE_SIZE + TILE_SIZE },
                    col);

                brushIndexes.insert(brushX + brushY * g_canvas[g_cidx].width);
            }
        }
    }

    auto isBorderTile = [&](uint64_t index, int dx, int dy) {
        const uint64_t neighborIndex = index + dx + dy * g_canvas[g_cidx].width;
        return brushIndexes.find(neighborIndex) == brushIndexes.end();
    };

    std::vector<ImVec2> borderPoints;
    for (uint64_t index : brushIndexes) {
        const uint64_t tileX = index % g_canvas[g_cidx].width, tileY = index / g_canvas[g_cidx].width;
        const ImVec2 pos(g_cam.x + tileX * TILE_SIZE, g_cam.y + tileY * TILE_SIZE);

        if (isBorderTile(index, -1, 0)) {
            borderPoints.push_back(pos); // Left border
            borderPoints.push_back(ImVec2(pos.x, pos.y + TILE_SIZE));
        }
        if (isBorderTile(index, 1, 0)) {
            borderPoints.push_back(ImVec2(pos.x + TILE_SIZE, pos.y)); // Right border
            borderPoints.push_back(ImVec2(pos.x + TILE_SIZE, pos.y + TILE_SIZE));
        }
        if (isBorderTile(index, 0, -1)) {
            borderPoints.push_back(pos); // Top border
            borderPoints.push_back(ImVec2(pos.x + TILE_SIZE, pos.y));
        }
        if (isBorderTile(index, 0, 1)) {
            borderPoints.push_back(ImVec2(pos.x, pos.y + TILE_SIZE)); // Bottom border
            borderPoints.push_back(ImVec2(pos.x + TILE_SIZE, pos.y + TILE_SIZE));
        }
    }

    for (size_t i = 0; i < borderPoints.size(); i += 2)
        ImGui::GetBackgroundDrawList()->AddLine(borderPoints[i], borderPoints[i + 1], IM_COL32_BLACK, 2.0f);

    for (size_t i = 0; i < borderPoints.size(); i += 2)
        ImGui::GetBackgroundDrawList()->AddLine(borderPoints[i], borderPoints[i + 1], IM_COL32_WHITE, 1.0f);
}

void applyBandAidBrushEffect(const ImVec2& lastMousePos, int x, int y) {
    const float brushRadius = brush_size / 2.0f;
    const uint8_t brushRadiusInt = static_cast<uint8_t>(brushRadius);

    if (lastMousePos.x >= 0 && lastMousePos.y >= 0) {
        const int lastX = static_cast<int>((lastMousePos.x - g_cam.x) / TILE_SIZE), lastY = static_cast<int>((lastMousePos.y - g_cam.y) / TILE_SIZE);
        const float distX = float(x - lastX), distY = float(y - lastY), distance = glm::sqrt(distX * distX + distY * distY);
        const int steps = static_cast<int>(distance) + 1;

        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const int brushX = static_cast<int>(lastX + t * distX), brushY = static_cast<int>(lastY + t * distY);

            for (int offsetY = -brushRadiusInt; offsetY <= brushRadiusInt; ++offsetY) {
                for (int offsetX = -brushRadiusInt; offsetX <= brushRadiusInt; ++offsetX) {
                    const int finalX = brushX + offsetX, finalY = brushY + offsetY;
                    if (finalX >= 0 && finalX < g_canvas[g_cidx].width && finalY >= 0 && finalY < g_canvas[g_cidx].height) {
                        selectedIndexes.push_back(finalX + finalY * g_canvas[g_cidx].width);

                        // Draw the brush effect with the same style as applyBrushEffect
                        const ImVec2 topLeft = { g_cam.x + finalX * TILE_SIZE, g_cam.y + finalY * TILE_SIZE };
                        const ImVec2 bottomRight = { topLeft.x + TILE_SIZE, topLeft.y + TILE_SIZE };

                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, IM_COL32(255, 255, 255, 100)); // White overlay
                        ImGui::GetBackgroundDrawList()->AddRect(topLeft, bottomRight, IM_COL32(0, 0, 0, 100)); // Black border
                    }
                }
            }
        }
    }
}

void applyBandAidEffect() {
    if (selectedIndexes.empty()) return;

    // Step 1: Collect surrounding colors of all selected tiles
    std::vector<ImU32> surroundingColors;

    for (auto index : selectedIndexes) {
        const int x = index % g_canvas[g_cidx].width, y = index / g_canvas[g_cidx].width;

        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const int sampleX = x + offsetX, sampleY = y + offsetY;

                if (sampleX >= 0 && sampleX < g_canvas[g_cidx].width && sampleY >= 0 && sampleY < g_canvas[g_cidx].height) {
                    const ImU32 color = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][sampleY * g_canvas[g_cidx].width + sampleX];
                    if (color != IM_COL32_BLACK_TRANS && std::find(selectedIndexes.begin(), selectedIndexes.end(), sampleX + sampleY * g_canvas[g_cidx].width) == selectedIndexes.end())
                        surroundingColors.push_back(color);
                }
            }
        }
    }

    if (surroundingColors.empty()) return;

    int totalR = 0, totalG = 0, totalB = 0, totalA = 0;
    for (const auto& color : surroundingColors) {
        totalR += (color >> IM_COL32_R_SHIFT) & 0xFF;
        totalG += (color >> IM_COL32_G_SHIFT) & 0xFF;
        totalB += (color >> IM_COL32_B_SHIFT) & 0xFF;
        totalA += (color >> IM_COL32_A_SHIFT) & 0xFF;
    }

    const int count = (int)surroundingColors.size();
    const ImU32 blendedColor = IM_COL32(totalR / count, totalG / count, totalB / count, totalA / count);

    // Step 3: Apply the blended color to the selected tiles
    for (auto index : selectedIndexes)
        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = blendedColor;

    // Clear the selection after applying the effect
    selectedIndexes.clear();

    // Optionally, add the change to the history for undo functionality
    g_canvas[g_cidx].UpdateCanvasHistory();
}

void cCanvas::UpdateZoom(float value) {
    // Zooming
    if (value != 0.f) {
        const float minZoom = 1.0f, maxZoom = 50.0f;

        // Calculate new zoom level
        const float newZoom = glm::clamp(TILE_SIZE + value, minZoom, maxZoom);
        const ImVec2 mousePos = ImGui::GetMousePos();

        // Adjust camera position to keep the zoom centered around the mouse position
        if (newZoom != TILE_SIZE) {
            // Calculate the position of the mouse in the world coordinates before zooming
            const ImVec2 preZoomWorldPos = {
                (mousePos.x - g_cam.x) / TILE_SIZE,
                (mousePos.y - g_cam.y) / TILE_SIZE
            };

            // Calculate the new camera position to keep the zoom centered
            g_cam.x = mousePos.x - preZoomWorldPos.x * newZoom;
            g_cam.y = mousePos.y - preZoomWorldPos.y * newZoom;

            // Update TILE_SIZE to the new zoom level
            TILE_SIZE = (uint8_t)newZoom;
        }
    }
}

bool IsPointInPolygon(const ImVec2& point, const std::vector<ImVec2>& polygon) {
    bool inside = false;
    size_t j = polygon.size() - 1;

    #pragma omp simd
    for (size_t i = 0; i < polygon.size(); ++i) {
        if ((polygon[i].y > point.y) != (polygon[j].y > point.y) &&
            point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x) {
            inside = !inside;
        }
        j = i;
    }
    return inside;
}

bool DoLinesIntersect(ImVec2 a1, ImVec2 a2, ImVec2 b1, ImVec2 b2) {
    auto crossProduct = [](ImVec2 v1, ImVec2 v2) {
        return v1.x * v2.y - v1.y * v2.x;
    };

    const ImVec2 r = ImVec2(a2.x - a1.x, a2.y - a1.y), s = ImVec2(b2.x - b1.x, b2.y - b1.y);
    const float rxs = crossProduct(r, s);
    const float t = crossProduct(ImVec2(b1.x - a1.x, b1.y - a1.y), s) / rxs;
    const float u = crossProduct(ImVec2(b1.x - a1.x, b1.y - a1.y), r) / rxs;

    return rxs != 0 && t >= 0 && t <= 1 && u >= 0 && u <= 1;
}

bool IsLineIntersectingPolygon(const std::vector<ImVec2>& polygon, ImVec2 p1, ImVec2 p2) {
    for (size_t i = 0; i < polygon.size() - 1; ++i) {
        if (DoLinesIntersect(p1, p2, polygon[i], polygon[i + 1])) return true;
    }
    // Also check the last edge connecting the last and the first point
    return DoLinesIntersect(p1, p2, polygon.back(), polygon.front());
}

std::vector<int> GetTilesWithinPolygon(const std::vector<ImVec2>& polygon, int width, int height) {
    std::vector<int> selectedTiles;

    // Calculate the bounding box of the polygon to limit the tile checks
    float minX = polygon[0].x, maxX = polygon[0].x;
    float minY = polygon[0].y, maxY = polygon[0].y;

    for (const auto& point : polygon) {
        if (point.x < minX) minX = point.x;
        if (point.x > maxX) maxX = point.x;
        if (point.y < minY) minY = point.y;
        if (point.y > maxY) maxY = point.y;
    }

    // Convert bounding box coordinates to tile indices
    const int startX = std::max(0, int((minX - g_cam.x) / TILE_SIZE));
    const int startY = std::max(0, int((minY - g_cam.y) / TILE_SIZE));
    const int endX = std::min(width - 1, int((maxX - g_cam.x) / TILE_SIZE));
    const int endY = std::min(height - 1, int((maxY - g_cam.y) / TILE_SIZE));

    for (int y = startY; y <= endY; ++y) {
        #pragma omp simd
        for (int x = startX; x <= endX; ++x) {
            const ImVec2 topLeft(g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE);
            const ImVec2 center(topLeft.x + TILE_SIZE / 2, topLeft.y + TILE_SIZE / 2);
            const ImVec2 topRight(topLeft.x + TILE_SIZE, topLeft.y);
            const ImVec2 bottomLeft(topLeft.x, topLeft.y + TILE_SIZE);
            const ImVec2 bottomRight(topLeft.x + TILE_SIZE, topLeft.y + TILE_SIZE);

            // Check if the tile's center point is inside the polygon
            if (IsPointInPolygon(center, polygon)) {
                const int index = x + y * width;
                selectedTiles.push_back(index);
                continue; // No need to check edges if the center is inside
            }

            // Check if any of the tile's edges intersect with the polygon
            if (IsLineIntersectingPolygon(polygon, topLeft, topRight) ||  // Top edge
                IsLineIntersectingPolygon(polygon, topRight, bottomRight) ||  // Right edge
                IsLineIntersectingPolygon(polygon, bottomRight, bottomLeft) ||  // Bottom edge
                IsLineIntersectingPolygon(polygon, bottomLeft, topLeft)) {  // Left edge
                const int index = x + y * width;
                selectedTiles.push_back(index);
            }
        }
    }

    return selectedTiles;
}

void CompositeLayersToBuffer(std::vector<ImU32>& compositedBuffer, const std::vector<std::vector<ImU32>>& tiles, const std::vector<uint8_t>& layerVisibility, const std::vector<uint8_t>& layerOpacity, uint32_t width, uint32_t height) {
    if (compositedBuffer.size() != width * height)
        compositedBuffer.resize(width * height, IM_COL32_BLACK_TRANS);

    // Checkerboard colors
    constexpr ImU32 checker1 = IM_COL32(128, 128, 128, 255);
    constexpr ImU32 checker2 = IM_COL32(192, 192, 192, 255);

    // Cache visible layers with non-zero opacity
    std::vector<size_t> visibleLayers;
    visibleLayers.reserve(tiles.size());
    for (size_t layer = 0; layer < tiles.size(); ++layer) {
        if (layerVisibility[layer] && layerOpacity[layer] > 0)
            visibleLayers.push_back(layer);
    }

    // Iterate over all pixels
    #pragma omp parallel for
    for (int32_t y = 0; y < height; ++y) {
        #pragma omp simd // Apply SIMD to optimize within the loop
        for (int32_t x = 0; x < width; ++x) {
            uint8_t finalAlpha = 0, finalR = 0, finalG = 0, finalB = 0;
            const size_t pixelIndex = y * width + x;

            #pragma omp simd
            for (size_t i = 0; i < visibleLayers.size(); ++i) {
                const auto& layer = visibleLayers[i];
                const ImU32& color = tiles[layer][pixelIndex];
                const uint8_t pixelAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;

                if (pixelAlpha != 0) {
                    const uint8_t layerAlpha = layerOpacity[layer];
                    const uint8_t blendedAlpha = (pixelAlpha * layerAlpha) >> 8; // Approximate alpha blending using bit shift

                    if (blendedAlpha > 0) {
                        const uint8_t colorR = (color >> IM_COL32_R_SHIFT) & 0xFF;
                        const uint8_t colorG = (color >> IM_COL32_G_SHIFT) & 0xFF;
                        const uint8_t colorB = (color >> IM_COL32_B_SHIFT) & 0xFF;

                        // Pre-multiply the color with the blended alpha using bit shifting
                        const uint16_t blendR = (colorR * blendedAlpha) >> 8;
                        const uint16_t blendG = (colorG * blendedAlpha) >> 8;
                        const uint16_t blendB = (colorB * blendedAlpha) >> 8;

                        // Optimized alpha blending using bit shifting for division approximation
                        finalR = (blendR + ((finalR * (255 - blendedAlpha)) >> 8));
                        finalG = (blendG + ((finalG * (255 - blendedAlpha)) >> 8));
                        finalB = (blendB + ((finalB * (255 - blendedAlpha)) >> 8));
                        finalAlpha = blendedAlpha + ((finalAlpha * (255 - blendedAlpha)) >> 8);
                    }
                }
            }

            // Checkerboard pattern blend based on transparency
            const ImU32 checkerColor = ((x + y) % 2 == 0) ? checker1 : checker2;

            // Get checker color components
            const uint8_t checkerR = (checkerColor >> IM_COL32_R_SHIFT) & 0xFF;
            const uint8_t checkerG = (checkerColor >> IM_COL32_G_SHIFT) & 0xFF;
            const uint8_t checkerB = (checkerColor >> IM_COL32_B_SHIFT) & 0xFF;
            const uint8_t checkerA = 255;  // Checker is fully opaque

            // Blend checker color with final result based on transparency
            const uint8_t inverseAlpha = 255 - finalAlpha;
            finalR = (finalR * finalAlpha + checkerR * inverseAlpha) >> 8;
            finalG = (finalG * finalAlpha + checkerG * inverseAlpha) >> 8;
            finalB = (finalB * finalAlpha + checkerB * inverseAlpha) >> 8;
            finalAlpha = std::max(finalAlpha, inverseAlpha); // Preserve max alpha

            compositedBuffer[pixelIndex] = IM_COL32(finalR, finalG, finalB, finalAlpha);
        }
    }
}

void UpdateCanvasTexture(ID3D11DeviceContext* context, const std::vector<ImU32>& compositedBuffer, uint32_t width, uint32_t height) {
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = context->Map(canvasTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        printf("Failed to map texture for canvas update. HRESULT: 0x%08X\n", hr);
        return;
    }

    ImU32* dest = static_cast<ImU32*>(mappedResource.pData);
    const size_t rowPitch = mappedResource.RowPitch / sizeof(ImU32);

    if (rowPitch == width)
        std::memcpy(dest, compositedBuffer.data(), width * height * sizeof(ImU32));
    else {
        // For non-contiguous memory, use explicit SIMD instructions
        #pragma omp parallel for
        for (int y = 0; y < height; ++y) {
            const ImU32* src = compositedBuffer.data() + y * width;
            ImU32* dstRow = dest + y * rowPitch;

            #pragma omp simd
            for (uint32_t x = 0; x < width; ++x) {
                dstRow[x] = src[x];
            }
        }
    }

    context->Unmap(canvasTexture, 0);
}

void cCanvas::Editor() {
    key_state.update();

    if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('N') & 1)
        g_app.ui_state = std::make_unique<cUIStateNewProject>();
    else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('O') & 1)
        g_app.ui_state = std::make_unique<cUIStateOpenProject>();
    else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('S') & 1)
        g_app.ui_state = std::make_unique<cUIStateSaveProject>();

    if (g_canvas.empty() || g_canvas[g_cidx].tiles.empty() || g_canvas[g_cidx].layerVisibility.empty()) return;
    auto& d = *ImGui::GetBackgroundDrawList(); auto& io = ImGui::GetIO(); static bool isTypingText = false;

    if (!g_app.ui_state) {
        if (!isTypingText) {
            if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('Z') & 1) {
                // Undo operation
                if (g_canvas[g_cidx].canvas_idx > 0) {
                    g_canvas[g_cidx].canvas_idx--;
                    const auto& canvasState = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
                    const auto decompressedByteArray = g_util.DecompressCanvasDataZlib(canvasState, tiles[selLayerIndex].size() * sizeof(ImU32));
                    g_canvas[g_cidx].tiles[selLayerIndex] = g_util.ConvertByteArrayToLayer(decompressedByteArray);
                }
            }
            else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('Y') & 1) {
                // Redo operation
                if (g_canvas[g_cidx].canvas_idx < g_canvas[g_cidx].previousCanvases.size() - 1) {
                    g_canvas[g_cidx].canvas_idx++;
                    const auto& canvasState = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
                    const auto decompressedByteArray = g_util.DecompressCanvasDataZlib(canvasState, tiles[selLayerIndex].size() * sizeof(ImU32));
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_util.ConvertByteArrayToLayer(decompressedByteArray);
                }
            }
            else if (GetAsyncKeyState(VK_DELETE)) // Delete our selection area
                DeleteSelection();
            else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('X')) { // Cut our selection area
                g_canvas[g_cidx].CopySelection();
                g_canvas[g_cidx].DeleteSelection();
            }
            else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('C')) // Copy our selection area
                CopySelection();
            else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('V')) { // Paste our selection area
                if (IsImageInClipboard())
                    PasteImageFromClipboard();
                else if (!copiedTiles.empty())
                    PasteSelection(); // Default paste functionality for non-image data
            }
            else if (key_state.key_pressed('B')) paintToolSelected = TOOL_BRUSH;
            else if (key_state.key_pressed('G')) paintToolSelected = TOOL_BUCKET;
            else if (key_state.key_pressed('E')) paintToolSelected = TOOL_ERASER;
            else if (key_state.key_pressed('X')) paintToolSelected = TOOL_DROPPER;
            else if (key_state.key_pressed('M')) paintToolSelected = TOOL_MOVE;
            else if (key_state.key_pressed('W')) paintToolSelected = TOOL_WAND;
            else if (key_state.key_pressed('S')) paintToolSelected = TOOL_SELECT;
        }

        UpdateZoom(ImGui::GetIO().MouseWheel * glm::max(TILE_SIZE * 0.08f, 1.0f));
    }

    {
        // Set sampler state for canvas texture
        g_app.g_pd3dDeviceContext->PSSetShaderResources(0, 1, &canvasSRV);
        g_app.g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_pSamplerStatePoint);

        static std::vector<ImU32> compositedBuffer;
        CompositeLayersToBuffer(compositedBuffer, g_canvas[g_cidx].tiles, g_canvas[g_cidx].layerVisibility, g_canvas[g_cidx].layerOpacity, g_canvas[g_cidx].width, g_canvas[g_cidx].height);
        UpdateCanvasTexture(g_app.g_pd3dDeviceContext, compositedBuffer, width, height);
    }

    // Draw canvas
    d.AddImage((ImTextureID)canvasSRV, { glm::floor(g_cam.x), glm::floor(g_cam.y) }, ImVec2(glm::floor(g_canvas[g_cidx].width * TILE_SIZE) + glm::floor(g_cam.x), glm::floor(g_canvas[g_cidx].height * TILE_SIZE) + glm::floor(g_cam.y)), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);

    // Convert the screen coordinates to tile coordinates
    const uint16_t x = static_cast<int>((ImGui::GetMousePos().x - g_cam.x) / TILE_SIZE), y = static_cast<int>((ImGui::GetMousePos().y - g_cam.y) / TILE_SIZE);
    const bool clickingInsideCanvas = x >= 0 && x < width && y >= 0 && y < height;
    static ImVec2 mouseStart;
    if (g_util.MousePressed(0) && clickingInsideCanvas) mouseStart = ImGui::GetMousePos();
    const bool bCanDraw = !g_util.IsClickingOutsideCanvas(mouseStart.x > 0 && mouseStart.y > 0 ? mouseStart : io.MousePos);
    static ImVec2 lastMousePos = ImVec2(-1, -1);

    if (layerVisibility[selLayerIndex] && layerOpacity[selLayerIndex] && bCanDraw && clickingInsideCanvas && !ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemHovered()) {
        switch (paintToolSelected) {
        case TOOL_BRUSH:
            //brush
            applyBrushEffect(lastMousePos, x, y, myCols[selColIndex]);

            break;
        case TOOL_BUCKET:
            if (g_util.MousePressed(0))
                if (selectedIndexes.empty() || std::find(selectedIndexes.begin(), selectedIndexes.end(), (uint64_t)x + (uint64_t)y * width) != selectedIndexes.end())
                    g_util.FloodFill((int)x, (int)y, true);

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
            break;
        case TOOL_ERASER:
            applyBrushEffect(lastMousePos, x, y, IM_COL32_BLACK_TRANS);
            break;
        case TOOL_DROPPER:
            if (io.MouseDown[0])
                myCols[selColIndex] = tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width];

            d.AddRect({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 1, g_cam.y + y * TILE_SIZE + TILE_SIZE - 1 }, IM_COL32(0, 0, 0, 255));
            d.AddRectFilled({ g_cam.x + x * TILE_SIZE + 1, g_cam.y + y * TILE_SIZE + 1 }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 2, g_cam.y + y * TILE_SIZE + TILE_SIZE - 2 }, IM_COL32(255, 255, 255, 255));
            break;
        case TOOL_SELECT:
            if (g_util.MousePressed(0) || g_util.MousePressed(1))
                selectedIndexes.clear();

            if (ImGui::IsMouseDown(0)) {
                const ImVec2 end = ImGui::GetMousePos();

                // Calculate the mouse positions relative to the camera
                const float mouseStartX = mouseStart.x - g_cam.x, mouseStartY = mouseStart.y - g_cam.y;
                const float mouseEndX = end.x - g_cam.x, mouseEndY = end.y - g_cam.y;

                // Snap the coordinates to the tile grid
                const float startX = g_cam.x + std::floor(std::min(mouseStartX, mouseEndX) / TILE_SIZE) * TILE_SIZE;
                const float startY = g_cam.y + std::floor(std::min(mouseStartY, mouseEndY) / TILE_SIZE) * TILE_SIZE;
                const float endX = g_cam.x + std::ceil(std::max(mouseStartX, mouseEndX) / TILE_SIZE) * TILE_SIZE;
                const float endY = g_cam.y + std::ceil(std::max(mouseStartY, mouseEndY) / TILE_SIZE) * TILE_SIZE;

                // Draw the rectangle with snapping to the grid
                d.AddRect({ startX, startY }, { endX, endY }, IM_COL32_BLACK, 0, NULL, 4);
                d.AddRect({ startX, startY }, { endX, endY }, IM_COL32_WHITE, 0, NULL, 2);
            }

            if (g_util.MouseReleased(0)) {
                const ImVec2 end = ImGui::GetMousePos();

                // Calculate the mouse positions relative to the camera
                const float mouseStartX = mouseStart.x - g_cam.x, mouseStartY = mouseStart.y - g_cam.y;
                const float mouseEndX = end.x - g_cam.x, mouseEndY = end.y - g_cam.y;

                // Snap the coordinates to the tile grid
                const float startX = std::floor(std::min(mouseStartX, mouseEndX) / TILE_SIZE) * TILE_SIZE;
                const float startY = std::floor(std::min(mouseStartY, mouseEndY) / TILE_SIZE) * TILE_SIZE;
                const float endX = std::ceil(std::max(mouseStartX, mouseEndX) / TILE_SIZE) * TILE_SIZE;
                const float endY = std::ceil(std::max(mouseStartY, mouseEndY) / TILE_SIZE) * TILE_SIZE;

                // Calculate the selection dimensions
                const int startTileX = int(startX / TILE_SIZE), startTileY = int(startY / TILE_SIZE);
                const int endTileX = (int)endX / TILE_SIZE, endTileY = (int)endY / TILE_SIZE;

                for (int tileY = startTileY; tileY < endTileY; tileY++) {
                    for (int tileX = startTileX; tileX < endTileX; tileX++)
                        selectedIndexes.push_back((int64_t)(tileX + tileY * width));
                }

                if (selectedIndexes.size() <= 1) selectedIndexes.clear();
            }

            break;

        case TOOL_WAND:
            if (g_util.MousePressed(1)) selectedIndexes.clear();
            if (g_util.MousePressed(0)) g_util.FloodFill(x, y, false);

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
            break;

        case TOOL_MOVE:
            if (g_util.MousePressed(1)) selectedIndexes.clear();

            if (g_util.MousePressed(0)) {
                if (selectedIndexes.empty()) {
                    // Select all tiles if none are selected
                    for (int i = 0; i < width * height; ++i) {
                        if (tiles[g_canvas[g_cidx].selLayerIndex][i] != IM_COL32_BLACK_TRANS)
                            selectedIndexes.push_back(i);
                    }
                }

                // Calculate selection bounds
                float minX = FLT_MAX, minY = FLT_MAX;
                float maxX = -FLT_MAX, maxY = -FLT_MAX;

                for (const auto& index : selectedIndexes) {
                    const int selectX = index % width, selectY = index / width;
                    const float tilePosX = g_cam.x + selectX * TILE_SIZE, tilePosY = g_cam.y + selectY * TILE_SIZE;
                    minX = std::min(minX, tilePosX); minY = std::min(minY, tilePosY);
                    maxX = std::max(maxX, tilePosX + TILE_SIZE); maxY = std::max(maxY, tilePosY + TILE_SIZE);
                }

                // Check if the click is outside the bounds
                if (mouseStart.x < minX || mouseStart.x > maxX || mouseStart.y < minY || mouseStart.y > maxY) {
                    selectedIndexes.clear();
                }
            }

            if (ImGui::IsMouseDown(0)) {
                ImVec2 offset = ImGui::GetMousePos();
                offset.x -= mouseStart.x;
                offset.y -= mouseStart.y;

                // Snap offset to grid
                offset.x = static_cast<float>(static_cast<int>(offset.x / TILE_SIZE) * TILE_SIZE);
                offset.y = static_cast<float>(static_cast<int>(offset.y / TILE_SIZE) * TILE_SIZE);

                for (const auto& index : selectedIndexes) {
                    const int selectX = index % width, selectY = index / width;
                    const ImVec2 tilePos = { g_cam.x + selectX * TILE_SIZE + offset.x, g_cam.y + selectY * TILE_SIZE + offset.y };
                    d.AddRectFilled(tilePos, { tilePos.x + TILE_SIZE, tilePos.y + TILE_SIZE }, tiles[g_canvas[g_cidx].selLayerIndex][index]);
                }
            }

            if (g_util.MouseReleased(0)) {
                const int offsetX = static_cast<int>((ImGui::GetMousePos().x - mouseStart.x) / TILE_SIZE) * TILE_SIZE;
                const int offsetY = static_cast<int>((ImGui::GetMousePos().y - mouseStart.y) / TILE_SIZE) * TILE_SIZE;
                std::vector<int> newSelectedIndexes;
                std::unordered_map<int, ImU32> newTileColors;

                for (const auto& index : selectedIndexes) {
                    const int selectX = index % width, selectY = index / width;
                    const int newX = (selectX + offsetX / TILE_SIZE + width) % width;
                    const int newY = (selectY + offsetY / TILE_SIZE + height) % height; // Wrap positions within canvas boundaries

                    const uint32_t newIndex = newX + newY * width;
                    newSelectedIndexes.push_back(newIndex);
                    newTileColors[newIndex] = tiles[g_canvas[g_cidx].selLayerIndex][index];
                }

                // Clear the original positions
                for (const auto& index : selectedIndexes)
                    tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32_BLACK_TRANS;

                // Update new positions
                for (const auto& [newIndex, color] : newTileColors)
                    tiles[g_canvas[g_cidx].selLayerIndex][newIndex] = color;

                selectedIndexes = newSelectedIndexes;
            }

            break;
        case TOOL_LINE:
            if (ImGui::IsMouseDown(0))
                DrawLineOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), x, y, myCols[selColIndex], true);
            else
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);

            if (g_util.MouseReleased(0))
                DrawLineOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), x, y, myCols[selColIndex]); // Draw the line on the canvas

            break;
        case TOOL_SQUARE:
            if (ImGui::IsMouseDown(0))
                DrawRectangleOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), x, y, myCols[selColIndex], true); // Draw the preview rectangle
            else
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);

            if (g_util.MouseReleased(0))
                DrawRectangleOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), x, y, myCols[selColIndex]); // Draw the rectangle on the canvas

            break;
        case TOOL_ELIPSE:
            if (ImGui::IsMouseDown(0))
                DrawCircleOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), x, y, myCols[selColIndex], true); // Draw the preview ellipse
            else
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);

            if (g_util.MouseReleased(0))
                DrawCircleOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), x, y, myCols[selColIndex]); // Draw the ellipse on the canvas

            break;
        case TOOL_ZOOM:
            if (g_util.MousePressed(0)) UpdateZoom(1.0f);
            else if (g_util.MousePressed(1)) UpdateZoom(-1.0f);

            break;
        case TOOL_BANDAID:
            // Clear selected indexes
            if (g_util.MousePressed(0)) selectedIndexes.clear();

            if (ImGui::IsMouseDown(0)) applyBandAidBrushEffect(lastMousePos, x, y);

            if (g_util.MouseReleased(0)) {
                applyBandAidEffect(); selectedIndexes.clear();
            }

            break;
        case TOOL_FREEFORM_SELECT:
            if (g_util.MousePressed(1)) {
                freeformPath.clear(); selectedIndexes.clear();
            }

            if (g_util.MousePressed(0)) {
                selectedIndexes.clear();
                freeformPath.clear();
                freeformPath.push_back(ImGui::GetMousePos());
            }

            if (ImGui::IsMouseDown(0)) {
                if (freeformPath.empty() || freeformPath.back().x != ImGui::GetMousePos().x || freeformPath.back().y != ImGui::GetMousePos().y)
                    freeformPath.push_back(ImGui::GetMousePos());
            }

            if (g_util.MouseReleased(0)) {
                // Finalize the freeform selection
                if (!freeformPath.empty()) {
                    // Convert the path to a polygon and select tiles within it
                    std::vector<int> selectedTiles = GetTilesWithinPolygon(freeformPath, g_canvas[g_cidx].width, g_canvas[g_cidx].height);
                    selectedIndexes.insert(selectedIndexes.end(), selectedTiles.begin(), selectedTiles.end());
                }

                freeformPath.clear(); // Clear the path after selection
            }

            // Draw the freeform selection path
            if (!freeformPath.empty()) {
                ImGui::GetBackgroundDrawList()->AddPolyline(freeformPath.data(), freeformPath.size(), IM_COL32_BLACK, NULL, 4.0f);
                ImGui::GetBackgroundDrawList()->AddPolyline(freeformPath.data(), freeformPath.size(), IM_COL32_WHITE, NULL, 2.0f);
            }

            break;
        }
    }

    static ImVec2 textPosition, previousTextPosition;
    static std::string textInput, previousTextInput;
    static std::vector<ImVec2> linePositions;
    static std::vector<std::string> lines;

    // Handle text input
    if (paintToolSelected == TOOL_TEXT) {
        if (io.MouseClicked[0] && !isTypingText) {
            textPosition = ImGui::GetMousePos();
            isTypingText = true;
            textInput.clear();
            previousTextInput.clear();
            previousTextPosition = textPosition;
            lines.clear();
            linePositions.clear();
            lines.push_back("");
            linePositions.push_back(textPosition);
        }

        if (isTypingText) {
            // Hotkey to quit typing
            if (GetAsyncKeyState(VK_ESCAPE)) isTypingText = false;

            ImVec2 cursorPos;

            // Capture text input
            for (int c = 0; c < io.InputQueueCharacters.Size; c++) {
                const ImWchar ch = io.InputQueueCharacters[c];
                if (ch == '\n' || ch == '\r') {
                    // Handle Enter key: move to new line
                    textPosition.y += text_size * TILE_SIZE; // Adjust with font size and TILE_SIZE
                    textPosition.x = linePositions[0].x; // Reset x position to start of the line
                    lines.push_back(""); // Add a new line
                    linePositions.push_back(textPosition); // Save the position for the new line
                    textInput.clear(); // Clear input for the new line
                    previousTextInput.clear();
                    continue;
                }

                // Handle backspace
                if (ch == 127 || ch == '\b') {
                    if (!textInput.empty()) {
                        textInput.pop_back();  // Remove the last character from input

                        // Recalculate cursor position for last character
                        if (!textInput.empty()) {
                            if (FT_Load_Char(face, textInput.back(), FT_LOAD_RENDER) == 0) {
                                FT_GlyphSlot g = face->glyph;
                                int glyphWidth = g->advance.x >> 6;

                                // Move cursor back by the width of the last glyph
                                cursorPos.x -= glyphWidth + 1.5f * TILE_SIZE / text_size;  // Add extra spacing to align correctly

                                // Clear the last character's area on the canvas
                                int glyph_x = static_cast<int>(cursorPos.x), glyph_y = static_cast<int>(cursorPos.y - (face->size->metrics.ascender >> 6));
                                int glyph_height = (face->size->metrics.height >> 6);

                                // Erase the previous character from the canvas
                                for (int row = 0; row < glyph_height; ++row) {
                                    for (int col = 0; col < glyphWidth; ++col) {
                                        int posX = glyph_x + col, posY = glyph_y + row;

                                        if (posX >= 0 && posX < width && posY >= 0 && posY < height)
                                            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][posY * width + posX] = IM_COL32_BLACK_TRANS; // Background color or transparent
                                    }
                                }
                            }
                        }
                        else {
                            // If text input is empty, reset the position to start of line
                            cursorPos.x = linePositions.back().x;
                        }
                    }
                    else if (lines.size() > 1) {
                        // Handle backspace when current line is empty
                        lines.pop_back();
                        linePositions.pop_back();
                        textPosition = linePositions.back();
                        textInput = lines.back();
                        previousTextInput = textInput;
                        // Clear the entire line space on canvas
                    }
                }
                else {
                    textInput += static_cast<char>(ch);  // Ensure correct type cast
                    UpdateCanvasHistory();
                }
            }

            // Update the current line content
            if (!lines.empty()) lines.back() = textInput;

            // Draw the current text at the mouse position using FreeType
            for (size_t i = 0; i < lines.size(); ++i)
                DrawTextOnCanvasFreeType(face, lines[i], static_cast<int>(linePositions[i].x), static_cast<int>(linePositions[i].y), myCols[selColIndex], text_size);

            // Update the previous text input and position
            previousTextInput = textInput;
            previousTextPosition = textPosition;

            // Calculate the cursor position and height
            float cursorHeight = 0.0f;

            // Set font size in FreeType
            FT_Set_Pixel_Sizes(face, 0, static_cast<int>(text_size * TILE_SIZE));

            // Adjust cursor position based on text and glyph metrics
            if (!textInput.empty()) {
                // Load the last character to get its glyph metrics
                if (FT_Load_Char(face, textInput.back(), FT_LOAD_RENDER) == 0) {
                    FT_GlyphSlot g = face->glyph;

                    // Adjust cursor X-position by accounting for the full advance of all characters
                    cursorPos.x = linePositions.back().x + (textInput.size() * ((g->advance.x >> 6) + static_cast<int>(1.5f * TILE_SIZE / text_size)));

                    // Offset X-position slightly to the right to align perfectly after the last glyph
                    cursorPos.x += 1.0f * TILE_SIZE / text_size;  // Fine-tuning offset

                    // Adjust cursor Y-position to align with the baseline and account for glyph metrics
                    cursorPos.y = linePositions.back().y + (face->size->metrics.ascender >> 6) * TILE_SIZE / text_size - (g->bitmap_top * TILE_SIZE / text_size);

                    // Set cursor height based on the tallest glyph in the font size
                    cursorHeight = (face->size->metrics.ascender >> 6) * TILE_SIZE / text_size;
                    cursorHeight *= 0.25f;
                }
            }
            else {
                // Default cursor position and height if no text
                cursorPos.x = linePositions.back().x - text_size;
                cursorPos.y = linePositions.back().y + (face->size->metrics.ascender >> 6) * TILE_SIZE / text_size;
                cursorHeight = (face->size->metrics.ascender >> 6) * TILE_SIZE;
            }

            // Draw blinking cursor with corrected position and height
            if (fmod(ImGui::GetTime(), 1.0f) > 0.5f) {
                ImVec2 cursorPos1 = ImVec2(cursorPos.x, cursorPos.y - cursorHeight), cursorPos2 = ImVec2(cursorPos.x, cursorPos.y + cursorHeight);
                d.AddLine(cursorPos1, cursorPos2, IM_COL32_BLACK, 2.0f);
                d.AddLine(cursorPos1, cursorPos2, IM_COL32_WHITE, 1.0f);
            }
        }
        else
            isTypingText = false;
    }

    // Draw a rectangle around the selected indexes
    DrawSelectionRectangle(selectedIndexes, TILE_SIZE, g_cam.x, g_cam.y, width, height, IM_COL32_WHITE, 2);

    // Add canvas to history for undo-redo feature
    if (!g_canvas.empty())
        if (paintToolSelected < 3 || paintToolSelected >= 6) // fix later
            if (bCanDraw && !g_app.ui_state)
                if (g_util.MouseReleased(0) || g_util.MouseReleased(1))
                    UpdateCanvasHistory();

    // Middle mouse navigate
    if (io.MouseDown[2] || paintToolSelected == TOOL_PAN && io.MouseDown[0]) {
        if (io.MouseDelta.x || io.MouseDelta.y) {
            g_cam.x += io.MouseDelta.x; g_cam.y += io.MouseDelta.y;
        }

        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    else
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

    // Update the previous mouse position for the next frame
    lastMousePos = ImGui::GetMousePos();
}