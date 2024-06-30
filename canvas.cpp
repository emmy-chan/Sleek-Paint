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

//void floodFill(int x, int y, ImColor curCol, ImColor col)
//{
//    if (x < g_canvas[g_cidx].width && y < g_canvas[g_cidx].height && y >= 0 && x >= 0 && g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][x + y * g_canvas[g_cidx].width] == curCol && curCol != col)
//    {
//        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][x + y * g_canvas[g_cidx].width] = col;
//        floodFill(x + 1, y, curCol, col);
//        floodFill(x, y + 1, curCol, col);
//        floodFill(x - 1, y, curCol, col);
//        floodFill(x, y - 1, curCol, col);
//    }
//}

#include <stack>
#include <utility> // for std::pair
#include <unordered_set>

std::unordered_set<uint16_t> selectedIndexes;
std::unordered_map<uint16_t, ImU32> copiedTiles; // Store copied tiles and their colors

// Flood fill function
void floodFill(int x, int y, bool paint)
{
    if (x < 0 || x >= g_canvas[g_cidx].width || y < 0 || y >= g_canvas[g_cidx].height) {
        printf("FloodFill: Out of bounds initial position\n");
        return;
    }

    ImU32 curCol = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][x + y * g_canvas[g_cidx].width];
    ImU32 fillCol = paint ? g_canvas[g_cidx].myCols[g_canvas[g_cidx].selColIndex] : curCol;

    if (curCol == fillCol && paint) {
        printf("FloodFill: Current color is the same as fill color\n");
        return;
    }

    std::stack<std::pair<int, int>> stack;
    stack.push({ x, y });

    if (!paint)
        selectedIndexes.clear();

    while (!stack.empty())
    {
        std::pair<int, int> p = stack.top();
        stack.pop();
        int curX = p.first;
        int curY = p.second;

        if (curX < 0 || curX >= g_canvas[g_cidx].width || curY < 0 || curY >= g_canvas[g_cidx].height)
            continue;

        uint16_t currentIndex = curX + curY * g_canvas[g_cidx].width;

        if (paint) {
            if (!selectedIndexes.empty() && selectedIndexes.find(currentIndex) == selectedIndexes.end())
                continue;

            if (g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex] != curCol)
                continue;

            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex] = fillCol;
        }
        else {
            if (g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex] != curCol || selectedIndexes.find(currentIndex) != selectedIndexes.end())
                continue;

            selectedIndexes.insert(currentIndex);
        }

        stack.push({ curX + 1, curY });
        stack.push({ curX - 1, curY });
        stack.push({ curX, curY + 1 });
        stack.push({ curX, curY - 1 });
    }

    printf("FloodFill: Completed without freezing\n");
}

//void floodSelect(std::vector<uint16_t>& selectedIndexes, int x, int y, ImU32 curCol, ImU32 col)
//{
//    //if (x < g_canvas[g_cidx].width && y < height && y >= 0 && x >= 0 && tiles[x + y * width] == curCol && curCol != col)
//    //{
//    //    selectedIndexes.emplace_back(x + y * width);
//    //    std::string txt = "x: " + std::to_string(x) + " y: " + std::to_string(y) + "\n";
//    //    //printf(txt.c_str());
//    //    floodSelect(selectedIndexes, x + 1, y, curCol, col);
//    //    floodSelect(selectedIndexes, x, y + 1, curCol, col);
//    //    floodSelect(selectedIndexes, x - 1, y, curCol, col);
//    //    floodSelect(selectedIndexes, x, y - 1, curCol, col);
//    //}
//}

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

    //Create state for (undo) previous canvas of blank canvas
    UpdateCanvasHistory();
}

void cCanvas::Clear() {
    for (auto& tile : g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex]) //int i = 0; i < g_canvas[g_cidx].width * g_canvas[g_cidx].height; i++
        tile = IM_COL32(255, 255, 255, 0);
}

