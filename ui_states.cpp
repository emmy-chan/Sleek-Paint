#include "data_manager.h"
#include "ui_states.h"
#include "canvas.h"

#include <string>
#include <fstream>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//cout
#include <iostream>
#include "stb_image.h"

//Nice little function for loading color data with alpha or no alpha
std::string GetColorData(const std::vector<ImU32>& colors, bool includeAlpha) {
    std::string data;
    for (uint16_t i = 0; i < colors.size(); i++) {
        const ImU32 color = colors[i];
        const int red = (color >> 0) & 0xFF;
        const int green = (color >> 8) & 0xFF;
        const int blue = (color >> 16) & 0xFF;
        const int alpha = (color >> 24) & 0xFF;

        data.append(std::to_string(red) + " ");
        data.append(std::to_string(green) + " ");
        data.append(std::to_string(blue) + " ");

        if (includeAlpha)
            data.append(std::to_string(alpha) + " ");
    }
    return data;
}

void SaveCanvasToImage(const char* name, const char* format) {
    int width = g_canvas[g_cidx].width;
    int height = g_canvas[g_cidx].height;

    // Allocate memory for RGBA data: width * height * 4 bytes (for R, G, B, and A)
    char* imageData = new char[width * height * 4];
    int imagePos = 0;

    // Place all white in the imageData to start
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            imageData[imagePos++] = 255; // Red
            imageData[imagePos++] = 255; // Green
            imageData[imagePos++] = 255; // Blue
            imageData[imagePos++] = 255; // Alpha
        }
    }

    // Blend all layers
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            float finalR = 1.0f, finalG = 1.0f, finalB = 1.0f, finalA = 1.0f; // Start with white background

            for (size_t layer = 0; layer < g_canvas[g_cidx].tiles.size(); layer++) {
                const ImU32 color = g_canvas[g_cidx].tiles[layer][i + j * width];

                // Extract RGBA components
                const float srcR = ((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                const float srcG = ((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                const float srcB = ((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                const float srcA = ((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;

                // Blend the source color onto the final color using alpha blending
                finalR = srcR * srcA + finalR * (1.0f - srcA);
                finalG = srcG * srcA + finalG * (1.0f - srcA);
                finalB = srcB * srcA + finalB * (1.0f - srcA);
                finalA = srcA + finalA * (1.0f - srcA);
            }

            // Convert final blended color back to integer format and store in imageData
            int idx = (i + j * width) * 4;
            imageData[idx] = static_cast<char>(finalR * 255);  // Red
            imageData[idx + 1] = static_cast<char>(finalG * 255);  // Green
            imageData[idx + 2] = static_cast<char>(finalB * 255);  // Blue
            imageData[idx + 3] = static_cast<char>(finalA * 255);  // Alpha
        }
    }

    // Save image based on the specified format
    bool success = false;
    if (strcmp(format, "png") == 0)
        success = stbi_write_png(name, width, height, 4, imageData, width * 4);
    else if (strcmp(format, "jpg") == 0) {
        // Convert RGBA to RGB by ignoring the alpha channel
        char* rgbData = new char[width * height * 3];
        for (int k = 0; k < width * height; k++) {
            rgbData[k * 3] = imageData[k * 4];
            rgbData[k * 3 + 1] = imageData[k * 4 + 1];
            rgbData[k * 3 + 2] = imageData[k * 4 + 2];
        }
        success = stbi_write_jpg(name, width, height, 3, rgbData, 100); // 100 is the quality parameter for JPG
        delete[] rgbData;
    }
    else if (strcmp(format, "bmp") == 0)
        success = stbi_write_bmp(name, width, height, 4, imageData);
    else if (strcmp(format, "tga") == 0)
        success = stbi_write_tga(name, width, height, 4, imageData);
    else
        std::cerr << "Unsupported format: " << format << std::endl;

    if (success)
        std::cout << "File saved successfully: " << name << std::endl;
    else
        std::cerr << "Failed to save file: " << name << std::endl;

    delete[] imageData;
}

void cUIStateSaveProject::Update()
{
    static bool bDisplayed = false;
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir);
    fileDialog.SetTitle("Save Project");

    if (!bDisplayed && !fileDialog.IsOpened()) fileDialog.Open();

    fileDialog.Display();

    static std::string file_name;
    static std::string extension;
    static std::string data;

    fileDialog.SetTypeFilters({ ".spr", ".jpg", ".png", ".bmp", ".tga" });
    if (fileDialog.GetTypeFilterIndex() == 0) fileDialog.SetCurrentTypeFilterIndex(3);

    if (fileDialog.HasSelected())
    {
        // Get our canvas and push it to our data string!
        if (fileDialog.GetTypeFilterIndex() == 1) data = GetColorData(g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex], true);
        file_name = fileDialog.GetSelected().string();

        // Get the file extension
        if (fileDialog.GetTypeFilterIndex() == 1 && file_name.find(".spr") == std::string::npos)
            extension = ".spr";
        else if (fileDialog.GetTypeFilterIndex() == 2 && file_name.find(".jpg") == std::string::npos)
            extension = ".jpg";
        else if (fileDialog.GetTypeFilterIndex() == 3 && file_name.find(".png") == std::string::npos)
            extension = ".png";
        else if (fileDialog.GetTypeFilterIndex() == 4 && file_name.find(".bmp") == std::string::npos)
            extension = ".bmp";
        else if (fileDialog.GetTypeFilterIndex() == 5 && file_name.find(".tga") == std::string::npos)
            extension = ".tga";

        file_name.append(extension);

        std::cout << "Saving file to: " << file_name << " with format: " << extension << std::endl;

        if (!std::filesystem::exists(file_name)) {
            if (fileDialog.GetTypeFilterIndex() == 1) {
                DataManager dm;
                dm.SaveDataToFile(file_name, data);
            }
            else
                SaveCanvasToImage(file_name.c_str(), extension.c_str() + 1);  // Skip the dot in the extension

            g_canvas[g_cidx].name = fileDialog.GetSelected().filename().string();
            g_app.ui_state.reset();
            bDisplayed = false;
            file_name.clear();
            data.clear();
        }
        else {
            ImGui::OpenPopup("Warning");
            bDisplayed = true;
        }

        fileDialog.ClearSelected();
    }

    if (bDisplayed) {
        if (ImGui::BeginPopupModal("Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            const std::string warning_txt = "File already exists, overwrite anyways?\n'" + g_canvas[g_cidx].name + "'";
            ImGui::Text(warning_txt.c_str());

            if (ImGui::Button("Yes")) {
                if (fileDialog.GetTypeFilterIndex() == 1) {
                    DataManager dm;
                    dm.SaveDataToFile(file_name, data);
                }
                else
                    SaveCanvasToImage(file_name.c_str(), extension.c_str() + 1);  // Skip the dot in the extension

                g_app.ui_state.reset();
                bDisplayed = false;
                file_name.clear();
                data.clear();
            }

            ImGui::SameLine();
            if (ImGui::Button("No"))
                bDisplayed = false;

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { 
                g_app.ui_state.reset();
                bDisplayed = false;
                file_name.clear();
                data.clear();
            }

            ImGui::EndPopup();
        }
    }
}

void cUIStateNewProject::Update()
{
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({ 145, 120 }, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({ io.DisplaySize.x / 2 - 88, io.DisplaySize.y / 2 - 50 }, ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("New Project", NULL, ImGuiWindowFlags_NoResize)) { //ImGuiWindowFlags_AlwaysAutoResize
        static int wInput = 32, hInput = 32;

        ImGui::Text("Width:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        ImGui::InputInt("##Width", &wInput);

        ImGui::Text("Height:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        ImGui::InputInt("##Height", &hInput);

        if (ImGui::Button("Ok")) {
            //Todo: just call our canvas initialize function and pass the width / height
            std::string scene_name = "Sprite " + std::to_string(g_canvas.size() + 1);
            uint16_t scene_idx = (uint16_t)g_canvas.size() + 2;

            for (auto& cnvs : g_canvas) {
                if (scene_name == cnvs.name) {
                    scene_name = cnvs.name;
                    scene_name.resize(7);
                    scene_name.append(std::to_string(scene_idx));
                    printf("Duplicate name detected!\n");
                }

                scene_idx++;
            }
            
            // Add our new canvas to our canvases and set our idx
            // To the newly created canvas!
            cCanvas canvas = cCanvas(scene_name, (int)wInput, (int)hInput);
            g_canvas.push_back(canvas);
            g_cidx = (uint16_t)g_canvas.size() - 1;

            // Create state for (undo) previous canvas of blank canvas
            g_canvas[g_cidx].UpdateCanvasHistory();

            // Reset our UI State
            g_app.ui_state.reset();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) g_app.ui_state.reset();
        ImGui::EndPopup();
    }
    else
        ImGui::OpenPopup("New Project");
}

void cUIStateCanvasSize::Update()
{
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({ 145, 120 }, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({ io.DisplaySize.x / 2 - 88, io.DisplaySize.y / 2 - 50 }, ImGuiCond_Appearing);

    if (ImGui::Begin("Resize Canvas", NULL, ImGuiWindowFlags_NoCollapse)) {
        static int wInput = 32;
        static int hInput = 32;

        ImGui::Text("Width:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        ImGui::InputInt("##Width", &wInput);

        ImGui::Text("Height:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        ImGui::InputInt("##Height", &hInput);
        //ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2 - 20);

        if (ImGui::Button("Ok")) {
            // Adapt canvas size
            g_canvas[g_cidx].AdaptNewSize((int)wInput, (int)hInput);

            // Reset our UI State
            g_app.ui_state.reset();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) g_app.ui_state.reset();
        ImGui::End();
    }
}

void cUIStateSaveWarning::Update()
{
    if (ImGui::BeginPopupModal("Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        const std::string warning_txt = "Save changes to '" + g_canvas[g_cidx].name + "' before closing?";

        ImGui::Text(warning_txt.c_str());

        if (ImGui::Button("Save"))
            g_app.ui_state = std::make_unique<cUIStateSaveProject>();

        ImGui::SameLine();
        if (ImGui::Button("Don't save")) {
            g_canvas[g_cidx].DestroyCanvas();
            g_app.ui_state.reset();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) g_app.ui_state.reset();

        ImGui::EndPopup();
    }
    else
        ImGui::OpenPopup("Warning");
}

void cUIStateLoadPalette::Update()
{
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir);
    fileDialog.SetTitle("Load Palette");
    fileDialog.SetTypeFilters({ ".pal" });

    if (!fileDialog.IsOpened()) fileDialog.Open();

    fileDialog.Display();

    if (fileDialog.HasSelected())
    {
        g_canvas[g_cidx].LoadColorPalette(fileDialog.GetSelected().string());
        g_app.ui_state.reset();

        //std::cout << "Selected filename " << fileDialog.GetSelected().string() << std::endl;
        fileDialog.ClearSelected();
    }
}

// Load from file
void LoadImageFileToCanvas(const std::string& filepath, const std::string& filename) {
    int image_width = 0;
    int image_height = 0;
    int channels = 0;
    unsigned char* image_data = stbi_load(filepath.c_str(), &image_width, &image_height, &channels, 0);
    if (image_data == NULL)
        return;

    std::vector<ImU32> image_layer;
    const size_t image_size = image_width * image_height;

    // Ensure we have 4 channels (RGBA)
    for (size_t i = 0; i < image_size; ++i) {
        const unsigned char r = image_data[i * channels];
        const unsigned char g = (channels > 1) ? image_data[i * channels + 1] : r;
        const unsigned char b = (channels > 2) ? image_data[i * channels + 2] : r;
        const unsigned char a = (channels > 3) ? image_data[i * channels + 3] : 255;
        image_layer.push_back(IM_COL32(r, g, b, a));
    }

    // Set our canvas dimensions based on the image
    cCanvas canvas = cCanvas(filename.c_str(), image_width, image_height, image_layer);
    g_canvas.push_back(canvas);
    g_cidx = (uint16_t)g_canvas.size() - 1;

    stbi_image_free(image_data);
}

void cUIStateOpenProject::Update()
{
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_NoStatusBar);
    fileDialog.SetTitle("Load Project");
    fileDialog.SetTypeFilters({ ".spr", ".jpg", ".png"});

    if (!fileDialog.IsOpened()) fileDialog.Open();

    fileDialog.Display();

    if (fileDialog.HasSelected())
    {
        g_app.ui_state.reset();

        LoadImageFileToCanvas(fileDialog.GetSelected().string(), fileDialog.GetSelected().filename().string());
        g_canvas[g_cidx].UpdateCanvasHistory();

        //std::cout << "Selected filename " << fileDialog.GetSelected().string() << std::endl;
        fileDialog.ClearSelected();
    }
}

void cUIStateSavePalette::Update()
{
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_NoStatusBar);
    fileDialog.SetTitle("Save Palette");
    fileDialog.SetTypeFilters({ ".pal" });

    if (!fileDialog.IsOpened()) fileDialog.Open();

    fileDialog.Display();

    if (fileDialog.HasSelected())
    {
        g_app.ui_state.reset();

        std::vector<ImU32> myCols = g_canvas[g_cidx].myCols;
        if (myCols.size() > 2) myCols.erase(myCols.begin(), myCols.begin() + 2);

        const std::string data = GetColorData(myCols, false);
        const std::string file_name = (fileDialog.GetSelected().string().find(".pal") == std::string::npos) ? fileDialog.GetSelected().string() + ".pal" : fileDialog.GetSelected().string();

        DataManager dm;
        dm.SaveDataToFile(file_name, data);

        //std::cout << "Selected filename " << file_name << std::endl;
        fileDialog.ClearSelected();
    }
}
