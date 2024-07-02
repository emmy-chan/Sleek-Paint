#include "canvas.h"
#include "camera.h"
#include "utils.h"

//Get Key State -_-
#include <WinUser.h>

#include <fstream>
#include <string>
#include "data_manager.h"

std::vector<cCanvas> g_canvas = std::vector<cCanvas>();
uint16_t g_cidx = uint16_t();

#include <stack>
#include <utility> // for std::pair
#include <unordered_set>
#include <set>

std::unordered_set<uint16_t> selectedIndexes;
std::unordered_map<uint16_t, ImU32> copiedTiles; // Store copied tiles and their colors

// Flood fill function
void floodFill(int x, int y, bool paint) {
    if (x < 0 || x >= g_canvas[g_cidx].width || y < 0 || y >= g_canvas[g_cidx].height)
        return;

    const ImU32 initialCol = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][x + y * g_canvas[g_cidx].width];
    const ImU32 fillCol = paint ? g_canvas[g_cidx].myCols[g_canvas[g_cidx].selColIndex] : initialCol;

    // Scale the threshold from 0-100 to 0-765
    const int threshold = (paint ? g_canvas[g_cidx].bucket_fill_threshold : g_canvas[g_cidx].magic_wand_threshold) * 765 / 100;

    std::stack<std::pair<int, int>> stack;
    stack.push({ x, y });

    if (!paint) selectedIndexes.clear();

    std::set<uint16_t> visited; // To avoid processing the same pixel multiple times

    while (!stack.empty()) {
        std::pair<int, int> p = stack.top();
        stack.pop();
        const int curX = p.first;
        const int curY = p.second;

        if (curX < 0 || curX >= g_canvas[g_cidx].width || curY < 0 || curY >= g_canvas[g_cidx].height)
            continue;

        const uint16_t currentIndex = curX + curY * g_canvas[g_cidx].width;

        if (visited.find(currentIndex) != visited.end())
            continue; // Skip already processed pixels

        visited.insert(currentIndex);
        const ImU32 currentCol = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex];

        if (paint) {
            // Check if the tile is in the selected tiles set (if there are any selected tiles)
            if (!selectedIndexes.empty() && selectedIndexes.find(currentIndex) == selectedIndexes.end())
                continue;

            if (g_util.ColorDifference(currentCol, initialCol) > threshold)
                continue;

            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex] = fillCol;
        }
        else {
            if (g_util.ColorDifference(currentCol, initialCol) > threshold || selectedIndexes.find(currentIndex) != selectedIndexes.end())
                continue;

            selectedIndexes.insert(currentIndex);
        }

        stack.push({ curX + 1, curY });
        stack.push({ curX - 1, curY });
        stack.push({ curX, curY + 1 });
        stack.push({ curX, curY - 1 });
    }

    printf("FloodFill: Completed successfully!\n");
}

//Todo: split this up for palette and initializing our canvas.
//Also todo in future: maybe make this func take a new size argument ?
void cCanvas::Initialize() {
    tiles.clear();
    previousCanvases.clear();
    NewLayer();
}

void cCanvas::NewLayer() {
    std::vector<ImU32> layer0;

    //Create our blank canvas
    for (int i = 0; i < width * height; i++)
        layer0.push_back(IM_COL32(0, 0, 0, 0));

    tiles.push_back(layer0);
    layerVisibility.push_back(true);
}

void cCanvas::Clear() {
    for (auto& tile : g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex]) //int i = 0; i < g_canvas[g_cidx].width * g_canvas[g_cidx].height; i++
        tile = IM_COL32(255, 255, 255, 0);
}

