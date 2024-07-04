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
#include "keystate.h"

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

void cCanvas::Initialize(const std::vector<ImU32>& initial_data, const uint16_t& new_width, const uint16_t& new_height) {
    tiles.clear();
    previousCanvases.clear();

    // Initialize canvas dimensions
    width = new_width;
    height = new_height;

    // Create new layer
    NewLayer(initial_data);
}

void cCanvas::NewLayer(const std::vector<ImU32>& initial_data) {
    std::vector<ImU32> layer0;

    // If initial data is provided, use it to initialize the layer
    if (!initial_data.empty())
        layer0 = initial_data;
    else {
        // Create a blank canvas
        for (int i = 0; i < width * height; i++)
            layer0.push_back(IM_COL32(0, 0, 0, 0));
    }

    tiles.push_back(layer0);
    layerVisibility.push_back(true);
}

void cCanvas::Clear() {
    for (auto& tile : g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex])
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
    if (previousCanvases.empty() || (!tiles[g_canvas[g_cidx].selLayerIndex].empty() && !previousCanvases.empty() && !g_util.IsTilesEqual(tiles[g_canvas[g_cidx].selLayerIndex], previousCanvases.back()))) {
        previousCanvases.push_back(tiles[g_canvas[g_cidx].selLayerIndex]);
        canvas_idx = previousCanvases.size() - 1;
        printf("Canvas state created.\n");
    }
    else
        printf("No changes detected; canvas state not updated.\n");
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

    // Prevent our canvas index from going out of bounds
    if (g_cidx == g_canvas.size()) g_cidx--;
}

// Function to calculate the bounding box of the selected indexes
ImVec2 GetTilePos(uint16_t index, float tileSize, float camX, float camY, int width) {
    const float x = index % width;
    const float y = index / width;
    return ImVec2(camX + x * tileSize, camY + y * tileSize);
}

void DrawSelectionRectangle(ImDrawList* drawList, const std::unordered_set<uint16_t>& indexes, float tileSize, float camX, float camY, int width, ImU32 col, uint8_t thickness) {
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

    drawList->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), IM_COL32_BLACK, 0.0f, 0, thickness * 2);
    drawList->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.0f, 0, thickness);
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
    for (auto& index : selectedIndexes)
        copiedTiles[index] = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
}

void cCanvas::DeleteSelection() {
    if (selectedIndexes.empty()) {
        printf("DeleteSelection: No selected tiles to delete.\n");
        return;
    }
    for (auto& index : selectedIndexes)
        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32(0, 0, 0, 0);

    g_canvas[g_cidx].UpdateCanvasHistory();
}

