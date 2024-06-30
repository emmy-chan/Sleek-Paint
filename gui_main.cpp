#include "gui_main.h"
#include "camera.h"
#include "app.h"
#include "canvas.h"
#include "assets.h"
#include <string>
#include "ui_states.h"

#include "data_manager.h"
#include "utils.h"
#include <unordered_set>
#include "imgui_internal.h"

ImU32 adjustSaturation(ImU32 color, float saturationFactor) {
    float r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    float g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    float b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    float gray = 0.299 * r + 0.587 * g + 0.114 * b;
    r = std::min(255.0f, gray + (r - gray) * saturationFactor);
    g = std::min(255.0f, gray + (g - gray) * saturationFactor);
    b = std::min(255.0f, gray + (b - gray) * saturationFactor);

    return IM_COL32((int)r, (int)g, (int)b, 255);
}

ImU32 adjustContrast(ImU32 color, float contrastFactor) {
    float r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    float g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    float b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    r = 128 + (r - 128) * contrastFactor;
    g = 128 + (g - 128) * contrastFactor;
    b = 128 + (b - 128) * contrastFactor;

    r = std::min(255.0f, std::max(0.0f, r));
    g = std::min(255.0f, std::max(0.0f, g));
    b = std::min(255.0f, std::max(0.0f, b));

    return IM_COL32((int)r, (int)g, (int)b, 255);
}

// PS1-style dithering function
ImU32 applyDithering(ImU32 color, uint64_t x, uint64_t y) {
    // Extract color components
    uint8_t r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    uint8_t g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    uint8_t b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    // Apply PS1-style dithering pattern
    const uint8_t ditherMatrix[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5}
    };

    uint8_t ditherThreshold = ditherMatrix[y % 4][x % 4];

    // Reduce color depth to 5 bits per channel
    r = (r & 0xF8) | (r >> 5);
    g = (g & 0xF8) | (g >> 5);
    b = (b & 0xF8) | (b >> 5);

    // Apply dithering based on the threshold
    if ((r & 7) > ditherThreshold) r |= 0x08;
    if ((g & 7) > ditherThreshold) g |= 0x08;
    if ((b & 7) > ditherThreshold) b |= 0x08;

    return IM_COL32(r, g, b, 255);
}