void cCanvas::AdaptNewSize() {
    //g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex].clear();

    for (int i = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex].size(); i < (g_canvas[g_cidx].width * g_canvas[g_cidx].height); i++)
        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex].push_back(IM_COL32(0, 0, 0, 0));

    Clear();
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

    //TWO MAIN COLORS
    //Create our two main colors
    //std::vector<ImU32> main_cols = { IM_COL32(0, 0, 0, 255), IM_COL32(255, 255, 255, 255) };

    ////Push our two main colors to the array
    //for (auto& c : main_cols)
    //    myCols.push_back(c);
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
    const ImVec2 mousePos = ImGui::GetMousePos();
    const int offsetX = static_cast<int>((mousePos.x - g_cam.x) / TILE_SIZE);
    const int offsetY = static_cast<int>((mousePos.y - g_cam.y) / TILE_SIZE);

    std::unordered_set<uint16_t> newSelectedIndexes;
    for (auto& tile : copiedTiles) {
        const int selectX = tile.first % g_canvas[g_cidx].width;
        const int selectY = tile.first / g_canvas[g_cidx].width;
        const int newX = selectX + offsetX;
        const int newY = selectY + offsetY;

        if (newX >= 0 && newX < g_canvas[g_cidx].width && newY >= 0 && newY < g_canvas[g_cidx].height) {
            uint16_t newIndex = newX + newY * g_canvas[g_cidx].width;
            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][newIndex] = tile.second;
            newSelectedIndexes.insert(newIndex);
        }
    }
    selectedIndexes = newSelectedIndexes;

    g_canvas[g_cidx].UpdateCanvasHistory();
}

void cCanvas::CopySelection() {
    copiedTiles.clear();
    for (auto& index : selectedIndexes) {
        copiedTiles[index] = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
    }
}

void cCanvas::DeleteSelection() {
    for (auto& index : selectedIndexes) {
        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = IM_COL32(0, 0, 0, 0);
    }

    g_canvas[g_cidx].UpdateCanvasHistory();
}

