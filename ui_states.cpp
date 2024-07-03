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
        ImU32 color = colors[i];
        int red = (color >> 0) & 0xFF;
        int green = (color >> 8) & 0xFF;
        int blue = (color >> 16) & 0xFF;
        int alpha = (color >> 24) & 0xFF;

        data.append(std::to_string(red) + " ");
        data.append(std::to_string(green) + " ");
        data.append(std::to_string(blue) + " ");

        if (includeAlpha) {
            data.append(std::to_string(alpha) + " ");
        }
    }
    return data;
}

void SaveCanvasToPng(const char* name) {
    // Allocate memory for RGBA data: width * height * 4 bytes (for R, G, B, and A)
    char* imageData = new char[g_canvas[g_cidx].width * g_canvas[g_cidx].height * 4];
    int imagePos = 0;

    // Blend all layers
    for (int j = 0; j < g_canvas[g_cidx].height; j++) {
        for (int i = 0; i < g_canvas[g_cidx].width; i++) {
            ImU32 finalColor = IM_COL32(0, 0, 0, 0); // Start with transparent black
            for (size_t layer = 0; layer < g_canvas[g_cidx].tiles.size(); layer++) {
                const ImU32 color = g_canvas[g_cidx].tiles[layer][i + j * g_canvas[g_cidx].width];

                // Extract RGBA components
                const int srcR = (color >> 0) & 0xFF;
                const int srcG = (color >> 8) & 0xFF;
                const int srcB = (color >> 16) & 0xFF;
                const int srcA = (color >> 24) & 0xFF;

                const int dstR = (finalColor >> 0) & 0xFF;
                const int dstG = (finalColor >> 8) & 0xFF;
                const int dstB = (finalColor >> 16) & 0xFF;
                const int dstA = (finalColor >> 24) & 0xFF;

                // Blend the source color onto the destination color using alpha blending
                const float alpha = srcA / 255.0f;
                const int outR = static_cast<int>(srcR * alpha + dstR * (1 - alpha));
                const int outG = static_cast<int>(srcG * alpha + dstG * (1 - alpha));
                const int outB = static_cast<int>(srcB * alpha + dstB * (1 - alpha));
                const int outA = static_cast<int>(srcA + dstA * (1 - alpha));

                finalColor = IM_COL32(outR, outG, outB, outA);
            }

            // Store the final blended color into imageData
            imageData[imagePos++] = (finalColor >> 0) & 0xFF;  // Red
            imageData[imagePos++] = (finalColor >> 8) & 0xFF;  // Green
            imageData[imagePos++] = (finalColor >> 16) & 0xFF; // Blue
            imageData[imagePos++] = (finalColor >> 24) & 0xFF; // Alpha
        }
    }

    // The '4' argument specifies 4 bytes per pixel (RGBA)
    stbi_write_png(name, g_canvas[g_cidx].width, g_canvas[g_cidx].height, 4, imageData, g_canvas[g_cidx].width * 4);
    delete[] imageData;
}