void cCanvas::AdaptNewSize(int width, int height) {
    // Create a temporary container for the resized image
    std::vector<std::vector<ImU32>> newLayers(g_canvas[g_cidx].tiles.size(), std::vector<ImU32>(width * height, IM_COL32(0, 0, 0, 0)));

    // Calculate scaling factors
    const float scaleX = static_cast<float>(width) / g_canvas[g_cidx].width;
    const float scaleY = static_cast<float>(height) / g_canvas[g_cidx].height;

    // Iterate over all layers and adapt the content
    for (size_t layerIndex = 0; layerIndex < g_canvas[g_cidx].tiles.size(); ++layerIndex) {
        const auto& oldLayer = g_canvas[g_cidx].tiles[layerIndex];
        auto& newLayer = newLayers[layerIndex];

        // Map old pixels to new positions with scaling
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int oldX = static_cast<int>(x / scaleX);
                const int oldY = static_cast<int>(y / scaleY);
                if (oldX < g_canvas[g_cidx].width && oldY < g_canvas[g_cidx].height) {
                    const int oldIndex = oldX + oldY * g_canvas[g_cidx].width;
                    const int newIndex = x + y * width;
                    if (oldIndex < oldLayer.size() && newIndex < newLayer.size()) {
                        newLayer[newIndex] = oldLayer[oldIndex];
                    }
                }
            }
        }
    }

    // Replace old layers with new layers
    g_canvas[g_cidx].tiles = std::move(newLayers);
    g_canvas[g_cidx].width = width;
    g_canvas[g_cidx].height = height;
}

// Helper function to compare two 1D vectors
bool isTilesEqual(const std::vector<ImU32>& a, const std::vector<ImU32>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
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
    if (previousCanvases.size() > 1 && previousCanvases.size() > canvas_idx) {
        previousCanvases.resize(canvas_idx + 1);
    }

    // Only add the current state to history if it's different from the last saved state
    if (previousCanvases.empty() || (!tiles[g_canvas[g_cidx].selLayerIndex].empty() && !previousCanvases.empty() && !isTilesEqual(tiles[g_canvas[g_cidx].selLayerIndex], previousCanvases.back()))) {
        previousCanvases.push_back(tiles[g_canvas[g_cidx].selLayerIndex]);
        canvas_idx = previousCanvases.size() - 1;
        printf("Canvas state created.\n");
    }
    else {
        printf("No changes detected; canvas state not updated.\n");
    }
}

//This is used for fixing accidental brushing when using scroll bars.
bool IsClickingOutsideCanvas() {
    auto& io = ImGui::GetIO();
    static bool bToolFlip = true;
    const bool bMouseOutsideCanvas = io.MousePos.x < 200 || io.MousePos.x > io.DisplaySize.x - 61 || io.MousePos.y > io.DisplaySize.y - 20 || io.MousePos.y < 24;
    static bool bCanDraw = !bMouseOutsideCanvas && !g_app.ui_state;

    if (bMouseOutsideCanvas && g_util.MousePressed(0)) {
        bToolFlip = false;
    }

    if (!bToolFlip) {
        if (g_util.MouseReleased(0))
            bToolFlip = !g_app.ui_state;
    }
    else
        bCanDraw = !bMouseOutsideCanvas && !g_app.ui_state;

    return bCanDraw;
}

void cCanvas::LoadColorPalette(std::string input) {
    myCols.clear();

    //Load color palette!
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

    //Reset memory dedicated to the canvas we are deleting?
    //Todo: we need to do more than just 'erase'
    //The memory will NOT be de-allocated
    //std::vector<cCanvas> tempVector;
    //g_canvas.swap(tempVector);

    //Prevent our canvas index from going out of bounds x3
    if (g_cidx == g_canvas.size())
        g_cidx--;
}

// Function to calculate the bounding box of the selected indexes
ImVec2 GetTilePos(uint16_t index, float tileSize, float camX, float camY, int width) {
    const float x = index % width;
    const float y = index / width;
    return ImVec2(camX + x * tileSize, camY + y * tileSize);
}

void DrawSelectionRectangle(ImDrawList* drawList, const std::unordered_set<uint16_t>& indexes, float tileSize, float camX, float camY, int width, ImU32 col) {
    if (indexes.empty()) return;

    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;

    for (uint16_t index : indexes) {
        ImVec2 pos = GetTilePos(index, tileSize, camX, camY, width);
        minX = std::min(minX, pos.x);
        minY = std::min(minY, pos.y);
        maxX = std::max(maxX, pos.x + tileSize);
        maxY = std::max(maxY, pos.y + tileSize);
    }

    drawList->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.0f, 0, 1.0f); // Yellow rectangle
}

std::unordered_set<uint16_t> initialSelectedIndexes;