void DrawLineOnCanvas(int x0, int y0, int x1, int y1, ImU32 color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < g_canvas[g_cidx].width && y0 >= 0 && y0 < g_canvas[g_cidx].height) {
            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][y0 * g_canvas[g_cidx].width + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
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

        if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('Z') & 1) {
            if (g_canvas[g_cidx].canvas_idx > 0) {
                g_canvas[g_cidx].canvas_idx--;
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
            }
        }

        if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('Y') & 1) {
            if (g_canvas[g_cidx].canvas_idx < g_canvas[g_cidx].previousCanvases.size() - 1) {
                g_canvas[g_cidx].canvas_idx++;
                g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
            }
        }

        //Zooming
        if (io.MouseWheel != 0.f) {
            //zoom = TILE_SIZE * 4;
            if (TILE_SIZE >= 16) {
                zoom += io.MouseWheel * 128;
            }
            zoom = glm::clamp(zoom, 0.f, 3520.f); //-592.f, 296.f

            //Prevent underflow
            if (TILE_SIZE >= 4) {
                TILE_SIZE += io.MouseWheel * 4;
            }
            else
                TILE_SIZE += io.MouseWheel * 2;

            TILE_SIZE = glm::clamp<int>(TILE_SIZE, 2, 126); //io.DisplaySize.y * 0.1
        }

        //if (io.MouseWheel > 0.f) {
        //    
        //    g_cam = { g_cam.x + (io.MousePos.x - (150 + width * TILE_SIZE) / 2) / TILE_SIZE, g_cam.y + (io.MousePos.y - (20 + height * TILE_SIZE) / 2) / TILE_SIZE }; //g_cam.x + (io.MousePos.x - (io.DisplaySize.x) / 2) / 4, g_cam.y + (io.MousePos.y - (io.DisplaySize.y) / 2) / 4
        //}
        //else if (io.MouseWheel < 0.f) {
        //    g_cam = { g_cam.x + ((width * TILE_SIZE) / 2 - io.MousePos.x) / TILE_SIZE, g_cam.y + ((height * TILE_SIZE) / 2 - io.MousePos.y) / TILE_SIZE };
        //}
    }

    //Debug stuff
    //ImGui::SetNextWindowPos({ 555, 555 }, ImGuiCond_Once);
    //std::string ya = "Zoom: " + std::to_string(zoom);
    //ImGui::Text(ya.c_str());
    //ya = "Tile size: " + std::to_string(TILE_SIZE);
    //ImGui::Text(ya.c_str());
    //ya = "Canvas state size: " + std::to_string(g_canvas[g_cidx].previousCanvases.size());
    //ImGui::Text(ya.c_str());
    ////ya = "Tile 0 alpha " + std::to_string(g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][0].Value.w);
    ////ImGui::Text(ya.c_str());
    ////ya = "Canvas idx: " + std::to_string(g_canvas[g_cidx].canvas_idx);
    ////ImGui::Text(ya.c_str());

    ////Mouse pos debug
    //ya = "mouse x: " + std::to_string((int)io.MousePos.x) 
    //    + "\nmouse y: " + std::to_string((int)io.MousePos.y);
    //ImGui::Text(ya.c_str());

    //state system please
    //if (paintToolSelected == 4) {
    //    static glm::vec2 mouse_rect = { io.MousePos.x, io.MousePos.y };
    //    const glm::vec2 mouse_rect_delta = { (io.MousePos.x - mouse_rect.x), (io.MousePos.y - mouse_rect.y) };

    //    if (io.MouseDown[0]) {
    //        d.AddRect({ mouse_rect.x, mouse_rect.y }, { mouse_rect.x + mouse_rect_delta.x, mouse_rect.y + mouse_rect_delta.y }, ImColor(0, 0, 0, 255), 0, 0, TILE_SIZE);
    //        selectedIndexes.clear();
    //    }
    //    else {
    //        mouse_rect = { io.MousePos.x, io.MousePos.y };
    //    }

    //    //Released the mouse
    //    if (last_pressed && !io.MouseDown[0]) {
    //        for (float y = 0; y < height; y++) {
    //            for (float x = 0; x < width; x++) {
    //                if (Hovering(mouse_rect.x, mouse_rect.y, mouse_rect.x + mouse_rect_delta.x, mouse_rect.y + mouse_rect_delta.y))
    //                    selectedIndexes.push_back((x + y * width) / TILE_SIZE);
    //                
    //                printf(std::string(std::to_string((x + y * width) / TILE_SIZE) + "\n").c_str());
    //            }
    //        }
    //    }
    //}

    /*glm::u32vec2 mouse = g_util.MapCoordsToRect({ io.MousePos.x, io.MousePos.y }, g_canvas[g_cidx].width * g_canvas[g_cidx].TILE_SIZE, g_canvas[g_cidx].height * g_canvas[g_cidx].TILE_SIZE);

    std::string txt = "mouse x: " + std::to_string(mouse.x)
        + "\nmouse y: " + std::to_string(mouse.y);
    ImGui::Text(txt.c_str());*/

    // Delete our selection area
    if (GetAsyncKeyState(VK_DELETE))
        DeleteSelection();

    // Copy our selection area
    if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState('C'))
        CopySelection();

    // Paste our selection area
    if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState('V') && !copiedTiles.empty())
        PasteSelection();

    for (float y = 0; y < height; y++) {
        for (float x = 0; x < width; x++) {
            // Add non selected tiles to list
            if (selectedIndexes.find((uint64_t)x + (uint64_t)y * width) == selectedIndexes.end())
                nonSelectedIndexes.insert((uint64_t)x + (uint64_t)y * width);

            // Draw grid background for the base layer
            if (((g_canvas[g_cidx].tiles[0][static_cast<uint64_t>(x) + static_cast<uint64_t>(y) * width] >> 24) & 0xFF) != 255) {
                const ImU32 col = (static_cast<int>(x) + static_cast<int>(y)) % 2 == 0 ? IM_COL32(110, 110, 110, 255) : IM_COL32(175, 175, 175, 255);
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, col);
            }

            // Draw each layer from bottom to top, including layers after the selected layer index
            for (size_t layer = 0; layer < g_canvas[g_cidx].tiles.size(); layer++) {
                ImU32 tileColor = g_canvas[g_cidx].tiles[layer][static_cast<uint64_t>(x) + static_cast<uint64_t>(y) * width];
                d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, tileColor);
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
    static ImVec2 start;

    if (bCanDraw && g_util.Hovering(g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE, g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE)) {
        switch (paintToolSelected) {
        case 0:
            //brush
            if (selectedIndexes.empty() || selectedIndexes.find((uint64_t)x + (uint64_t)y * width) != selectedIndexes.end()) {
                if (io.MouseDown[0]) {
                    if (ImGui::GetMousePos().x > 0 && ImGui::GetMousePos().x < io.DisplaySize.x - 1 && ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().y < io.DisplaySize.y - 1)
                        tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width] = myCols[selColIndex];
                }
                else if (io.MouseDown[1]) {
                    if (ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().x < io.DisplaySize.x - 1 && ImGui::GetMousePos().y > 0 && ImGui::GetMousePos().y < io.DisplaySize.y - 1)
                        tiles[g_canvas[g_cidx].selLayerIndex][(uint64_t)x + (uint64_t)y * width] = IM_COL32(0, 0, 0, 0);
                }
            }

            d.AddRectFilled({ g_cam.x + x * TILE_SIZE, g_cam.y + y * TILE_SIZE }, { g_cam.x + x * TILE_SIZE + TILE_SIZE, g_cam.y + y * TILE_SIZE + TILE_SIZE }, myCols[selColIndex]);
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
            if (g_util.MousePressed(0)) {
                selectedIndexes.clear();
                start = ImGui::GetMousePos();
            }

            if (ImGui::IsMouseDown(0))
                d.AddRect(start, ImGui::GetMousePos(), IM_COL32_WHITE, 0, NULL, 4);

            if (g_util.MouseReleased(0)) {
                const ImVec2 end = ImGui::GetMousePos();
                const float startX = std::floor(std::min(start.x, end.x) / TILE_SIZE) * TILE_SIZE;
                const float startY = std::floor(std::min(start.y, end.y) / TILE_SIZE) * TILE_SIZE;
                const float endX = std::ceil(std::max(start.x, end.x) / TILE_SIZE) * TILE_SIZE;
                const float endY = std::ceil(std::max(start.y, end.y) / TILE_SIZE) * TILE_SIZE;

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
                start = ImGui::GetMousePos();
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
                if (start.x < minX || start.x > maxX || start.y < minY || start.y > maxY) {
                    selectedIndexes.clear();
                    initialSelectedIndexes.clear();
                }
            }

            if (ImGui::IsMouseDown(0)) {
                ImVec2 offset = ImGui::GetMousePos();
                offset.x -= start.x; offset.y -= start.y;

                for (const auto& index : initialSelectedIndexes) {
                    const int selectX = index % width;
                    const int selectY = index / width;
                    const ImVec2 tilePos = { g_cam.x + selectX * TILE_SIZE + offset.x, g_cam.y + selectY * TILE_SIZE + offset.y };
                    d.AddRectFilled(tilePos, { tilePos.x + TILE_SIZE, tilePos.y + TILE_SIZE }, tiles[g_canvas[g_cidx].selLayerIndex][index]);
                }
            }

            if (g_util.MouseReleased(0)) {
                ImVec2 offset = ImGui::GetMousePos();
                offset.x -= start.x; offset.y -= start.y;

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
    static ImVec2 lineStart;
    if (paintToolSelected == 7) {
        if (g_util.MousePressed(0)) {
            lineStart = ImGui::GetMousePos();
        }

        if (ImGui::IsMouseDown(0)) {
            // Draw the preview line
            d.AddLine(lineStart, ImGui::GetMousePos(), myCols[selColIndex], 4);
        }

        if (g_util.MouseReleased(0)) {
            // Convert the screen coordinates to tile coordinates
            const uint16_t startX = static_cast<int>((lineStart.x - g_cam.x) / TILE_SIZE);
            const uint16_t startY = static_cast<int>((lineStart.y - g_cam.y) / TILE_SIZE);

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