void cGUI::Display()
{
    auto& io = ImGui::GetIO();

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        ImGui::BeginMainMenuBar();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_FA_PLUS_SQUARE" New Project"))
                g_app.ui_state = std::make_unique<cUIStateNewProject>();

            if (ImGui::MenuItem(ICON_FA_FILE " Open Project"))
                g_app.ui_state = std::make_unique<cUIStateOpenProject>();

            if (ImGui::MenuItem(ICON_FA_SAVE" Save Project") && g_canvas.size() > 0)
                g_app.ui_state = std::make_unique<cUIStateSaveProject>();

            if (ImGui::MenuItem("Exit")) {
                if (g_canvas.size() > 0) {
                    // warn user about quitting with shit open
                }
                else
                    exit(EXIT_SUCCESS);
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem(ICON_FA_UNDO" Undo") && g_canvas.size() > 0) {
                if (g_canvas[g_cidx].canvas_idx > 0) {
                    g_canvas[g_cidx].canvas_idx--;
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
                }
            }
            if (ImGui::MenuItem(ICON_FA_REDO" Redo") && g_canvas.size() > 0) {
                if (g_canvas[g_cidx].canvas_idx < g_canvas[g_cidx].previousCanvases.size() - 1) {
                    g_canvas[g_cidx].canvas_idx++;
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = g_canvas[g_cidx].previousCanvases[g_canvas[g_cidx].canvas_idx];
                }
            }
            if (ImGui::MenuItem(ICON_FA_CUT" Cut") && g_canvas.size() > 0) {
                    g_canvas[g_cidx].CopySelection();
                    g_canvas[g_cidx].DeleteSelection();
            }
            if (ImGui::MenuItem(ICON_FA_COPY" Copy") && g_canvas.size() > 0) {
                    g_canvas[g_cidx].CopySelection();
            }
            if (ImGui::MenuItem(ICON_FA_PASTE" Paste") && g_canvas.size() > 0) {
                    g_canvas[g_cidx].PasteSelection();
            }
            
            if (ImGui::MenuItem(ICON_FA_EXCHANGE " Convert color to palette") && g_canvas.size() > 0) {
                for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                    for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                        const uint64_t index = x + y * g_canvas[g_cidx].width;
                        const ImU32 currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                        // Skip processing for fully transparent tiles
                        if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) == 0) continue; 

                        ImU32 closestColor = g_canvas[g_cidx].myCols[0];
                        int minDistance = g_util.ColorDistanceSquared(currentColor, closestColor);

                        // Compare current tile color to each color in myCols to find the closest one
                        for (size_t i = 1; i < g_canvas[g_cidx].myCols.size(); i++) {
                            const int distance = g_util.ColorDistanceSquared(currentColor, g_canvas[g_cidx].myCols[i]);
                            if (distance < minDistance) {
                                closestColor = g_canvas[g_cidx].myCols[i];
                                minDistance = distance;
                            }
                        }

                        // Set the tile's color to the closest color found
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = closestColor;
                    }
                }

                g_canvas[g_cidx].UpdateCanvasHistory(); // Update history after processing all tiles
            }

            if (ImGui::MenuItem(ICON_FA_COPY " Derive Color Palette") && g_canvas.size() > 0) {
                // Clear the myCols vector
                g_canvas[g_cidx].myCols.clear();

                // Create our two main colors
                std::vector<ImU32> main_cols = { IM_COL32(0, 0, 0, 255), IM_COL32(255, 255, 255, 255) };

                //Push our two main colors to the array
                for (auto& c : main_cols)
                    g_canvas[g_cidx].myCols.push_back(c);

                // Create a set to store unique colors
                std::unordered_set<ImU32> uniqueColors;

                // Iterate over the canvas to collect unique colors
                for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                    for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                        uint64_t index = x + y * g_canvas[g_cidx].width;
                        ImU32 currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
                        uniqueColors.insert(currentColor);
                    }
                }

                // Push unique colors to the myCols vector
                for (const auto& color : uniqueColors) {
                    g_canvas[g_cidx].myCols.push_back(color);
                }

                // Make sure colors are not duplicates
                for (int i = 0; i < g_canvas[g_cidx].myCols.size() - 1; i++) {
                    bool duplicateFound = false;
                    int duplicateIndex = -1;

                    // Check for duplicate colors
                    for (int j = g_canvas[g_cidx].myCols.size() - 1; j > i; j--) {
                        if (g_canvas[g_cidx].myCols[j] == g_canvas[g_cidx].myCols[i]) {
                            duplicateFound = true;
                            duplicateIndex = j;
                            break;
                        }
                    }

                    if (duplicateFound) {
                        g_canvas[g_cidx].myCols.erase(g_canvas[g_cidx].myCols.begin() + duplicateIndex);
                        //goto retry_draw;
                    }
                }
            }

            if (ImGui::MenuItem(ICON_FA_ASTERISK " Apply dithering") && g_canvas.size() > 0) {
                // Apply the changes to the canvas
                for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                    for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                        uint64_t index = x + y * g_canvas[g_cidx].width;
                        ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
                        currentColor = applyDithering(currentColor, x, y);
                    }
                }
            }

            if (ImGui::CollapsingHeader("Color Adjustments")) {
                float saturationFactor = 1.0f; // Default to no change in saturation
                float contrastFactor = 1.0f;   // Default to no change in contrast
                bool changed = false; // Track if any adjustments are made

                // Saturation slider
                if (ImGui::SliderFloat("Saturation", &saturationFactor, 0.0f, 2.0f, "Factor: %.2f")) {
                    changed = true;
                }

                // Contrast slider
                if (ImGui::SliderFloat("Contrast", &contrastFactor, 0.5f, 2.0f, "Factor: %.2f")) {
                    changed = true;
                }

                if (changed && g_canvas.size() > 0) {
                    // Apply the changes to the canvas
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            uint64_t index = x + y * g_canvas[g_cidx].width;
                            ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                            // Adjust saturation and contrast
                            currentColor = adjustSaturation(currentColor, saturationFactor);
                            currentColor = adjustContrast(currentColor, contrastFactor);
                        }
                    }
                }

                if (ImGui::IsMouseReleased(0)) g_canvas[g_cidx].UpdateCanvasHistory(); // Update history to record the state 
            }

            ImGui::EndMenu();
        }


        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem(ICON_FA_IMAGE" Canvas Size") && g_canvas.size() > 0)
                g_app.ui_state = std::make_unique<cUIStateCanvasSize>();

            if (ImGui::MenuItem(ICON_FA_FILE" Load Color Palette") && g_canvas.size() > 0)
                g_app.ui_state = std::make_unique<cUIStateLoadPalette>();

            if (ImGui::MenuItem(ICON_FA_SAVE" Save Color Palette") && g_canvas.size() > 0)
                g_app.ui_state = std::make_unique<cUIStateSavePalette>();

            ImGui::EndMenu();
        }

        // Dont show any tools if we dont have any projects open.
        if (g_canvas.empty())
            return;

        ImGui::SetCursorPosX(197);
        if (ImGui::BeginTabBar("ProjectTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
        {
            for (uint16_t i = 0; i < g_canvas.size(); i++) {
                bool open = true;

                if (open && ImGui::BeginTabItem(g_canvas[i].name.c_str(), &open, ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyResizeDown))
                {
                    g_cidx = i;
                    ImGui::EndTabItem();
                }

                if (!open) {
                    //If we didn't do anything in this canvas
                    //let's just delete it without asking
                    if (g_canvas[g_cidx].previousCanvases.size() > 1) {
                        //Maybe make this constructor take in the old value and return to our old scene / project index once we do something there?
                        g_app.ui_state = std::make_unique<cUIStateSaveWarning>();
                    }
                    else
                        g_canvas[g_cidx].DestroyCanvas();
                }
            }

            if (!g_canvas.empty()) {
                std::string string = "Brush Size";

                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 135 - ImGui::CalcTextSize(string.c_str()).x);
                ImGui::PushItemWidth(125);

                int temp_brush_size = static_cast<int>(g_canvas[g_cidx].brush_size); // Create a temporary int variable

                // Use the temporary int variable with ImGui::SliderInt
                if (ImGui::SliderInt(string.c_str(), &temp_brush_size, 1, 10)) {
                    g_canvas[g_cidx].brush_size = static_cast<uint8_t>(temp_brush_size); // Assign the value back to your uint8_t variable
                }

                ImGui::PopItemWidth();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndMainMenuBar();

        auto& style = ImGui::GetStyle();
        ImGui::SetNextWindowPos({ 197, io.DisplaySize.y - 19 });
        ImGui::SetNextWindowSize({ io.DisplaySize.x - 197 - 60, 0 });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.24f, 0.27f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.24f, 0.24f, 0.27f, 1.f));
        style.WindowBorderSize = 0;
        style.GrabMinSize = 150;

        ImGui::Begin("##ScrollH", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PushItemWidth(ImGui::GetWindowWidth());
        ImGui::SliderFloat("##ScrollHorizontal", &g_cam.x, glm::abs(io.DisplaySize.x / 2 - g_canvas[g_cidx].zoom), 150 + -glm::abs(io.DisplaySize.x / 2 - (g_canvas[g_cidx].width * g_canvas[g_cidx].TILE_SIZE) + g_canvas[g_cidx].zoom), "");
        ImGui::End();

        ImGui::SetNextWindowPos({ io.DisplaySize.x - 60, 24 });
        ImGui::SetNextWindowSize({ 20, io.DisplaySize.y - 43 });
        ImGui::Begin("##ScrollV", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::VSliderFloat("##ScrollVertical", { 20, ImGui::GetWindowHeight() }, &g_cam.y, -glm::abs((g_canvas[g_cidx].height * 2 * g_canvas[g_cidx].TILE_SIZE) / 2) - g_canvas[g_cidx].zoom, glm::abs((io.DisplaySize.y - (g_canvas[g_cidx].height * g_canvas[g_cidx].TILE_SIZE) / 2) - 19 + g_canvas[g_cidx].zoom), "");
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        style.WindowBorderSize = 1;
        style.GrabMinSize = 0;

        ImGui::SetNextWindowPos({ io.DisplaySize.x - 40, 23 });
        ImGui::SetNextWindowSize({ 40, io.DisplaySize.y - 22 });
        
        const ImVec4 panelColor = { 0.2f, 0.2f, 0.215f, 1.f };
        const ImVec4 panelActiveColor = { 0.f, 0.46f, 0.78f, 1.f };
        
        ImGui::Begin("##Tools", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 4.5f, 4.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.7f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 5 ? panelActiveColor : panelColor);
        if (ImGui::ImageButton((void*)g_assets.wand_texture, ImVec2(16, 16))) g_canvas[g_cidx].paintToolSelected = 5;
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 4 ? panelActiveColor : panelColor);
        if (ImGui::ImageButton((void*)g_assets.selection_texture, ImVec2(16, 16))) g_canvas[g_cidx].paintToolSelected = 4;
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        //if (ImGui::Button(ICON_FA_PEN_FANCY, ImVec2(25, 25))) g_canvas[g_cidx].paintToolSelected = 4; //ICON_FA_ARROWS
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 6 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_MOUSE_POINTER, ImVec2(25, 25))) g_canvas[g_cidx].paintToolSelected = 6;
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 0 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_PENCIL, ImVec2(25, 25))) g_canvas[g_cidx].paintToolSelected = 0;
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 1 ? panelActiveColor : panelColor);
        if (ImGui::ImageButton((void*)g_assets.bucket_texture, ImVec2(16, 16))) g_canvas[g_cidx].paintToolSelected = 1; //u8"\uf573"
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 7 ? panelActiveColor : panelColor);
        if (ImGui::Button("/", ImVec2(25, 25))) g_canvas[g_cidx].paintToolSelected = 7;
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 2 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_ERASER, ImVec2(25, 25))) g_canvas[g_cidx].paintToolSelected = 2;
        ImGui::PushStyleColor(ImGuiCol_Button, g_canvas[g_cidx].paintToolSelected == 3 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_EYE_DROPPER, ImVec2(25, 25))) g_canvas[g_cidx].paintToolSelected = 3;
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(6);
        ImGui::End();
    }

    auto& draw = *ImGui::GetBackgroundDrawList();
    draw.AddRectFilled({ io.DisplaySize.x - 60, io.DisplaySize.y - 19 }, { io.DisplaySize.x - 40, io.DisplaySize.y }, ImColor(51, 51, 55, 255));

    ImGui::SetNextWindowSize({ 197, io.DisplaySize.y - 300 });
    ImGui::SetNextWindowPos({ 0, 23 });

    ImGui::Begin("##Colors", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });

    retry_draw:
    // Get the current window scroll and position information
    ImGuiWindow& window = *ImGui::GetCurrentWindow();

    // Calculate the visible region (scroll region)
    const float scroll_start_y = window.Scroll.y;
    const float scroll_end_y = window.Scroll.y + window.Size.y;

    // Main color buttons rendering loop
    for (uint16_t i = 0; i < g_canvas[g_cidx].myCols.size(); i++) {
        const std::string id = "Color " + std::to_string(i + 1);

        // Calculate the position of the current color button
        ImVec2 color_pos = ImGui::GetCursorScreenPos();
        float color_pos_y = color_pos.y - window.Pos.y + window.Scroll.y;

        // Check if the color button is within the visible scroll region
        if (color_pos_y >= scroll_start_y && color_pos_y <= scroll_end_y) {
            if (i % 8 != 0) ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_FrameBg, g_canvas[g_cidx].selColIndex == i ? ImVec4(1.0f, 1.0f, 1.0f, 1.f) : ImVec4(0.05f, 0.05f, 0.05f, 1));

            if (ImGui::ColorButton(id.c_str(), ImGui::ColorConvertU32ToFloat4(g_canvas[g_cidx].myCols[i]), NULL, { 21, 21 }))
                g_canvas[g_cidx].selColIndex = i;

            ImGui::PopStyleColor();
        }
        else {
            // If the color button is not in the visible region, advance the cursor
            if (i % 8 != 0) ImGui::SameLine();
            ImGui::Dummy({ 21, 21 });
        }
    }

    if (ImGui::Button("-")) {
        g_canvas[g_cidx].myCols.erase(g_canvas[g_cidx].myCols.begin() + g_canvas[g_cidx].selColIndex);
        if (g_canvas[g_cidx].selColIndex > 0) g_canvas[g_cidx].selColIndex--;
    }

    ImGui::SameLine();

    if (ImGui::Button("+"))
        g_canvas[g_cidx].myCols.push_back(ImColor(0, 0, 0, 255));

    ImGui::PopStyleVar(); ImGui::Spacing(); ImGui::Separator();

    for (size_t i = 0; i < g_canvas[g_cidx].tiles.size(); i++) {
        const std::string name = "Layer " + std::to_string(i + 1);
        const bool isSelected = (g_canvas[g_cidx].selLayerIndex == i);

        // Start drag and drop source
        if (ImGui::Selectable(name.c_str(), isSelected)) {
            g_canvas[g_cidx].selLayerIndex = i;
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("DND_LAYER", &i, sizeof(size_t));
            ImGui::Text("Dragging %s", name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_LAYER")) {
                size_t payloadIndex = *(const size_t*)payload->Data;
                if (payloadIndex != i) {
                    // Swap layers
                    std::swap(g_canvas[g_cidx].tiles[i], g_canvas[g_cidx].tiles[payloadIndex]);
                    // Adjust selected layer index if necessary
                    if (g_canvas[g_cidx].selLayerIndex == payloadIndex) {
                        g_canvas[g_cidx].selLayerIndex = i;
                    }
                    else if (g_canvas[g_cidx].selLayerIndex == i) {
                        g_canvas[g_cidx].selLayerIndex = payloadIndex;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    if (ImGui::Button("Add Layer", { ImGui::GetColumnWidth(), ImGui::GetFrameHeight() }))
        g_canvas[g_cidx].NewLayer();

    if (ImGui::Button("Remove Layer", { ImGui::GetColumnWidth(), ImGui::GetFrameHeight() })) {
        if (g_canvas[g_cidx].selLayerIndex > 0 && g_canvas[g_cidx].tiles.size() > 1) {
            g_canvas[g_cidx].tiles.resize(g_canvas[g_cidx].tiles.size() - 1);
            g_canvas[g_cidx].selLayerIndex--;
        }
    }

    ImGui::End();

    ImGui::SetNextWindowSize({ 197, 278 });
    ImGui::SetNextWindowPos({ 0, io.DisplaySize.y - 278 });
    ImGui::Begin("##Color Picker", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::PushItemWidth(182);

    {
        ImVec4 picker_color = ImGui::ColorConvertU32ToFloat4(g_canvas[g_cidx].myCols[g_canvas[g_cidx].selColIndex]);

        if (ImGui::ColorPicker4("##Picker", (float*)&picker_color, ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) {
            // If color is edited, convert it back to ImU32 and store it
            g_canvas[g_cidx].myCols[g_canvas[g_cidx].selColIndex] = ImGui::ColorConvertFloat4ToU32(picker_color);
        }
    }

    ImGui::PopItemWidth();

    // Main 2 colors
    for (size_t i = g_canvas[g_cidx].myCols.size() - 2; i < g_canvas[g_cidx].myCols.size(); i++) {
        std::string id = "Color " + std::to_string(i + 1);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, g_canvas[g_cidx].selColIndex == i ? ImVec4(1.0f, 1.0f, 1.0f, 1.f) : ImVec4(0.05f, 0.05f, 0.05f, 1));

        if (ImGui::ColorButton(id.c_str(), ImGui::ColorConvertU32ToFloat4(g_canvas[g_cidx].myCols[i]), NULL, { 87, 21 }))
            g_canvas[g_cidx].selColIndex = (uint16_t)i;

        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::End();
}