void cCanvas::PasteSelection() {
    // Ensure there is something to paste
    if (copiedTiles.empty()) {
        printf("PasteSelection: No copied tiles to paste.\n");
        return;
    }

    // Create a new layer for pasting
    NewLayer();
    g_canvas[g_cidx].selLayerIndex = g_canvas[g_cidx].tiles.size() - 1; // Set to the newly created layer

    std::unordered_set<uint16_t> newSelectedIndexes;
    for (auto& tile : copiedTiles) {
        const int selectX = tile.first % g_canvas[g_cidx].width;
        const int selectY = tile.first / g_canvas[g_cidx].width;

        // Ensure the coordinates are within the canvas boundaries
        if (selectX >= 0 && selectX < g_canvas[g_cidx].width && selectY >= 0 && selectY < g_canvas[g_cidx].height) {
            uint16_t newIndex = selectX + selectY * g_canvas[g_cidx].width;
            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][newIndex] = tile.second;
            newSelectedIndexes.insert(newIndex);
        }
    }
    selectedIndexes = newSelectedIndexes;

    g_canvas[g_cidx].UpdateCanvasHistory();
}

void cCanvas::CopySelection() {
    copiedTiles.clear();
    if (selectedIndexes.empty()) {
        printf("CopySelection: No selected tiles to copy.\n");
        return;
    }
    for (auto& index : selectedIndexes) {
        copiedTiles[index] = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
    }
}

void cCanvas::DeleteSelection() {
    if (selectedIndexes.empty()) {
        printf("DeleteSelection: No selected tiles to delete.\n");
        return;
    }
    for (auto& index : selectedIndexes) {
        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32(0, 0, 0, 0);
    }

    g_canvas[g_cidx].UpdateCanvasHistory();
}

void DrawLineOnCanvas(int x0, int y0, int x1, int y1, ImU32 color, bool preview = false) {
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < g_canvas[g_cidx].width && y0 >= 0 && y0 < g_canvas[g_cidx].height) {
            if (preview) {
                // Draw the tile preview
                ImVec2 topLeft = { g_cam.x + x0 * g_canvas[g_cidx].TILE_SIZE, g_cam.y + y0 * g_canvas[g_cidx].TILE_SIZE };
                ImVec2 bottomRight = { topLeft.x + g_canvas[g_cidx].TILE_SIZE, topLeft.y + g_canvas[g_cidx].TILE_SIZE };
                ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
            }
            else
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][y0 * g_canvas[g_cidx].width + x0] = color;
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