void DrawLineOnCanvas(int x0, int y0, int x1, int y1, ImU32 color, bool preview = false) {
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    // Calculate the half-thickness for offset calculations
    const int halfThickness = static_cast<int>(g_canvas[g_cidx].line_size / 2.0f);

    while (true) {
        for (int offsetX = -halfThickness; offsetX <= halfThickness; ++offsetX) {
            for (int offsetY = -halfThickness; offsetY <= halfThickness; ++offsetY) {
                int drawX = x0 + offsetX;
                int drawY = y0 + offsetY;

                if (drawX >= 0 && drawX < g_canvas[g_cidx].width && drawY >= 0 && drawY < g_canvas[g_cidx].height) {
                    if (preview) {
                        // Draw the tile preview
                        ImVec2 topLeft = { g_cam.x + drawX * g_canvas[g_cidx].TILE_SIZE, g_cam.y + drawY * g_canvas[g_cidx].TILE_SIZE };
                        ImVec2 bottomRight = { topLeft.x + g_canvas[g_cidx].TILE_SIZE, topLeft.y + g_canvas[g_cidx].TILE_SIZE };
                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
                    }
                    else {
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][drawY * g_canvas[g_cidx].width + drawX] = color;
                    }
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
    const int radius = static_cast<int>(sqrt(pow(endX - startX, 2) + pow(endY - startY, 2)));

    int x = radius;
    int y = 0;
    int radiusError = 1 - x;

    while (x >= y) {
        auto plot8points = [&](int x, int y) {
            const int points[8][2] = {
                {startX + x, startY + y},
                {startX - x, startY + y},
                {startX + x, startY - y},
                {startX - x, startY - y},
                {startX + y, startY + x},
                {startX - y, startY + x},
                {startX + y, startY - x},
                {startX - y, startY - x},
            };

            for (auto& point : points) {
                const int px = point[0];
                const int py = point[1];
                if (px >= 0 && px < g_canvas[g_cidx].width && py >= 0 && py < g_canvas[g_cidx].height) {
                    if (preview) {
                        const ImVec2 topLeft = { g_cam.x + px * g_canvas[g_cidx].TILE_SIZE, g_cam.y + py * g_canvas[g_cidx].TILE_SIZE };
                        const ImVec2 bottomRight = { topLeft.x + g_canvas[g_cidx].TILE_SIZE, topLeft.y + g_canvas[g_cidx].TILE_SIZE };
                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
                    }
                    else {
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][py * g_canvas[g_cidx].width + px] = color;
                    }
                }
            }
        };

        plot8points(x, y);

        y++;
        if (radiusError < 0) {
            radiusError += 2 * y + 1;
        }
        else {
            x--;
            radiusError += 2 * (y - x + 1);
        }
    }
}

void DrawRectangleOnCanvas(int x0, int y0, int x1, int y1, ImU32 color, bool preview = false) {
    const int startX = std::min(x0, x1);
    const int endX = std::max(x0, x1);
    const int startY = std::min(y0, y1);
    const int endY = std::max(y0, y1);

    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            // Check if the current pixel is on the boundary
            if (x == startX || x == endX || y == startY || y == endY) {
                if (x >= 0 && x < g_canvas[g_cidx].width && y >= 0 && y < g_canvas[g_cidx].height) {
                    if (preview) {
                        const ImVec2 topLeft = { g_cam.x + x * g_canvas[g_cidx].TILE_SIZE, g_cam.y + y * g_canvas[g_cidx].TILE_SIZE };
                        const ImVec2 bottomRight = { topLeft.x + g_canvas[g_cidx].TILE_SIZE, topLeft.y + g_canvas[g_cidx].TILE_SIZE };
                        ImGui::GetBackgroundDrawList()->AddRectFilled(topLeft, bottomRight, color);
                    }
                    else
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][y * g_canvas[g_cidx].width + x] = color;
                }
            }
        }
    }
}

