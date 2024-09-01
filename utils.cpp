#include "utils.h"

//MapToRect func
#include "canvas.h"
#include "camera.h"
#include "imgui.h"
#include <numeric>
#include <random>
#include <stack>
#include <set>
#include "stb_image.h"

cUtils g_util = cUtils();

bool cUtils::Hovering(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight) {
    const int mouseX = static_cast<int>(ImGui::GetMousePos().x);
    const int mouseY = static_cast<int>(ImGui::GetMousePos().y);

    return (mouseX >= iXStart && mouseX < (iXStart + iWidth) && mouseY >= iYStart && mouseY < (iYStart + iHeight));
}

bool cUtils::Holding(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t mouseButton) {
    if (ImGui::GetIO().MouseDown[mouseButton]) // && hwWindow == GetActiveWindow()
        if (Hovering(iXStart, iYStart, iWidth, iHeight))
            return true;

    return false;
}

bool cUtils::Clicked(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t btn) {
    if (MousePressed(btn))
        if (Hovering(iXStart, iYStart, iWidth, iHeight))
            return true;

    return false;
}

bool cUtils::MousePressed(const uint8_t& btn)
{
    return ImGui::GetIO().MouseDown[btn] && ImGui::GetIO().MouseDownDuration[btn] <= 0.f;
}

bool cUtils::MouseReleased(const uint8_t& btn)
{
	return !ImGui::GetIO().MouseDown[btn] && ImGui::GetIO().MouseDownDurationPrev[btn] > 0.f;
}

