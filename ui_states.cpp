#include "data_manager.h"
#include "ui_states.h"
#include "canvas.h"

#include <string>
#include <fstream>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// cout
#include <iostream>
#include "stb_image.h"

#include "utils.h"

// Nice little function for loading color data with alpha or no alpha
std::string GetColorData(const std::vector<ImU32>& colors, bool includeAlpha) {
    std::string data;
    for (uint16_t i = 0; i < colors.size(); i++) {
        const ImU32 color = colors[i];
        const int red = (color >> 0) & 0xFF, green = (color >> 8) & 0xFF,
        blue = (color >> 16) & 0xFF, alpha = (color >> 24) & 0xFF;

        data.append(std::to_string(red) + " ");
        data.append(std::to_string(green) + " ");
        data.append(std::to_string(blue) + " ");

        if (includeAlpha) data.append(std::to_string(alpha) + " ");
    }
    return data;
}

void SaveCanvasToImage(const char* name, const char* format) {
    int width = g_canvas[g_cidx].width, height = g_canvas[g_cidx].height;

    // Allocate memory for RGBA data: width * height * 4 bytes (for R, G, B, and A)
    unsigned char* imageData = new unsigned char[width * height * 4];
    int imagePos = 0;

    // Determine if the format supports transparency
    const bool supportsTransparency = (strcmp(format, "png") == 0 || strcmp(format, "bmp") == 0 || strcmp(format, "tga") == 0);

    // Initialize the canvas with white or black background depending on transparency support
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            imageData[imagePos++] = 255; // Red
            imageData[imagePos++] = 255; // Green
            imageData[imagePos++] = 255; // Blue
            imageData[imagePos++] = supportsTransparency ? 0 : 255; // Alpha (transparent or opaque)
        }
    }

    // Blend all layers
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            // Start with transparent or opaque background
            float finalR = supportsTransparency ? 0.0f : 1.0f, finalG = supportsTransparency ? 0.0f : 1.0f,
            finalB = supportsTransparency ? 0.0f : 1.0f, finalA = supportsTransparency ? 0.0f : 1.0f;

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
            const int idx = (i + j * width) * 4;
            imageData[idx] = static_cast<unsigned char>(finalR * 255);  // Red
            imageData[idx + 1] = static_cast<unsigned char>(finalG * 255);  // Green
            imageData[idx + 2] = static_cast<unsigned char>(finalB * 255);  // Blue
            imageData[idx + 3] = static_cast<unsigned char>(finalA * 255);  // Alpha
        }
    }

    // Save image based on the specified format
    bool success = false;
    if (strcmp(format, "png") == 0)
        success = stbi_write_png(name, width, height, 4, imageData, width * 4);
    else if (strcmp(format, "jpg") == 0) {
        // Convert RGBA to RGB by ignoring the alpha channel
        unsigned char* rgbData = new unsigned char[width * height * 3];
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

std::string SerializeCanvas(const cCanvas& canvas) {
    std::ostringstream oss;
    oss << canvas.name << "\n";
    oss << canvas.width << " " << canvas.height << "\n";
    oss << canvas.tiles.size() << "\n";

    for (size_t i = 0; i < canvas.tiles.size(); ++i) {
        oss << canvas.tiles[i].size() << "\n";
        for (uint32_t color : canvas.tiles[i]) {
            oss << color << " ";
        }
        oss << "\n";
        oss << canvas.layerNames[i] << "\n";
        oss << canvas.layerVisibility[i] << "\n";
    }

    return oss.str();
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
        file_name = fileDialog.GetSelected().string();

        // Remove the file extension
        file_name = g_util.RemoveFileExtension(file_name);

        // Set the file extension based on selection
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

        // Apply our extension
        file_name.append(extension);

        std::cout << "Saving file to: " << file_name << " with format: " << extension << std::endl;

        if (!std::filesystem::exists(file_name)) {
            if (fileDialog.GetTypeFilterIndex() == 1) {
                // Save the entire project
                DataManager dm;
                std::string serializedData = SerializeCanvas(g_canvas[g_cidx]);
                dm.SaveDataToFile(file_name, serializedData);
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
            const std::string warning_txt = "File already exists, overwrite anyways?\n'" + file_name + "'";
            ImGui::Text(warning_txt.c_str());

            if (ImGui::Button("Yes")) {
                if (fileDialog.GetTypeFilterIndex() == 1) {
                    // Save the entire project
                    DataManager dm;
                    std::string serializedData = SerializeCanvas(g_canvas[g_cidx]);
                    dm.SaveDataToFile(file_name, serializedData);
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

// Function to constrain proportions based on a fixed aspect ratio
void ConstrainProportions(int& width, int& height, float aspectRatio, bool widthChanged) {
    if (widthChanged) 
        height = static_cast<int>(width / aspectRatio);
    else
        width = static_cast<int>(height * aspectRatio);
}

void cUIStateNewProject::Update()
{
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({ 145, 150 }, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({ io.DisplaySize.x / 2 - 88, io.DisplaySize.y / 2 - 50 }, ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("New Project", NULL, ImGuiWindowFlags_NoResize)) { //ImGuiWindowFlags_AlwaysAutoResize
        static int wInput = 32, hInput = 32;
        const float aspectRatio = 1.0f;
        bool widthChanged = false;

        ImGui::Text("Width:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        if (ImGui::InputInt("##Width", &wInput)) {
            widthChanged = true;
            if (constrain_proportions) ConstrainProportions(wInput, hInput, aspectRatio, widthChanged);
        }

        ImGui::Text("Height:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        if (ImGui::InputInt("##Height", &hInput)) {
            widthChanged = false;
            if (constrain_proportions) ConstrainProportions(wInput, hInput, aspectRatio, widthChanged);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1, 1 });
        ImGui::Checkbox("Match Size", &constrain_proportions);
        ImGui::PopStyleVar();

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
    ImGui::SetNextWindowSize({ 145, 150 }, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({ io.DisplaySize.x / 2 - 88, io.DisplaySize.y / 2 - 50 }, ImGuiCond_Appearing);

    if (ImGui::Begin("Resize Canvas", NULL, ImGuiWindowFlags_NoCollapse)) {
        static int wInput = 32, hInput = 32;
        const float aspectRatio = 1.0f;
        bool widthChanged = false;

        ImGui::Text("Width:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        if (ImGui::InputInt("##Width", &wInput)) {
            widthChanged = true;
            if (constrain_proportions) ConstrainProportions(wInput, hInput, aspectRatio, widthChanged);
        }

        ImGui::Text("Height:");
        ImGui::SameLine();
        ImGui::SetCursorPosX(100);
        if (ImGui::InputInt("##Height", &hInput)) {
            widthChanged = false;
            if (constrain_proportions) ConstrainProportions(wInput, hInput, aspectRatio, widthChanged);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1, 1 });
        ImGui::Checkbox("Match Size", &constrain_proportions);
        ImGui::PopStyleVar();

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

cCanvas DeserializeCanvas(const std::string& data) {
    std::istringstream iss(data);
    std::string canvasName;
    int width, height;
    size_t layerCount;

    // Deserialize canvas name
    std::getline(iss, canvasName);
    std::cout << "Canvas name: " << canvasName << std::endl;

    // Deserialize canvas dimensions
    iss >> width >> height;
    std::cout << "Canvas dimensions: " << width << "x" << height << std::endl;

    // Create canvas object
    cCanvas canvas(canvasName, width, height);

    // Deserialize number of layers
    iss >> layerCount;
    std::cout << "Number of layers: " << layerCount << std::endl;
    if (layerCount > 1000) // Arbitrary large value to catch potential errors
        throw std::runtime_error("Unreasonable number of layers detected");

    canvas.tiles.resize(layerCount);
    canvas.layerNames.resize(layerCount);
    canvas.layerVisibility.resize(layerCount);

    // Deserialize each layer's data
    for (size_t i = 0; i < layerCount; ++i) {
        size_t layerSize;
        iss >> layerSize;
        std::cout << "Layer " << i << " size: " << layerSize << std::endl;

        // Check if layerSize is reasonable
        if (layerSize > 1000000) { // Arbitrary large value to catch potential errors
            throw std::runtime_error("Unreasonable layer size detected");
        }

        canvas.tiles[i].resize(layerSize);

        for (size_t j = 0; j < layerSize; ++j)
            iss >> canvas.tiles[i][j];

        iss >> std::ws; // Skip any whitespace
        std::getline(iss, canvas.layerNames[i]);
        iss >> canvas.layerVisibility[i];
    }

    return canvas;
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

        const std::string selectedFile = fileDialog.GetSelected().string();
        const std::string extension = selectedFile.substr(selectedFile.find_last_of('.'));

        if (extension == ".spr") {
            std::cout << "Loading project file: " << selectedFile << std::endl;

            DataManager dm;
            std::string serializedData = dm.LoadDataFromFile(selectedFile);
            if (!serializedData.empty()) {
                try {
                    cCanvas canvas = DeserializeCanvas(serializedData);
                    g_canvas.push_back(std::move(canvas));
                    g_cidx = (uint16_t)g_canvas.size() - 1;
                    std::cout << "Project loaded successfully with " << g_canvas[g_cidx].tiles.size() << " layers." << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "Error deserializing canvas: " << e.what() << std::endl;
                }
            }
            else {
                std::cerr << "Failed to load project file: " << selectedFile << std::endl;
            }
        }
        else
            LoadImageFileToCanvas(selectedFile, fileDialog.GetSelected().filename().string()); // Load an image file
        
        g_canvas[g_cidx].UpdateCanvasHistory();
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
        fileDialog.ClearSelected();
    }
}

void cUIStateRenameLayer::Update()
{
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({ 250, 115 }, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({ io.DisplaySize.x / 2 - 125, io.DisplaySize.y / 2 - 57 }, ImGuiCond_Appearing);
    static char newName[10] = "";

    if (ImGui::BeginPopupModal("Rename Layer", NULL, ImGuiWindowFlags_NoResize)) { //ImGuiWindowFlags_AlwaysAutoResize
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 16);
        ImGui::InputText("##New Layer Name", newName, IM_ARRAYSIZE(newName));

        bool nameExists = false;
        int count = 0;
        for (const auto& name : g_canvas[g_cidx].layerNames) {
            if (count != g_canvas[g_cidx].selLayerIndex && name == newName) {
                nameExists = true;
                break;
            }

            count++;
        }

        if (ImGui::Button(ICON_FA_CHECK, { ImGui::GetWindowWidth() * 0.5f - 12, 0 }) && !nameExists) {
            g_canvas[g_cidx].layerNames[g_canvas[g_cidx].selLayerIndex] = newName;
            ImGui::CloseCurrentPopup();

            // Reset our UI State
            g_app.ui_state.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TIMES, { ImGui::GetWindowWidth() * 0.5f - 12, 0})) {
            ImGui::CloseCurrentPopup();

            // Reset our UI State
            g_app.ui_state.reset();
        }

        if (nameExists) ImGui::Text("Name already exists!");

        ImGui::EndPopup();
    }
    else {
        // Ensure the string length does not exceed the size of newName
        if (g_canvas[g_cidx].layerNames[g_canvas[g_cidx].selLayerIndex].length() >= sizeof(newName)) {
            // Handle the error: truncate or notify
            std::strncpy(newName, g_canvas[g_cidx].layerNames[g_canvas[g_cidx].selLayerIndex].c_str(), sizeof(newName) - 1);
            newName[sizeof(newName) - 1] = '\0'; // Ensure null termination
        }
        else
            std::strcpy(newName, g_canvas[g_cidx].layerNames[g_canvas[g_cidx].selLayerIndex].c_str());

        ImGui::OpenPopup("Rename Layer");
    }
}