void cCanvas::UpdateZoom() {
    // Zooming
    if (ImGui::GetIO().MouseWheel != 0.f) {
        const float minZoom = 2.0f;
        const float maxZoom = 50.0f;

        // Calculate new zoom level
        const float newZoom = glm::clamp(TILE_SIZE + ImGui::GetIO().MouseWheel * 4, minZoom, maxZoom);
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
}

void cCanvas::Editor() {
    if (g_canvas.empty() || g_canvas[g_cidx].tiles.empty() || g_canvas[g_cidx].layerVisibility.empty()) return;
    auto& d = *ImGui::GetBackgroundDrawList(); auto& io = ImGui::GetIO();

    if (!g_app.ui_state) {
        key_state.update();

        if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('Z') & 1) {
            if (g_canvas[g_cidx].canvas_idx > 0) {
                g_canvas[g_cidx].canvas_idx--;
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
            }
        }
        else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('Y') & 1) {
            if (g_canvas[g_cidx].canvas_idx < g_canvas[g_cidx].previousCanvases.size() - 1) {
                g_canvas[g_cidx].canvas_idx++;
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
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
        else if (GetAsyncKeyState(VK_CONTROL) && key_state.key_pressed('V') && !copiedTiles.empty()) // Paste our selection area
            PasteSelection();
        else if (key_state.key_pressed('B')) paintToolSelected = TOOL_BRUSH;
        else if (key_state.key_pressed('G')) paintToolSelected = TOOL_BUCKET;
        else if (key_state.key_pressed('E')) paintToolSelected = TOOL_ERASER;
        else if (key_state.key_pressed('X')) paintToolSelected = TOOL_DROPPER;
        else if (key_state.key_pressed('M')) paintToolSelected = TOOL_MOVE;
        else if (key_state.key_pressed('W')) paintToolSelected = TOOL_WAND;
        else if (key_state.key_pressed('S')) paintToolSelected = TOOL_SELECT;

        UpdateZoom();
    }

    for (float y = 0; y < height; y++) {
        for (float x = 0; x < width; x++) {
            const uint64_t index = static_cast<uint64_t>(x) + static_cast<uint64_t>(y) * width;

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
                if (!g_canvas[g_cidx].layerVisibility[layer])
                    continue; // Skip drawing this layer if it is not visible

                const ImU32 tileColor = g_canvas[g_cidx].tiles[layer][index];

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

    const bool bCanDraw = !g_util.IsClickingOutsideCanvas() && x >= 0 && x < g_canvas[g_cidx].width && y >= 0 && y < g_canvas[g_cidx].height;
    static ImVec2 mouseStart; static ImVec2 lastMousePos = ImVec2(-1, -1);
    if (g_util.MousePressed(0)) mouseStart = ImGui::GetMousePos();
    const float brushRadius = brush_size / 2.0f;

    if (bCanDraw && g_util.Hovering(g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE, g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE)) {
        switch (paintToolSelected) {
        case TOOL_BRUSH:
            //brush
            if (io.MouseDown[0] || io.MouseDown[1]) {
                // Check if there is a valid previous position
                if (lastMousePos.x >= 0 && lastMousePos.y >= 0) {
                    const int lastX = static_cast<int>((lastMousePos.x - g_cam.x) / TILE_SIZE);
                    const int lastY = static_cast<int>((lastMousePos.y - g_cam.y) / TILE_SIZE);

                    // Calculate the distance between the previous and current mouse positions
                    const float distX = x - lastX;
                    const float distY = y - lastY;
                    const float distance = glm::sqrt(distX * distX + distY * distY);

                    // Number of steps to interpolate
                    const int steps = static_cast<int>(distance) + 1;

                    for (int i = 0; i <= steps; ++i) {
                        const float t = static_cast<float>(i) / steps;
                        const int brushX = static_cast<int>(lastX + t * distX);
                        const int brushY = static_cast<int>(lastY + t * distY);

                        // Apply brush effect at the interpolated position
                        if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)brushX + (uint64_t)brushY * width) != selectedIndexes.end()) {
                            for (int offsetY = -brushRadius; offsetY <= brushRadius; ++offsetY) {
                                for (int offsetX = -brushRadius; offsetX <= brushRadius; ++offsetX) {
                                    float distance = glm::sqrt(offsetX * offsetX + offsetY * offsetY);
                                    if (distance <= brushRadius) {
                                        const int finalX = brushX + offsetX;
                                        const int finalY = brushY + offsetY;
                                        if (finalX >= 0 && finalX < width && finalY >= 0 && finalY < height) {
                                            if (io.MouseDown[0])
                                                tiles[g_canvas[g_cidx].selLayerIndex][finalX + finalY * width] = myCols[selColIndex];
                                            else if (io.MouseDown[1])
                                                tiles[g_canvas[g_cidx].selLayerIndex][finalX + finalY * width] = IM_COL32(0, 0, 0, 0);
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
                    const float distance = glm::sqrt(offsetX * offsetX + offsetY * offsetY);
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
        case TOOL_BUCKET:
            if (g_util.MousePressed(0))
                if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)x + (uint64_t)y * width) != selectedIndexes.end())
                    floodFill((int)x, (int)y, true);

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
            break;
        case TOOL_ERASER:
            if (io.MouseDown[0])
                if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)x + (uint64_t)y * width) != selectedIndexes.end())
                    if (ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().x < io.DisplaySize.x - 1 && ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().y < io.DisplaySize.y - 1)
                        tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width] = IM_COL32(0, 0, 0, 0);

            d.AddRect({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 1, g_cam.y + y * TILE_SIZE + TILE_SIZE - 1 }, IM_COL32(0, 0, 0, 255));
            d.AddRectFilled({ g_cam.x + x * TILE_SIZE + 1, g_cam.y + y * TILE_SIZE + 1 }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 2, g_cam.y + y * TILE_SIZE + TILE_SIZE - 2 }, IM_COL32(255, 255, 255, 255));
            break;
        case TOOL_DROPPER:
            if (io.MouseDown[0])
                myCols[selColIndex] = tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width];

            d.AddRect({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 1, g_cam.y + y * TILE_SIZE + TILE_SIZE - 1 }, IM_COL32(0, 0, 0, 255));
            d.AddRectFilled({ g_cam.x + x * TILE_SIZE + 1, g_cam.y + y * TILE_SIZE + 1 }, { g_cam.x + x * TILE_SIZE + TILE_SIZE - 2, g_cam.y + y * TILE_SIZE + TILE_SIZE - 2 }, IM_COL32(255, 255, 255, 255));
            break;
        case TOOL_SELECT:
            if (g_util.MousePressed(0))
                selectedIndexes.clear();

            if (ImGui::IsMouseDown(0)) {
                d.AddRect(mouseStart, ImGui::GetMousePos(), IM_COL32_BLACK, 0, NULL, 4);
                d.AddRect(mouseStart, ImGui::GetMousePos(), IM_COL32_WHITE, 0, NULL, 2);
            }

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

                        if (tilePosX >= startX && tilePosX < endX && tilePosY >= startY && tilePosY < endY)
                            selectedIndexes.insert((uint16_t)(selectX + selectY * width));
                    }
                }
            }

            break;

        case TOOL_WAND:
            if (g_util.MousePressed(0)) {
                const ImVec2 mousePos = ImGui::GetMousePos();
                const int tileX = static_cast<int>((mousePos.x - g_cam.x) / TILE_SIZE);
                const int tileY = static_cast<int>((mousePos.y - g_cam.y) / TILE_SIZE);

                if (tileX >= 0 && tileX < width && tileY >= 0 && tileY < height)
                    floodFill(tileX, tileY, false);
            }

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
            break;

        case TOOL_MOVE:
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
                for (const auto& index : initialSelectedIndexes)
                    tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32(0, 0, 0, 0);

                // Update new positions
                for (const auto& [newIndex, color] : newTileColors)
                    tiles[g_canvas[g_cidx].selLayerIndex][newIndex] = color;

                selectedIndexes = newSelectedIndexes;
            }

            break;
        case TOOL_LINE:
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

            break;
        case TOOL_SQUARE:
            if (ImGui::IsMouseDown(0)) {
                // Draw the preview rectangle
                const ImVec2 mousePos = ImGui::GetMousePos();
                const uint16_t endX = static_cast<int>((mousePos.x - g_cam.x) / TILE_SIZE);
                const uint16_t endY = static_cast<int>((mousePos.y - g_cam.y) / TILE_SIZE);

                DrawRectangleOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), endX, endY, myCols[selColIndex], true);
            }
            else
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);

            if (g_util.MouseReleased(0)) {
                // Convert the screen coordinates to tile coordinates
                const uint16_t startX = static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE);
                const uint16_t startY = static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE);

                // Draw the rectangle on the canvas
                DrawRectangleOnCanvas(startX, startY, x, y, myCols[selColIndex]);
            }

            break;
        case TOOL_ELIPSE:
            if (ImGui::IsMouseDown(0)) {
                // Draw the preview ellipse
                const ImVec2 mousePos = ImGui::GetMousePos();
                const uint16_t endX = static_cast<int>((mousePos.x - g_cam.x) / TILE_SIZE);
                const uint16_t endY = static_cast<int>((mousePos.y - g_cam.y) / TILE_SIZE);

                DrawCircleOnCanvas(static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE), static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE), endX, endY, myCols[selColIndex], true);
            }
            else
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);

            if (g_util.MouseReleased(0)) {
                // Convert the screen coordinates to tile coordinates
                const uint16_t startX = static_cast<int>((mouseStart.x - g_cam.x) / TILE_SIZE);
                const uint16_t startY = static_cast<int>((mouseStart.y - g_cam.y) / TILE_SIZE);

                // Draw the ellipse on the canvas
                DrawCircleOnCanvas(startX, startY, x, y, myCols[selColIndex]);
            }

            break;
        }
    }

    // Draw a rectangle around the selected indexes
    if (!selectedIndexes.empty())
        DrawSelectionRectangle(&d, selectedIndexes, TILE_SIZE, g_cam.x, g_cam.y, width, IM_COL32_WHITE, 2);

    // Add canvas to history for undo-redo feature
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

    // Update the previous mouse position for the next frame
    lastMousePos = ImGui::GetMousePos();
}