// Function to calculate the squared distance between two colors to avoid taking the square root unnecessarily
int cUtils::ColorDistanceSquared(const ImU32& col1, const ImU32& col2) {
    const int r1 = (col1 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g1 = (col1 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b1 = (col1 >> IM_COL32_B_SHIFT) & 0xFF;
    const int r2 = (col2 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g2 = (col2 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b2 = (col2 >> IM_COL32_B_SHIFT) & 0xFF;
    return (r2 - r1) * (r2 - r1) + (g2 - g1) * (g2 - g1) + (b2 - b1) * (b2 - b1);
}

int cUtils::ColorDifference(const ImU32& col1, const ImU32& col2) {
    const int r1 = (col1 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g1 = (col1 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b1 = (col1 >> IM_COL32_B_SHIFT) & 0xFF;
    const int a1 = (col1 >> IM_COL32_A_SHIFT) & 0xFF;
    const int r2 = (col2 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g2 = (col2 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b2 = (col2 >> IM_COL32_B_SHIFT) & 0xFF;
    const int a2 = (col2 >> IM_COL32_A_SHIFT) & 0xFF;
    const int diff = std::sqrt<int>((r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2) + (a1 - a2) * (a1 - a2));
    return diff;
}

// Helper function to compare two 1D vectors
bool cUtils::IsTilesEqual(const std::vector<ImU32>& a, const std::vector<ImU32>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

const bool cUtils::IsClickingOutsideCanvas(ImVec2 mouse) {
    auto& io = ImGui::GetIO();
    static bool bToolFlip = true;
    const bool bMouseOutsideCanvas = mouse.x < 200 || mouse.x > io.DisplaySize.x - 61 || mouse.y > io.DisplaySize.y - 20 || mouse.y < 24;
    static bool bCanDraw = !bMouseOutsideCanvas && !g_app.ui_state;

    if (bMouseOutsideCanvas && g_util.MousePressed(0))
        bToolFlip = false;

    if (!bToolFlip) {
        if (g_util.MouseReleased(0)) bToolFlip = !g_app.ui_state;
    }
    else
        bCanDraw = !bMouseOutsideCanvas && !g_app.ui_state;

    return !bCanDraw;
}

// Function to generate a permutation of indices
std::vector<uint64_t> cUtils::GeneratePermutation(uint64_t size, uint64_t seed) {
    std::vector<uint64_t> permutation(size);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), std::default_random_engine(seed));
    return permutation;
}

// Function to apply XOR to a color
ImU32 cUtils::XorColor(ImU32 color, ImU32 key) {
    // Extract the alpha channel
    const ImU32 alpha = color & 0xFF000000;
    // XOR only the RGB channels
    const ImU32 rgb = color & 0x00FFFFFF;
    const ImU32 key_rgb = key & 0x00FFFFFF;
    const ImU32 xor_rgb = rgb ^ key_rgb;
    // Combine the unchanged alpha channel with the XORed RGB channels
    return alpha | xor_rgb;
}

void cUtils::GenerateRandomKeyAndSeed(ImU32& key, uint64_t& seed) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> disKey;
    std::uniform_int_distribution<uint64_t> disSeed;

    key = disKey(gen);
    seed = disSeed(gen);
}

ImU32 cUtils::AdjustSaturation(ImU32 color, float saturationFactor) {
    float r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    float g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    float b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    const float gray = 0.299 * r + 0.587 * g + 0.114 * b;
    r = min(255.0f, gray + (r - gray) * saturationFactor);
    g = min(255.0f, gray + (g - gray) * saturationFactor);
    b = min(255.0f, gray + (b - gray) * saturationFactor);

    return IM_COL32((int)r, (int)g, (int)b, 255);
}

ImU32 cUtils::AdjustContrast(ImU32 color, float contrastFactor) {
    float r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    float g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    float b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    r = 128 + (r - 128) * contrastFactor;
    g = 128 + (g - 128) * contrastFactor;
    b = 128 + (b - 128) * contrastFactor;

    r = min(255.0f, max(0.0f, r));
    g = min(255.0f, max(0.0f, g));
    b = min(255.0f, max(0.0f, b));

    return IM_COL32((int)r, (int)g, (int)b, 255);
}

// Distribute error to neighboring pixels
void DistributeError(uint64_t x, uint64_t y, int errR, int errG, int errB, double factor) {
    if (x < 0 || x >= g_canvas[g_cidx].width || y < 0 || y >= g_canvas[g_cidx].height)
        return;

    const uint64_t index = x + y * g_canvas[g_cidx].width;
    ImU32& neighborColor = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][index];

    // Check if the alpha component is not zero
    if (((neighborColor >> IM_COL32_A_SHIFT) & 0xFF) == 0) return;

    uint8_t nr = (neighborColor >> IM_COL32_R_SHIFT) & 0xFF;
    uint8_t ng = (neighborColor >> IM_COL32_G_SHIFT) & 0xFF;
    uint8_t nb = (neighborColor >> IM_COL32_B_SHIFT) & 0xFF;

    nr = std::clamp<int>(nr + errR * factor, 0, 255);
    ng = std::clamp<int>(ng + errG * factor, 0, 255);
    nb = std::clamp<int>(nb + errB * factor, 0, 255);

    neighborColor = IM_COL32(nr, ng, nb, 255);
}

// PS1-style Floyd-Steinberg dithering function
ImU32 cUtils::ApplyFloydSteinbergDithering(ImU32 color, uint64_t x, uint64_t y) {
    // Extract color components
    uint8_t r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    uint8_t g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    uint8_t b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    // Save original color components
    const uint8_t origR = r;
    const uint8_t origG = g;
    const uint8_t origB = b;

    // Reduce color depth to 5 bits per channel
    r = (r & 0xF8) | (r >> 5);
    g = (g & 0xF8) | (g >> 5);
    b = (b & 0xF8) | (b >> 5);

    // Calculate error
    const int errR = origR - r;
    const int errG = origG - g;
    const int errB = origB - b;

    // Distribute the error to neighboring pixels
    DistributeError(x + 1, y, errR, errG, errB, 7.0 / 16.0);
    DistributeError(x - 1, y + 1, errR, errG, errB, 3.0 / 16.0);
    DistributeError(x, y + 1, errR, errG, errB, 5.0 / 16.0);
    DistributeError(x + 1, y + 1, errR, errG, errB, 1.0 / 16.0);

    return IM_COL32(r, g, b, 255);
}

// Flood fill function
void cUtils::FloodFill(const int& x, const int& y, bool paint) {
    if (x < 0 || x >= g_canvas[g_cidx].width || y < 0 || y >= g_canvas[g_cidx].height)
        return;

    const ImU32 initialCol = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][x + y * g_canvas[g_cidx].width];
    const ImU32 fillCol = paint ? g_canvas[g_cidx].myCols[g_canvas[g_cidx].selColIndex] : initialCol;

    // Scale the threshold from 0-100 to 0-765
    const int threshold = (paint ? bucket_fill_threshold : magic_wand_threshold) * 765 / 100;

    std::stack<std::pair<int, int>> stack;
    stack.push({ x, y });

    if (!paint) selectedIndexes.clear();

    std::set<uint32_t> visited; // Use uint32_t for larger canvas sizes

    while (!stack.empty()) {
        std::pair<int, int> p = stack.top();
        stack.pop();
        const int curX = p.first,
            curY = p.second;

        if (curX < 0 || curX >= g_canvas[g_cidx].width || curY < 0 || curY >= g_canvas[g_cidx].height)
            continue;

        const uint32_t currentIndex = curX + curY * g_canvas[g_cidx].width; // Changed to uint32_t

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

std::string cUtils::RemoveFileExtension(const std::string& file_name) {
    // Initialize the set of supported extensions
    const std::unordered_set<std::string> supported_extensions = { ".png", ".bmp", ".jpg", ".tga" };
    
    const size_t last_dot = file_name.find_last_of('.');
    if (last_dot != std::string::npos) {
        std::string extension = file_name.substr(last_dot);
        if (supported_extensions.find(extension) != supported_extensions.end()) {
            return file_name.substr(0, last_dot);
        }
    }
    return file_name;
}

// Load from file
void cUtils::LoadImageFileToCanvas(const std::string& filepath, const std::string& filename) {
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
    cCanvas canvas = cCanvas(filename.c_str(), image_width, image_height, IM_COL32(0, 0, 0, 0), image_layer);
    g_canvas.push_back(canvas);
    g_cidx = (uint16_t)g_canvas.size() - 1;

    stbi_image_free(image_data);
}