void cCanvas::Editor() {
    auto& d = *ImGui::GetBackgroundDrawList();
    auto& io = ImGui::GetIO();

    std::unordered_set<uint16_t> nonSelectedIndexes;

    if (!g_app.ui_state) {
        if (GetAsyncKeyState('B'))
            paintToolSelected = 0;
        else if (GetAsyncKeyState('G'))
            paintToolSelected = 1;
        else if (GetAsyncKeyState('E'))
            paintToolSelected = 2;
        else if (GetAsyncKeyState('X'))
            paintToolSelected = 3;
        else if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('Z') & 1) {
            if (g_canvas[g_cidx].canvas_idx > 0) {
                g_canvas[g_cidx].canvas_idx--;
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
            }
        }
        else if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('Y') & 1) {
            if (g_canvas[g_cidx].canvas_idx < g_canvas[g_cidx].previousCanvases.size() - 1) {
                g_canvas[g_cidx].canvas_idx++;
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
            }
        }

        // Zooming
        if (io.MouseWheel != 0.f) {
            const float minZoom = 4.0f;
            const float maxZoom = 50.0f;

            // Calculate new zoom level
            float newZoom = TILE_SIZE + io.MouseWheel * 4;
            newZoom = glm::clamp(newZoom, minZoom, maxZoom);
            const auto mousePos = ImGui::GetMousePos();

            // Adjust camera position to keep the zoom centered around the mouse position
            ImVec2 offset = { mousePos.x - g_cam.x, mousePos.y - g_cam.y };
            offset.x /= TILE_SIZE;
            offset.y /= TILE_SIZE;
            offset.x *= newZoom;
            offset.y *= newZoom;
            g_cam.x = mousePos.x - offset.x;
            g_cam.y = mousePos.y - offset.y;

            TILE_SIZE = newZoom;
        }

        // Delete our selection area
        if (GetAsyncKeyState(VK_DELETE))
            DeleteSelection();

        // Cut our selection area
        if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState('X')) {
            g_canvas[g_cidx].CopySelection();
            g_canvas[g_cidx].DeleteSelection();
        }

        // Copy our selection area
        if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState('C'))
            CopySelection();

        // Paste our selection area
        if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState('V') && !copiedTiles.empty())
            PasteSelection();
    }

    for (float y = 0; y < height; y++) {
        for (float x = 0; x < width; x++) {
            uint64_t index = static_cast<uint64_t>(x) + static_cast<uint64_t>(y) * width;

            // Add non-selected tiles to the list
            if (selectedIndexes.find(index) == selectedIndexes.end()) {
                nonSelectedIndexes.insert(index);
            }

            // Draw grid background for the base layer
            const ImU32 col = (static_cast<int>(x) + static_cast<int>(y)) % 2 == 0 ? IM_COL32(110, 110, 110, 255) : IM_COL32(175, 175, 175, 255);
            d.AddRectFilled(
                ImVec2(g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE),
                ImVec2(g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE),
                col
            );

            // Draw each layer from bottom to top, including layers after the selected layer index
            for (size_t layer = 0; layer < g_canvas[g_cidx].tiles.size(); layer++) {
                // Check if the layer is visible
                if (!g_canvas[g_cidx].layerVisibility[layer]) {
                    continue; // Skip drawing this layer if it is not visible
                }

                ImU32 tileColor = g_canvas[g_cidx].tiles[layer][index];

                // Check for transparency in the tile color
                if (((tileColor >> 24) & 0xFF) > 0) {
                    d.AddRectFilled(
                        ImVec2(g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE),
                        ImVec2(g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE),
                        tileColor
                    );
                }
            }
        }
    }

    // Convert the screen coordinates to tile coordinates
    uint16_t x = static_cast<int>((ImGui::GetMousePos().x - g_cam.x) / TILE_SIZE);
    uint16_t y = static_cast<int>((ImGui::GetMousePos().y - g_cam.y) / TILE_SIZE);

    // Clamp the coordinates to ensure they are within the canvas dimensions
    //x = glm::clamp<uint16_t>(x, (uint16_t)0, g_canvas[g_cidx].width - (uint16_t)1);
    //y = glm::clamp<uint16_t>(y, (uint16_t)0, g_canvas[g_cidx].height - (uint16_t)1);

    const bool bCanDraw = IsClickingOutsideCanvas() && x >= 0 && x < g_canvas[g_cidx].width && y >= 0 && y < g_canvas[g_cidx].height;
    static ImVec2 mouseStart;
    if (g_util.MousePressed(0)) mouseStart = ImGui::GetMousePos();

    const float brushRadius = brush_size / 2.0f;

    if (bCanDraw && g_util.Hovering(g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE, g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE)) {
        switch (paintToolSelected) {
        case 0:
            //brush
            if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)x + (uint64_t)y * width) != selectedIndexes.end()) {
                if (io.MouseDown[0] || io.MouseDown[1]) {
                    for (int offsetY = -brushRadius; offsetY <= brushRadius; ++offsetY) {
                        for (int offsetX = -brushRadius; offsetX <= brushRadius; ++offsetX) {
                            float distance = std::sqrt(offsetX * offsetX + offsetY * offsetY);
                            if (distance <= brushRadius) {
                                const int brushX = x + offsetX;
                                const int brushY = y + offsetY;
                                if (brushX >= 0 && brushX < width && brushY >= 0 && brushY < height) {
                                    if (ImGui::GetMousePos().x > 0 && ImGui::GetMousePos().x < io.DisplaySize.x - 1 &&
                                        ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().y < io.DisplaySize.y - 1) {
                                        if (io.MouseDown[0]) {
                                            tiles[g_canvas[g_cidx].selLayerIndex][brushX + brushY * width] = myCols[selColIndex];
                                        }
                                        else if (io.MouseDown[1]) {
                                            tiles[g_canvas[g_cidx].selLayerIndex][brushX + brushY * width] = IM_COL32(0, 0, 0, 0);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Adjust the visualization of the brush size
            for (int offsetY = -brushRadius; offsetY <= brushRadius; ++offsetY) {
                for (int offsetX = -brushRadius; offsetX <= brushRadius; ++offsetX) {
                    float distance = std::sqrt(offsetX * offsetX + offsetY * offsetY);
                    if (distance <= brushRadius) {
                        const int brushX = x + offsetX;
                        const int brushY = y + offsetY;
                        if (brushX >= 0 && brushX < width && brushY >= 0 && brushY < height) {
                            d.AddRectFilled({ g_cam.x + brushX * TILE_SIZE, g_cam.y + brushY * TILE_SIZE },
                                { g_cam.x + brushX * TILE_SIZE + TILE_SIZE, g_cam.y + brushY * TILE_SIZE + TILE_SIZE },
                                myCols[selColIndex]);
                        }
                    }
                }
            }

            break;
        case 1:
            //paint bucket
            if (g_util.MousePressed(0))
                if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)x + (uint64_t)y * width) != selectedIndexes.end())
                    floodFill((int)x, (int)y, true);

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
            break;
        case 2:
            //eraser
            if (io.MouseDown[0])
                if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)x + (uint64_t)y * width) != selectedIndexes.end())
                    if (ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().x < io.DisplaySize.x - 1 && ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().y < io.DisplaySize.y - 1)
                        tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width] = IM_COL32(0, 0, 0, 0);

            d.AddRect({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 1, g_cam.y + y * TILE_SIZE + TILE_SIZE - 1 }, IM_COL32(0, 0, 0, 255));
            d.AddRectFilled({ g_cam.x + x * TILE_SIZE + 1, g_cam.y + y * TILE_SIZE + 1 }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 2, g_cam.y + y * TILE_SIZE + TILE_SIZE - 2 }, IM_COL32(255, 255, 255, 255));
            break;
        case 3:
            //dropper
            if (io.MouseDown[0])
                myCols[selColIndex] = tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width];

            d.AddRect({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 1, g_cam.y + y * TILE_SIZE + TILE_SIZE - 1 }, IM_COL32(0, 0, 0, 255));
            d.AddRectFilled({ g_cam.x + x * TILE_SIZE + 1, g_cam.y + y * TILE_SIZE + 1 }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 2, g_cam.y + y * TILE_SIZE + TILE_SIZE - 2 }, IM_COL32(255, 255, 255, 255));
            break;
        case 4:
            if (g_util.MousePressed(0))
                selectedIndexes.clear();

            if (ImGui::IsMouseDown(0))
                d.AddRect(mouseStart, ImGui::GetMousePos(), IM_COL32_WHITE, 0, NULL, 4);

            if (g_util.MouseReleased(0)) {
                const ImVec2 end = ImGui::GetMousePos();
                const float startX = std::floor(std::min(mouseStart.x, end.x) / TILE_SIZE) * TILE_SIZE;
                const float startY = std::floor(std::min(mouseStart.y, end.y) / TILE_SIZE) * TILE_SIZE;
                const float endX = std::ceil(std::max(mouseStart.x, end.x) / TILE_SIZE) * TILE_SIZE;
                const float endY = std::ceil(std::max(mouseStart.y, end.y) / TILE_SIZE) * TILE_SIZE;

                for (float selectY = 0; selectY < height; selectY++) {
                    for (float selectX = 0; selectX < width; selectX++) {
                        const float tilePosX = g_cam.x + selectX * TILE_SIZE;
                        const float tilePosY = g_cam.y + selectY * TILE_SIZE;

                        if (tilePosX >= startX && tilePosX < endX && tilePosY >= startY && tilePosY < endY) {
                            selectedIndexes.insert((uint16_t)(selectX + selectY * width));
                        }
                    }
                }
            }

            break;

        case 5:
            //magic wand
            if (g_util.MousePressed(0)) {
                const ImVec2 mousePos = ImGui::GetMousePos();
                const int tileX = static_cast<int>((mousePos.x - g_cam.x) / TILE_SIZE);
                const int tileY = static_cast<int>((mousePos.y - g_cam.y) / TILE_SIZE);

                if (tileX >= 0 && tileX < width && tileY >= 0 && tileY < height) {
                    floodFill(tileX, tileY, false);
                }
            }

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
            break;

        case 6:
            // move tool
            if (g_util.MousePressed(0)) {
                initialSelectedIndexes = selectedIndexes;

                // Calculate selection bounds
                float minX = FLT_MAX, minY = FLT_MAX;
                float maxX = -FLT_MAX, maxY = -FLT_MAX;

                for (const auto& index : initialSelectedIndexes) {
                    const int selectX = index % width;
                    const int selectY = index / width;
                    const float tilePosX = g_cam.x + selectX * TILE_SIZE;
                    const float tilePosY = g_cam.y + selectY * TILE_SIZE;
                    minX = std::min(minX, tilePosX);
                    minY = std::min(minY, tilePosY);
                    maxX = std::max(maxX, tilePosX + TILE_SIZE);
                    maxY = std::max(maxY, tilePosY + TILE_SIZE);
                }

                // Check if the click is outside the bounds
                if (mouseStart.x < minX || mouseStart.x > maxX || mouseStart.y < minY || mouseStart.y > maxY) {
                    selectedIndexes.clear();
                    initialSelectedIndexes.clear();
                }
            }

            if (ImGui::IsMouseDown(0)) {
                ImVec2 offset = ImGui::GetMousePos();
                offset.x -= mouseStart.x; offset.y -= mouseStart.y;

                for (const auto& index : initialSelectedIndexes) {
                    const int selectX = index % width;
                    const int selectY = index / width;
                    const ImVec2 tilePos = { g_cam.x + selectX * TILE_SIZE + offset.x, g_cam.y + selectY * TILE_SIZE + offset.y };
                    d.AddRectFilled(tilePos, { tilePos.x + TILE_SIZE, tilePos.y + TILE_SIZE }, tiles[g_canvas[g_cidx].selLayerIndex][index]);
                }
            }

            if (g_util.MouseReleased(0)) {
                ImVec2 offset = ImGui::GetMousePos();
                offset.x -= mouseStart.x; offset.y -= mouseStart.y;

                std::unordered_set<uint16_t> newSelectedIndexes;
                std::unordered_map<uint16_t, ImU32> newTileColors;

                for (const auto& index : initialSelectedIndexes) {
                    const int selectX = index % width;
                    const int selectY = index / width;
                    const int newX = static_cast<int>(selectX + offset.x / TILE_SIZE);
                    const int newY = static_cast<int>(selectY + offset.y / TILE_SIZE);

                    if (newX >= 0 && newX < width && newY >= 0 && newY < height) {
                        uint16_t newIndex = newX + newY * width;
                        newSelectedIndexes.insert(newIndex);
                        newTileColors[newIndex] = tiles[g_canvas[g_cidx].selLayerIndex][index];
                    }
                }

                // Clear the original positions
                for (const auto& index : initialSelectedIndexes) {
                    tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32(0, 0, 0, 0);
                }

                // Update new positions
                for (const auto& [newIndex, color] : newTileColors) {
                    tiles[g_canvas[g_cidx].selLayerIndex][newIndex] = color;
                }

                selectedIndexes = newSelectedIndexes;
            }

            break;
        }
    }

    // Draw a rectangle around the selected indexes
    if (!selectedIndexes.empty()) {
        DrawSelectionRectangle(&d, selectedIndexes, TILE_SIZE, g_cam.x, g_cam.y, width, IM_COL32_WHITE);
        DrawSelectionRectangle(&d, nonSelectedIndexes, TILE_SIZE, g_cam.x, g_cam.y, width, IM_COL32(175, 175, 175, 255));
    }

    // Line tool
    if (paintToolSelected == 7 && bCanDraw) {
        if (ImGui::IsMouseDown(0)) {
            // Draw the preview line
            const ImVec2 mousePos = ImGui::GetMousePos();
            const uint16_t endX = static_cast<int>((mousePos.x - g_cam.x) / TILE_SIZE);
            const uint16_t endY = static_cast<int>((mousePos.y - g_cam.y) / TILE_SIZE);

            DrawLineOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), endX, endY, myCols[selColIndex], true);
        }
        else
            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);

        if (g_util.MouseReleased(0)) {
            // Convert the screen coordinates to tile coordinates
            const uint16_t startX = static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE);
            const uint16_t startY = static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE);

            // Draw the line on the canvas
            DrawLineOnCanvas(startX, startY, x, y, myCols[selColIndex]);
        }
    }

    //TODO: when we click new project it makes one state for us right now... But we should make it create that state upon new creation only. Fix states being added during dialog.
    //Add canvas to history for undo-redo feature
    if (!g_canvas.empty())
        if (paintToolSelected < 3 || paintToolSelected >= 6) // fix later
            if (bCanDraw && !g_app.ui_state)
                if (g_util.MouseReleased(0) || g_util.MouseReleased(1))
                    UpdateCanvasHistory();

    // Middle mouse navigate
    if (io.MouseDelta.x || io.MouseDelta.y) {
        if (io.MouseDown[2]) {
            g_cam.x += io.MouseDelta.x;
            g_cam.y += io.MouseDelta.y;
        }
    }
}