void cUIStateSaveProject::Update()
{
    //if (ImGui::Begin("Save Project", NULL, ImGuiWindowFlags_NoCollapse)) {
    //    static char file_name[MAX_PATH + 1];
    //    ImGui::Text("File");
    //    ImGui::PushItemWidth(176);
    //    ImGui::InputText("##ss", file_name, IM_ARRAYSIZE(file_name));
    //    ImGui::PopItemWidth();
    //    //ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2 - 20);

    //    if (ImGui::Button("Save")) {
    //        //Do save stuff here!
    //        g_app.ui_state.reset();
    //    }

    //    ImGui::SameLine();
    //    if (ImGui::Button("Cancel")) g_app.ui_state.reset();

    //    ImGui::End();
    //}

    static bool bDisplayed = false;
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir);
    fileDialog.SetTitle("Save Project");
    //fileDialog.SetTypeFilters({ ".pal" });

    if (!bDisplayed && !fileDialog.IsOpened())
        fileDialog.Open();

    fileDialog.Display();

    static std::string file_name;
    static std::string data;

    fileDialog.SetTypeFilters({ ".spr", ".png" });
    fileDialog.SetCurrentTypeFilterIndex(2);

    if (fileDialog.HasSelected())
    {
        //Get our canvas and push it to our data string!
        //GetColorData caused freezing one time... very strange... maybe issues with width height variables?
        if (fileDialog.GetTypeFilterIndex() == 1) data = GetColorData(g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex], true);
        file_name = fileDialog.GetSelected().string();

        if (!fileDialog.GetTypeFilterIndex() == 1 && !file_name.find(".spr") != std::string::npos)
            file_name.append(".spr");
        else if (fileDialog.GetTypeFilterIndex() == 2 && !file_name.find(".png") != std::string::npos)
            file_name.append(".png");

        if (!std::filesystem::exists(file_name)) {
            if (fileDialog.GetTypeFilterIndex() == 1) {
                DataManager dm;
                dm.SaveDataToFile(file_name, data);
            }
            else if (fileDialog.GetTypeFilterIndex() == 2)
                SaveCanvasToPng(file_name.c_str());

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
            std::string warning_txt = "File already exists, overwrite anyways?\n'" + g_canvas[g_cidx].name + "'";

            ImGui::Text(warning_txt.c_str());

            if (ImGui::Button("Yes")) {
                if (fileDialog.GetTypeFilterIndex() == 1) {
                    DataManager dm;
                    dm.SaveDataToFile(file_name, data);
                }
                else if (fileDialog.GetTypeFilterIndex() == 2)
                    SaveCanvasToPng(file_name.c_str());

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

    //if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    //{
    //    ImGui::Text("All those beautiful files will be deleted.\nThis operation cannot be undone!\n\n");
    //    ImGui::Separator();

    //    //static int unused_i = 0;
    //    //ImGui::Combo("Combo", &unused_i, "Delete\0Delete harder\0");

    //    static bool dont_ask_me_next_time = false;
    //    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    //    ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
    //    ImGui::PopStyleVar();

    //    if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
    //    ImGui::SetItemDefaultFocus();
    //    ImGui::SameLine();
    //    if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
    //    ImGui::EndPopup();
    //}

    if (ImGui::BeginPopupModal("New Project", NULL, ImGuiWindowFlags_NoResize)) { //ImGuiWindowFlags_AlwaysAutoResize
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
            //Todo: just call our canvas initialize function and pass the width / height
            std::string scene_name = "Sprite " + std::to_string(g_canvas.size() + 1);
            
            //Idk if this is practical in the long run... but...
            //...It should work for now!
            //Fix duplicate names... maybe? Without a global
            //...Tracking var... lol
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
        std::string warning_txt = "Save changes to '" + g_canvas[g_cidx].name + "' before closing?";

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

    if (!fileDialog.IsOpened())
        fileDialog.Open();

    fileDialog.Display();

    if (fileDialog.HasSelected())
    {
        g_canvas[g_cidx].LoadColorPalette(fileDialog.GetSelected().string());
        g_app.ui_state.reset();

        //std::cout << "Selected filename " << fileDialog.GetSelected().string() << std::endl;
        fileDialog.ClearSelected();
    }
}

//TODO: add multiple file formats?
//TODO: fix mem leak when loading images and closing the project...
//Idk if its the image load function itself or lack of cleaning up canvas.
//We shall do some tests
void LoadImageFileToCanvas(const std::string& filepath, const std::string& filename) {
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filepath.c_str(), &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return;

    // Set our canvas dimensions based on image
    cCanvas canvas = cCanvas(filename.c_str(), image_width, image_height);
    g_canvas.push_back(canvas);
    g_cidx = (uint16_t)g_canvas.size() - 1;

    std::vector<ImU32> image_layer;
    size_t image_size = image_width * image_height * 4;
    for (size_t i = 0; i < image_size; i += 4) {
        image_layer.push_back(ImColor(image_data[i], image_data[i + 1], image_data[i + 2], image_data[i + 3]));
    }

    // Initialize canvas with the loaded image data
    g_canvas[g_cidx].NewLayer(image_layer);

    stbi_image_free(image_data);
}

void cUIStateOpenProject::Update()
{
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_NoStatusBar);
    fileDialog.SetTitle("Load Project");
    fileDialog.SetTypeFilters({ ".spr", ".png" });

    if (!fileDialog.IsOpened())
        fileDialog.Open();

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

    if (!fileDialog.IsOpened())
        fileDialog.Open();

    fileDialog.Display();

    if (fileDialog.HasSelected())
    {
        g_app.ui_state.reset();

        std::vector<ImU32> myCols = g_canvas[g_cidx].myCols;

        myCols.erase(myCols.begin());
        myCols.erase(myCols.begin());

        std::string data = GetColorData(myCols, false);
        std::string file_name = fileDialog.GetSelected().string();

        //Add extension to file for the user
        if (!file_name.find(".pal") != std::string::npos) //file_name.size() > 0 && file_name[file_name.size() - 3] != '.'
        {
            file_name.append(".pal");
        }

        DataManager dm;
        dm.SaveDataToFile(file_name, data);

        std::cout << "Selected filename " << file_name << std::endl;
        fileDialog.ClearSelected();
    }
}
