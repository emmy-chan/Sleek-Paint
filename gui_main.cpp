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

// Function to get the texture ID for the current selection
void* GetToolTexture(int tool) {
    switch (tool) {
        case 4: return (void*)g_assets.selection_texture;
        case TOOL_FREEFORM_SELECT: return (void*)g_assets.freeform_selection_texture;
        case TOOL_WAND: return (void*)g_assets.wand_texture;
        default: return g_assets.selection_texture; // No texture if no tool is selected
    }
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

            if (ImGui::MenuItem(ICON_FA_SIGN_OUT" Exit")) {
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
            if (ImGui::MenuItem(ICON_FA_COPY" Copy") && g_canvas.size() > 0)
                    g_canvas[g_cidx].CopySelection();
            if (ImGui::MenuItem(ICON_FA_PASTE" Paste") && g_canvas.size() > 0) {
                if (g_canvas[g_cidx].IsImageInClipboard())
                    g_canvas[g_cidx].PasteImageFromClipboard();
                else if (!copiedTiles.empty())
                    g_canvas[g_cidx].PasteSelection(); // Default paste functionality for non-image data
            }
            
            if (ImGui::MenuItem(ICON_FA_EXCHANGE " Convert Canvas Colors To Palette") && g_canvas.size() > 0) {
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
                const std::vector<ImU32> main_cols = { IM_COL32(0, 0, 0, 255), IM_COL32(255, 255, 255, 255) };

                // Push our two main colors to the array
                for (auto& c : main_cols)
                    g_canvas[g_cidx].myCols.push_back(c);

                // Create a set to store unique colors
                std::unordered_set<ImU32> uniqueColors;

                // Iterate over the canvas to collect unique colors
                for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                    for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                        const uint64_t index = x + y * g_canvas[g_cidx].width;
                        const ImU32 currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                        // Check the alpha value
                        if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0)
                            uniqueColors.insert(currentColor);
                    }
                }

                // Push unique colors to the myCols vector
                for (const auto& color : uniqueColors)
                    g_canvas[g_cidx].myCols.push_back(color);

                // Make sure colors are not duplicates, starting from the third element
                for (int i = 2; i < g_canvas[g_cidx].myCols.size() - 1; i++) {
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

                    if (duplicateFound)
                        g_canvas[g_cidx].myCols.erase(g_canvas[g_cidx].myCols.begin() + duplicateIndex);
                }
            }


            if (ImGui::MenuItem(ICON_FA_ADJUST " Apply Dithering") && g_canvas.size() > 0) {
                // Apply the changes to the canvas
                for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                    for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                        const uint64_t index = x + y * g_canvas[g_cidx].width;
                        ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
                        
                        if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0)
                            currentColor = g_util.ApplyFloydSteinbergDithering(currentColor, x, y);
                    }
                }

                g_canvas[g_cidx].UpdateCanvasHistory();
            }

            if (ImGui::MenuItem(ICON_FA_TH " Apply Selection Pixelate") && g_canvas.size() > 0) {
                // Define the pixelation block size
                const uint8_t blockSize = 8;

                // Apply pixelation to the selected blocks
                for (const auto& index : selectedIndexes) {
                    const uint64_t x = index % g_canvas[g_cidx].width;
                    const uint64_t y = index / g_canvas[g_cidx].width;
                    const uint64_t blockX = x / blockSize * blockSize;
                    const uint64_t blockY = y / blockSize * blockSize;

                    // Compute the average color of the block
                    uint64_t redSum = 0, greenSum = 0, blueSum = 0, alphaSum = 0;
                    uint64_t pixelCount = 0;

                    for (uint64_t by = 0; by < blockSize && (blockY + by) < g_canvas[g_cidx].height; ++by) {
                        for (uint64_t bx = 0; bx < blockSize && (blockX + bx) < g_canvas[g_cidx].width; ++bx) {
                            const uint64_t blockIndex = (blockX + bx) + (blockY + by) * g_canvas[g_cidx].width;
                            if (selectedIndexes.find(blockIndex) != selectedIndexes.end()) {
                                const ImU32 color = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][blockIndex];
                                redSum += (color >> IM_COL32_R_SHIFT) & 0xFF;
                                greenSum += (color >> IM_COL32_G_SHIFT) & 0xFF;
                                blueSum += (color >> IM_COL32_B_SHIFT) & 0xFF;
                                alphaSum += (color >> IM_COL32_A_SHIFT) & 0xFF;
                                ++pixelCount;
                            }
                        }
                    }

                    if (pixelCount > 0) {
                        const ImU32 avgColor = IM_COL32(
                            redSum / pixelCount,
                            greenSum / pixelCount,
                            blueSum / pixelCount,
                            alphaSum / pixelCount
                        );

                        // Assign the average color to the entire block
                        for (uint64_t by = 0; by < blockSize && (blockY + by) < g_canvas[g_cidx].height; ++by) {
                            for (uint64_t bx = 0; bx < blockSize && (blockX + bx) < g_canvas[g_cidx].width; ++bx) {
                                const uint64_t blockIndex = (blockX + bx) + (blockY + by) * g_canvas[g_cidx].width;
                                if (selectedIndexes.find(blockIndex) != selectedIndexes.end())
                                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][blockIndex] = avgColor;
                            }
                        }
                    }
                }

                g_canvas[g_cidx].UpdateCanvasHistory();
            }

            // Function to center horizontally
            if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT " Center Selection Horizontal") && g_canvas.size() > 0) {
                if (!selectedIndexes.empty()) {
                    int minX = g_canvas[g_cidx].width, maxX = 0;

                    // Find the horizontal bounding box of the non-transparent tiles
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            if (selectedIndexes.find(x + y * g_canvas[g_cidx].width) == selectedIndexes.end())
                                continue;

                            const uint64_t index = x + y * g_canvas[g_cidx].width;
                            const ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                            if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
                                if (x < minX) minX = x; if (x > maxX) maxX = x;
                            }
                        }
                    }

                    // Calculate the horizontal center position
                    const int boundingBoxWidth = maxX - minX + 1;
                    const int canvasCenterX = g_canvas[g_cidx].width / 2;
                    const int offsetX = canvasCenterX - boundingBoxWidth / 2 - minX;

                    // Create a copy of the current tiles
                    std::vector<ImU32> newTiles = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex];
                    std::unordered_set<uint64_t> newSelectedIndexes;

                    // Clear the area within the bounding box
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = minX; x <= maxX; x++) {
                            const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                            if (selectedIndexes.find(oldIndex) != selectedIndexes.end())
                                newTiles[oldIndex] = 0;
                        }
                    }

                    // Move the non-transparent tiles to the new horizontally centered positions
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            if (selectedIndexes.find(x + y * g_canvas[g_cidx].width) == selectedIndexes.end())
                                continue;

                            const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                            const ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][oldIndex];

                            if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
                                const int newX = x + offsetX;

                                if (newX >= 0 && newX < g_canvas[g_cidx].width) {
                                    const uint64_t newIndex = newX + y * g_canvas[g_cidx].width;
                                    newTiles[newIndex] = currentColor;
                                    newSelectedIndexes.insert(newIndex);
                                }
                            }
                        }
                    }

                    // Copy the new tiles back to the canvas
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = std::move(newTiles);

                    // Update the selectedIndexes
                    selectedIndexes.swap(newSelectedIndexes);

                    g_canvas[g_cidx].UpdateCanvasHistory();
                }
            }

            // Function to center vertically
            if (ImGui::MenuItem(ICON_FA_ARROW_UP " Center Selection Vertical") && g_canvas.size() > 0) {
                if (!selectedIndexes.empty()) {
                    int minY = g_canvas[g_cidx].height, maxY = 0;

                    // Find the vertical bounding box of the non-transparent tiles
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            if (selectedIndexes.find(x + y * g_canvas[g_cidx].width) == selectedIndexes.end())
                                continue;

                            const uint64_t index = x + y * g_canvas[g_cidx].width;
                            const ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                            if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
                                if (y < minY) minY = y;
                                if (y > maxY) maxY = y;
                            }
                        }
                    }

                    // Calculate the vertical center position
                    const int boundingBoxHeight = maxY - minY + 1;
                    const int canvasCenterY = g_canvas[g_cidx].height / 2;
                    const int offsetY = canvasCenterY - boundingBoxHeight / 2 - minY;

                    // Create a copy of the current tiles
                    std::vector<ImU32> newTiles = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex];
                    std::unordered_set<uint64_t> newSelectedIndexes;

                    // Clear the area within the bounding box
                    for (uint64_t y = minY; y <= maxY; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                            if (selectedIndexes.find(oldIndex) != selectedIndexes.end())
                                newTiles[oldIndex] = 0;
                        }
                    }

                    // Move the non-transparent tiles to the new vertically centered positions
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            if (selectedIndexes.find(x + y * g_canvas[g_cidx].width) == selectedIndexes.end())
                                continue;

                            const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                            const ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][oldIndex];

                            if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
                                const int newY = y + offsetY;

                                if (newY >= 0 && newY < g_canvas[g_cidx].height) {
                                    const uint64_t newIndex = x + newY * g_canvas[g_cidx].width;
                                    newTiles[newIndex] = currentColor;
                                    newSelectedIndexes.insert(newIndex);
                                }
                            }
                        }
                    }

                    // Copy the new tiles back to the canvas
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = std::move(newTiles);

                    // Update the selectedIndexes
                    selectedIndexes.swap(newSelectedIndexes);

                    g_canvas[g_cidx].UpdateCanvasHistory();
                }
            }

            // Function to flip horizontally
            if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT " Flip Selection Horizontal") && g_canvas.size() > 0) {
                if (!selectedIndexes.empty()) {
                    int minX = g_canvas[g_cidx].width, maxX = 0;

                    // Find the horizontal bounding box of the non-transparent tiles
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            if (selectedIndexes.find(x + y * g_canvas[g_cidx].width) == selectedIndexes.end())
                                continue;

                            const uint64_t index = x + y * g_canvas[g_cidx].width;
                            const ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                            if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
                                if (x < minX) minX = x;
                                if (x > maxX) maxX = x;
                            }
                        }
                    }

                    // Create a copy of the current tiles
                    std::vector<ImU32> newTiles = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex];
                    std::unordered_set<uint64_t> newSelectedIndexes;

                    // Clear the area within the bounding box
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = minX; x <= maxX; x++) {
                            const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                            if (selectedIndexes.find(oldIndex) != selectedIndexes.end())
                                newTiles[oldIndex] = 0;
                        }
                    }

                    // Flip the non-transparent tiles to the new horizontally flipped positions
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = minX; x <= maxX; x++) {
                            const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                            if (selectedIndexes.find(oldIndex) == selectedIndexes.end())
                                continue;

                            const ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][oldIndex];

                            if (((currentColor >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
                                const int newX = maxX - (x - minX);

                                if (newX >= 0 && newX < g_canvas[g_cidx].width) {
                                    const uint64_t newIndex = newX + y * g_canvas[g_cidx].width;
                                    newTiles[newIndex] = currentColor;
                                    newSelectedIndexes.insert(newIndex);
                                }
                            }
                        }
                    }

                    // Copy the new tiles back to the canvas
                    g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex] = std::move(newTiles);

                    // Update the selectedIndexes
                    selectedIndexes.swap(newSelectedIndexes);

                    g_canvas[g_cidx].UpdateCanvasHistory();
                }
            }

            if (ImGui::MenuItem(ICON_FA_REDO " Rotate 90 Degrees") && g_canvas.size() > 0) {
                // Create a new canvas to store the rotated image
                const uint64_t newWidth = g_canvas[g_cidx].height;
                const uint64_t newHeight = g_canvas[g_cidx].width;
                std::vector<std::vector<ImU32>> newTiles(g_canvas[g_cidx].tiles.size(), std::vector<ImU32>(newWidth * newHeight));

                for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                    for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                        const uint64_t oldIndex = x + y * g_canvas[g_cidx].width;
                        const uint64_t newX = newHeight - 1 - y;
                        const uint64_t newY = x;
                        const uint64_t newIndex = newX + newY * newWidth;

                        for (size_t layer = 0; layer < g_canvas[g_cidx].tiles.size(); ++layer)
                            newTiles[layer][newIndex] = g_canvas[g_cidx].tiles[layer][oldIndex];
                    }
                }

                // Replace the old tiles with the new rotated tiles
                g_canvas[g_cidx].width = newWidth;
                g_canvas[g_cidx].height = newHeight;
                g_canvas[g_cidx].tiles = std::move(newTiles);

                g_canvas[g_cidx].UpdateCanvasHistory();
            }

            if (ImGui::CollapsingHeader("Scrambler")) {
                // Scramble Key / Seed
                static ImU32 key = 0xA5A5A5A5;
                static uint64_t seed = 12345;

                // Encrypt and Decrypt buttons
                if (ImGui::MenuItem(ICON_FA_LOCK " Scramble") && g_canvas.size() > 0) {
                    const uint64_t size = g_canvas[g_cidx].width * g_canvas[g_cidx].height;
                    const std::vector<uint64_t> permutation = g_util.GeneratePermutation(size, seed);

                    // Encrypt the colors on the canvas
                    std::vector<ImU32> tempColors(size);
                    for (uint64_t i = 0; i < size; i++) {
                        const uint64_t permutedIndex = permutation[i];

                        // Encrypt the color with XOR
                        const ImU32 currentColor = g_util.XorColor(g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][i], key);
                        tempColors[permutedIndex] = currentColor;
                    }
                    // Copy the encrypted colors back to the canvas
                    for (uint64_t i = 0; i < size; i++)
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][i] = tempColors[i];

                    g_canvas[g_cidx].UpdateCanvasHistory();
                }

                if (ImGui::MenuItem(ICON_FA_UNLOCK " Descramble") && g_canvas.size() > 0) {
                    const uint64_t size = g_canvas[g_cidx].width * g_canvas[g_cidx].height;
                    const std::vector<uint64_t> permutation = g_util.GeneratePermutation(size, seed);
                    std::vector<uint64_t> inversePermutation(size);

                    // Create the inverse permutation
                    for (uint64_t i = 0; i < size; i++)
                        inversePermutation[permutation[i]] = i;

                    // Decrypt the colors on the canvas
                    std::vector<ImU32> tempColors(size);
                    for (uint64_t i = 0; i < size; i++) {
                        const uint64_t originalIndex = inversePermutation[i];
                        const ImU32 encryptedColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][i];

                        // Decrypt the color with XOR
                        const ImU32 decryptedColor = g_util.XorColor(encryptedColor, key);

                        tempColors[originalIndex] = decryptedColor;
                    }

                    // Copy the decrypted colors back to the canvas
                    for (uint64_t i = 0; i < size; i++)
                        g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][i] = tempColors[i];

                    g_canvas[g_cidx].UpdateCanvasHistory();
                }

                // Input fields for key and seed
                ImGui::InputScalar("Key", ImGuiDataType_U32, &key, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::InputScalar("Seed", ImGuiDataType_U64, &seed, nullptr, nullptr, "%016llX", ImGuiInputTextFlags_CharsHexadecimal);

                // Button to generate random key and seed
                if (ImGui::Button(ICON_FA_KEY " Generate Random Key & Seed"))
                    g_util.GenerateRandomKeyAndSeed(key, seed);
            }

            if (ImGui::CollapsingHeader("Color Adjustments") && g_canvas.size() > 0) {
                static float saturationFactor = 1.0f, contrastFactor = 1.0f;
                bool changed = false;

                // Store the original colors if not already stored
                static std::vector<ImU32> originalColors;
                if (originalColors.empty() && g_canvas.size() > 0) {
                    originalColors.resize(g_canvas[g_cidx].width * g_canvas[g_cidx].height);
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            const uint64_t index = x + y * g_canvas[g_cidx].width;
                            originalColors[index] = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];
                        }
                    }
                }

                // Saturation slider
                if (ImGui::SliderFloat("Saturation", &saturationFactor, 0.0f, 2.0f, "Factor: %.2f"))
                    changed = true;

                // Contrast slider
                if (ImGui::SliderFloat("Contrast", &contrastFactor, 0.0f, 2.0f, "Factor: %.2f"))
                    changed = true;

                if (changed) {
                    // Apply the changes to the canvas based on the original colors
                    for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
                        for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                            const uint64_t index = x + y * g_canvas[g_cidx].width;
                            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index] = 
                                g_util.AdjustSaturation(g_util.AdjustContrast(originalColors[index], contrastFactor), saturationFactor);
                        }
                    }
                }
                else
                    saturationFactor = 1.0f, contrastFactor = 1.0f;

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

            ImGui::EndTabBar();
        }

        ImGui::EndMainMenuBar();

        // Dont show any tools if we dont have any projects open.
        if (g_canvas.empty()) return;

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
        ImGui::SetWindowSize({ ImGui::GetWindowWidth() - 20, 20});
        ImGui::PushItemWidth(ImGui::GetWindowWidth());
        ImGui::SliderFloat("##ScrollHorizontal", &g_cam.x, glm::abs((g_canvas[g_cidx].width * (TILE_SIZE * 1.685f))), -glm::abs((g_canvas[g_cidx].width * TILE_SIZE) / 2), "");
        ImGui::End();

        ImGui::SetNextWindowPos({ io.DisplaySize.x - 60, 24 });
        ImGui::SetNextWindowSize({ 20, io.DisplaySize.y - 43 });
        ImGui::Begin("##ScrollV", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::VSliderFloat("##ScrollVertical", { 20, ImGui::GetWindowHeight() }, &g_cam.y, -glm::abs((g_canvas[g_cidx].height * TILE_SIZE)), glm::abs(((g_canvas[g_cidx].height * TILE_SIZE)) + 125), "");
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
        
        ImGui::Begin("##Tools", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 4.5f, 4.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.7f, 1.f });

        // Determine the texture based on the current tool selection
        void* buttonTexture = GetToolTexture(paintToolSelected);

        ImGui::PushStyleColor(ImGuiCol_Button,
            paintToolSelected == 4 ||
            paintToolSelected == TOOL_FREEFORM_SELECT ||
            paintToolSelected == 5 ? panelActiveColor : panelColor);

        // Check if the texture is valid before using it
        if (buttonTexture) {
            // Create a button with the current tool texture, resized to 16x16
            if (ImGui::ImageButton(buttonTexture, ImVec2(16, 16)))
                ImGui::OpenPopup("Select Tool Popup"); // Open the popup when the button is clicked
        }
        else {
            // Render a placeholder button or handle the case when no texture is selected
            if (ImGui::ImageButton(g_assets.selection_texture, ImVec2(16, 16)))
                ImGui::OpenPopup("Select Tool Popup"); // Open the popup when the button is clicked
        }
        ImGui::PopStyleColor();

        // Create the popup for all selection tools
        if (ImGui::BeginPopup("Select Tool Popup")) {
            // Selection Tool Button
            ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 4 ? panelActiveColor : panelColor);
            if (ImGui::ImageButton((void*)g_assets.selection_texture, ImVec2(16, 16))) {
                paintToolSelected = 4;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Selection Tool");
            ImGui::PopStyleColor();

            // Free Form Selection Tool Button
            ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_FREEFORM_SELECT ? panelActiveColor : panelColor);
            if (ImGui::ImageButton((void*)g_assets.freeform_selection_texture, ImVec2(16, 16))) {
                paintToolSelected = TOOL_FREEFORM_SELECT;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free Form Selection Tool");
            ImGui::PopStyleColor();

            // Magic Wand Tool Button
            ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 5 ? panelActiveColor : panelColor);
            if (ImGui::ImageButton((void*)g_assets.wand_texture, ImVec2(16, 16))) {
                paintToolSelected = 5;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Magic Wand Tool");
            ImGui::PopStyleColor();

            ImGui::EndPopup();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 6 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_MOUSE_POINTER, ImVec2(25, 25))) paintToolSelected = 6;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pointer Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 0 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_PENCIL, ImVec2(25, 25))) paintToolSelected = 0;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pencil Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 1 ? panelActiveColor : panelColor);
        if (ImGui::ImageButton((void*)g_assets.bucket_texture, ImVec2(16, 16))) paintToolSelected = 1;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bucket Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 7 ? panelActiveColor : panelColor);
        if (ImGui::Button("/", ImVec2(25, 25))) paintToolSelected = 7;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_TEXT ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_FONT, ImVec2(25, 25))) paintToolSelected = TOOL_TEXT;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Text Tool");

        // Square and Ellipse Tool with Popup
        void* shapeIcon = nullptr;

        if (paintToolSelected == TOOL_SQUARE)
            shapeIcon = (void*)ICON_FA_SQUARE;  // Show square icon when square tool is selected
        else if (paintToolSelected == TOOL_ELIPSE)
            shapeIcon = (void*)ICON_FA_CIRCLE;  // Show ellipse icon when ellipse tool is selected
        else
            shapeIcon = (void*)ICON_FA_SQUARE;  // Show rectangle (square) icon by default when no tool is selected

        ImGui::PushStyleColor(ImGuiCol_Button,
            (paintToolSelected == TOOL_SQUARE || paintToolSelected == TOOL_ELIPSE) ? panelActiveColor : panelColor);

        // Display the current shape icon
        if (ImGui::Button((const char*)shapeIcon, ImVec2(25, 25)))
            ImGui::OpenPopup("Shape Tool Popup");

        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shape Tools");
        ImGui::PopStyleColor();

        if (ImGui::BeginPopup("Shape Tool Popup")) {
            // Square Tool Button
            ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_SQUARE ? panelActiveColor : panelColor);
            if (ImGui::Button(ICON_FA_SQUARE, ImVec2(25, 25))) {
                paintToolSelected = TOOL_SQUARE;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Square Tool");
            ImGui::PopStyleColor();

            // Ellipse Tool Button
            ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_ELIPSE ? panelActiveColor : panelColor);
            if (ImGui::Button(ICON_FA_CIRCLE, ImVec2(25, 25))) {
                paintToolSelected = TOOL_ELIPSE;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ellipse Tool");
            ImGui::PopStyleColor();

            ImGui::EndPopup();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 2 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_ERASER, ImVec2(25, 25))) paintToolSelected = 2;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Eraser Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == 3 ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_EYE_DROPPER, ImVec2(25, 25))) paintToolSelected = 3;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Eyedropper Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_BANDAID ? panelActiveColor : panelColor);
        if (ImGui::ImageButton((void*)g_assets.bandaid_texture, ImVec2(16, 16))) paintToolSelected = TOOL_BANDAID;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Band-Aid Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_PAN ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_HAND_POINTER, ImVec2(25, 25))) paintToolSelected = TOOL_PAN;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pan Tool");

        ImGui::PushStyleColor(ImGuiCol_Button, paintToolSelected == TOOL_ZOOM ? panelActiveColor : panelColor);
        if (ImGui::Button(ICON_FA_SEARCH, ImVec2(25, 25))) paintToolSelected = TOOL_ZOOM;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom Tool");

        ImGui::PopStyleColor(10);
        ImGui::PopStyleVar();
        ImGui::End();
    }

    auto& draw = *ImGui::GetBackgroundDrawList();
    draw.AddRectFilled({ io.DisplaySize.x - 60, io.DisplaySize.y - 19 }, { io.DisplaySize.x - 40, io.DisplaySize.y }, ImColor(51, 51, 55, 255));

    ImGui::SetNextWindowSize({ 197, io.DisplaySize.y - 300 });
    ImGui::SetNextWindowPos({ 0, 23 });

    ImGui::Begin("##Colors", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });

    // Child window for color buttons with scrollbar
    ImGui::BeginChild("##ColorButtons", ImVec2(0, io.DisplaySize.y * 0.25f), false); // Enable scrollbar

    // Calculate the number of buttons that can fit in one row
    const float buttonSize = 20.0f;
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalWidth = ImGui::GetContentRegionAvail().x;
    const int buttonsPerRow = static_cast<int>(totalWidth / (buttonSize + itemSpacing));

    for (uint16_t i = 2; i < g_canvas[g_cidx].myCols.size(); i++) {
        const std::string id = "Color " + std::to_string(i + 1);

        // Start a new row if necessary
        if ((i - 2) % buttonsPerRow != 0)
            ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, g_canvas[g_cidx].selColIndex == i ? ImVec4(1.0f, 1.0f, 1.0f, 1.f) : ImVec4(0.05f, 0.05f, 0.05f, 1));

        if (ImGui::ColorButton(id.c_str(), ImGui::ColorConvertU32ToFloat4(g_canvas[g_cidx].myCols[i]), NULL, { buttonSize, buttonSize }))
            g_canvas[g_cidx].selColIndex = i;

        ImGui::PopStyleColor();
    }

    ImGui::EndChild();

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.475f);
    ImGui::PopStyleVar(2); ImGui::Spacing(); ImGui::Separator();

    // Layer UI
    for (size_t i = 0; i < g_canvas[g_cidx].tiles.size(); i++) {
        const bool isSelected = (g_canvas[g_cidx].selLayerIndex == i);

        // Set the text color based on the selection state
        if (isSelected)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // Full brightness for selected
        else
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Dimmed brightness for non-selected

        if (ImGui::Selectable(g_canvas[g_cidx].layerNames[i].c_str(), isSelected, 0, ImVec2(79, 0)))
            g_canvas[g_cidx].selLayerIndex = i;

        ImGui::SameLine(ImGui::GetContentRegionMax().x - 72);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);

        std::string name = std::string(ICON_FA_ARROW_UP) + "##" + std::to_string(i);

        if (ImGui::Button(name.c_str(), ImVec2(22, 22))) {
            if (i > 0) {
                std::swap(g_canvas[g_cidx].tiles[i], g_canvas[g_cidx].tiles[i - 1]);
                std::swap(g_canvas[g_cidx].layerVisibility[i], g_canvas[g_cidx].layerVisibility[i - 1]);
                std::swap(g_canvas[g_cidx].layerNames[i], g_canvas[g_cidx].layerNames[i - 1]);
                std::swap(g_canvas[g_cidx].layerOpacity[i], g_canvas[g_cidx].layerOpacity[i - 1]);

                if (g_canvas[g_cidx].selLayerIndex == i) {
                    g_canvas[g_cidx].selLayerIndex = i - 1;
                }
                else if (g_canvas[g_cidx].selLayerIndex == i - 1)
                    g_canvas[g_cidx].selLayerIndex = i;
            }
        }

        // Arrow down button to move layer down
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 48);
        name = std::string(ICON_FA_ARROW_DOWN) + "##" + std::to_string(i);

        if (ImGui::Button(name.c_str(), ImVec2(22, 22))) {
            if (i < g_canvas[g_cidx].tiles.size() - 1) {
                std::swap(g_canvas[g_cidx].tiles[i], g_canvas[g_cidx].tiles[i + 1]);
                std::swap(g_canvas[g_cidx].layerVisibility[i], g_canvas[g_cidx].layerVisibility[i + 1]);
                std::swap(g_canvas[g_cidx].layerNames[i], g_canvas[g_cidx].layerNames[i + 1]);
                std::swap(g_canvas[g_cidx].layerOpacity[i], g_canvas[g_cidx].layerOpacity[i + 1]);

                if (g_canvas[g_cidx].selLayerIndex == i) {
                    g_canvas[g_cidx].selLayerIndex = i + 1;
                }
                else if (g_canvas[g_cidx].selLayerIndex == i + 1)
                    g_canvas[g_cidx].selLayerIndex = i;
            }
        }

        // Add the eye button to toggle visibility, moving it to the right
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 24);
        const char* eyeIcon = g_canvas[g_cidx].layerVisibility[i] ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
        const std::string label = std::string(eyeIcon) + "##" + std::to_string(i); // Append unique identifier

        if (ImGui::Button(label.c_str(), { 24, 22 }))
            g_canvas[g_cidx].layerVisibility[i] = !g_canvas[g_cidx].layerVisibility[i];

        // Add Rename button
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 96);
        name = std::string(ICON_FA_EDIT) + "##" + std::to_string(i);
        if (ImGui::Button(name.c_str(), { 22, 22 }))
            g_app.ui_state = std::make_unique<cUIStateRenameLayer>();

        ImGui::PopStyleColor();
    }

    int tempVal = g_canvas[g_cidx].layerOpacity[g_canvas[g_cidx].selLayerIndex] * 100 / 255;
    if (ImGui::SliderInt("Opacity", &tempVal, 0, 100)) {
        // Scale the slider value from 0-100 to 0-255
        const int16_t scaledVal = tempVal * 255 / 100;

        // Calculate the delta in the 0-255 range
        const int16_t delta = scaledVal - g_canvas[g_cidx].layerOpacity[g_canvas[g_cidx].selLayerIndex];

        // Update the layer's opacity with the scaled value
        g_canvas[g_cidx].layerOpacity[g_canvas[g_cidx].selLayerIndex] = scaledVal;

        for (uint64_t y = 0; y < g_canvas[g_cidx].height; y++) {
            for (uint64_t x = 0; x < g_canvas[g_cidx].width; x++) {
                const uint64_t index = x + y * g_canvas[g_cidx].width;
                ImU32& currentColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

                // Extract the alpha value
                int16_t alpha = (currentColor >> IM_COL32_A_SHIFT) & 0xFF;

                // Modify the alpha value based on the delta
                alpha = std::clamp(alpha + delta, 0, 255);

                // Reconstruct the color with the new alpha value
                currentColor = (currentColor & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
            }
        }
    }

    if (ImGui::Button("Add Layer", { ImGui::GetColumnWidth(), ImGui::GetFrameHeight() }))
        g_canvas[g_cidx].NewLayer();

    if (ImGui::Button("Remove Layer", { ImGui::GetColumnWidth(), ImGui::GetFrameHeight() })) {
        if (g_canvas[g_cidx].tiles.size() > 1) {
            g_canvas[g_cidx].tiles.erase(g_canvas[g_cidx].tiles.begin() + g_canvas[g_cidx].selLayerIndex);
            g_canvas[g_cidx].layerVisibility.erase(g_canvas[g_cidx].layerVisibility.begin() + g_canvas[g_cidx].selLayerIndex);
            g_canvas[g_cidx].layerNames.erase(g_canvas[g_cidx].layerNames.begin() + g_canvas[g_cidx].selLayerIndex);
            g_canvas[g_cidx].layerOpacity.erase(g_canvas[g_cidx].layerOpacity.begin() + g_canvas[g_cidx].selLayerIndex);
            if (g_canvas[g_cidx].selLayerIndex > 0) g_canvas[g_cidx].selLayerIndex--;
        }
    }

    ImGui::Spacing(); ImGui::Separator();
    if (paintToolSelected == TOOL_BRUSH || paintToolSelected == TOOL_ERASER || paintToolSelected == TOOL_BANDAID) {
        ImGui::Text("Brush Size:");
        int temp_brush_size = static_cast<int>(brush_size);

        if (ImGui::SliderInt("##Brush Size", &temp_brush_size, 1, 10))
            brush_size = static_cast<uint8_t>(temp_brush_size);
    }
    else if (paintToolSelected == TOOL_BUCKET) {
        ImGui::Text("Bucket Threshold:");
        int temp = static_cast<int>(bucket_fill_threshold);

        if (ImGui::SliderInt("##Bucket Threshold", &temp, 1, 100))
            bucket_fill_threshold = static_cast<uint8_t>(temp);
    }
    else if (paintToolSelected == TOOL_WAND) {
        ImGui::Text("Wand Threshold:");
        int temp = static_cast<int>(magic_wand_threshold);

        if (ImGui::SliderInt("##Wand Threshold", &temp, 1, 100))
            magic_wand_threshold = static_cast<uint8_t>(temp);
    }
    else if (paintToolSelected == TOOL_LINE) {
        ImGui::Text("Line Size:");
        int temp = static_cast<int>(line_size);

        if (ImGui::SliderInt("##Line Size", &temp, 1, 10))
            line_size = static_cast<uint8_t>(temp);
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
    for (size_t i = 0; i < 2; i++) {
        const std::string id = "Color " + std::to_string(i + 1);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, g_canvas[g_cidx].selColIndex == i ? ImVec4(1.0f, 1.0f, 1.0f, 1.f) : ImVec4(0.05f, 0.05f, 0.05f, 1));

        if (ImGui::ColorButton(id.c_str(), ImGui::ColorConvertU32ToFloat4(g_canvas[g_cidx].myCols[i]), NULL, { 87, 21 }))
            g_canvas[g_cidx].selColIndex = (uint16_t)i;

        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::End();
}
