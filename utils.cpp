#include "utils.h"

//MapToRect func
#include "canvas.h"
#include "camera.h"
#include "imgui.h"
#include <numeric>
#include <random>
#include <queue>
#include <set>
#include "stb_image.h"

cUtils g_util = cUtils();

bool cUtils::Hovering(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight) {
    // Convert mouse position to canvas space based on camera position and zoom
    const float mouseX = ImGui::GetMousePos().x;
    const float mouseY = ImGui::GetMousePos().y;

    // Compute canvas space boundaries
    const float startX = g_cam.x;
    const float startY = g_cam.y;
    const float endX = startX + iWidth * TILE_SIZE;
    const float endY = startY + iHeight * TILE_SIZE;

    // Check if mouse is within the canvas bounds
    return (mouseX >= startX && mouseX < endX && mouseY >= startY && mouseY < endY);
}

bool cUtils::Holding(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t mouseButton) {
    return ImGui::GetIO().MouseDown[mouseButton] && Hovering(iXStart, iYStart, iWidth, iHeight); // && hwWindow == GetActiveWindow()
}

bool cUtils::Clicked(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t btn) {
    return MousePressed(btn) && Hovering(iXStart, iYStart, iWidth, iHeight);
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

int cUtils::ColorDifferenceSquared(const ImU32& col1, const ImU32& col2) {
    const int r1 = (col1 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g1 = (col1 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b1 = (col1 >> IM_COL32_B_SHIFT) & 0xFF;
    const int a1 = (col1 >> IM_COL32_A_SHIFT) & 0xFF;
    const int r2 = (col2 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g2 = (col2 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b2 = (col2 >> IM_COL32_B_SHIFT) & 0xFF;
    const int a2 = (col2 >> IM_COL32_A_SHIFT) & 0xFF;
    const int diff = (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2) + (a1 - a2) * (a1 - a2);
    return diff;
}

// Helper function to compare two 1D vectors
bool cUtils::IsTilesEqual(const std::vector<ImU32>& a, const std::vector<ImU32>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

bool cUtils::IsClickingInsideCanvas(const ImVec2& startMousePos, const ImVec2& currentMousePos) {
    // Calculate canvas pixel bounds
    const float canvasLeft = g_cam.x;
    const float canvasTop = g_cam.y;
    const float canvasRight = canvasLeft + g_canvas[g_cidx].width * TILE_SIZE;
    const float canvasBottom = canvasTop + g_canvas[g_cidx].height * TILE_SIZE;

    // Check if stroke started AND ended inside canvas
    const bool startedInside = startMousePos.x >= canvasLeft && startMousePos.x <= canvasRight &&
        startMousePos.y >= canvasTop && startMousePos.y <= canvasBottom;

    const bool endedInside = currentMousePos.x >= canvasLeft && currentMousePos.x <= canvasRight &&
        currentMousePos.y >= canvasTop && currentMousePos.y <= canvasBottom;

    // Only allow if both are inside and UI is not blocking input
    return (startedInside || endedInside) && !g_app.ui_state;
}

std::vector<uint64_t> cUtils::GeneratePermutation(uint64_t size, uint64_t seed) {
    std::vector<uint64_t> permutation(size);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), std::default_random_engine(seed));
    return permutation;
}

void cUtils::GenerateRandomKeyAndSeed(uint64_t& key, uint64_t& seed) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> disKey(0, UINT64_MAX);
    std::uniform_int_distribution<uint64_t> disSeed(0, UINT64_MAX);

    key = disKey(gen);
    seed = disSeed(gen);
}

// Function to apply XOR to a color
ImU32 cUtils::XorColor(ImU32 color, uint64_t key) {
    // Extract the alpha channel
    const ImU32 alpha = color & 0xFF000000;
    // XOR only the RGB channels
    const ImU32 rgb = color & 0x00FFFFFF;
    const uint64_t key_rgb = key & 0x00FFFFFF;
    const ImU32 xor_rgb = rgb ^ key_rgb;
    // Combine the unchanged alpha channel with the XORed RGB channels
    return alpha | xor_rgb;
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

    nr = glm::clamp<int>(nr + errR * factor, 0, 255);
    ng = glm::clamp<int>(ng + errG * factor, 0, 255);
    nb = glm::clamp<int>(nb + errB * factor, 0, 255);

    neighborColor = IM_COL32(nr, ng, nb, 255);
}

// Quantize to a lower bit depth for a retro look
uint8_t Quantize(uint8_t color, uint8_t bits) {
    // Calculate the maximum value for the target bit depth
    const uint8_t maxVal = (1 << bits) - 1;
    return (color * maxVal) / 255 * (255 / maxVal);
}

// PS1-style Floyd-Steinberg dithering function
ImU32 cUtils::ApplyFloydSteinbergDithering(ImU32 color, uint64_t x, uint64_t y) {
    // Extract color components
    const uint8_t r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    const uint8_t g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    const uint8_t b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    // Quantize to lower bit depth (e.g., 4 bits)
    const uint8_t quantR = Quantize(r, 4);
    const uint8_t quantG = Quantize(g, 4);
    const uint8_t quantB = Quantize(b, 4);

    // Calculate the error
    const int errR = r - quantR;
    const int errG = g - quantG;
    const int errB = b - quantB;

    // Distribute the error
    DistributeError(x + 1, y, errR, errG, errB, 7.0 / 16.0);
    DistributeError(x - 1, y + 1, errR, errG, errB, 3.0 / 16.0);
    DistributeError(x, y + 1, errR, errG, errB, 5.0 / 16.0);
    DistributeError(x + 1, y + 1, errR, errG, errB, 1.0 / 16.0);

    return IM_COL32(quantR, quantG, quantB, 255);
}

ImU32 cUtils::BlendColor(ImU32 baseColor, uint8_t glyphAlpha) {
    // Extract base color components (ignoring the alpha channel)
    const uint8_t baseR = (baseColor >> IM_COL32_R_SHIFT) & 0xFF;
    const uint8_t baseG = (baseColor >> IM_COL32_G_SHIFT) & 0xFF;
    const uint8_t baseB = (baseColor >> IM_COL32_B_SHIFT) & 0xFF;

    // If the glyph has any alpha value greater than zero, we'll directly apply the base color
    if (glyphAlpha > 0) {
        return baseColor; // Use the solid color, ignoring alpha channel blending
    }

    // If the glyph is fully transparent (which in this case we don't want), return an unchanged color
    return IM_COL32(baseR, baseG, baseB, 255);  // Forcefully return opaque color
}

// Flood fill function
void cUtils::FloodFill(const int& x, const int& y, bool paint) {
    if (x < 0 || x >= g_canvas[g_cidx].width || y < 0 || y >= g_canvas[g_cidx].height)
        return;

    const ImU32 initialCol = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][x + y * g_canvas[g_cidx].width];
    const ImU32 fillCol = paint ? g_canvas[g_cidx].myCols[g_canvas[g_cidx].selColIndex] : initialCol;

    // Scale the threshold from 0-100 to 0-255
    const int threshold = std::pow((paint ? bucket_fill_threshold : magic_wand_threshold) * 255 / 100, 2);

    std::queue<std::pair<int, int>> queue;
    queue.push({ x, y });

    if (!paint) selectedIndexes.clear();

    const int totalSize = g_canvas[g_cidx].width * g_canvas[g_cidx].height;
    std::vector<bool> visited(totalSize, false);  // Use a boolean vector for visited check

    while (!queue.empty()) {
        std::pair<int, int> p = queue.front();
        queue.pop();
        const int curX = p.first;
        const int curY = p.second;

        if (curX < 0 || curX >= g_canvas[g_cidx].width || curY < 0 || curY >= g_canvas[g_cidx].height)
            continue;

        const uint32_t currentIndex = curX + curY * g_canvas[g_cidx].width;

        if (visited[currentIndex])
            continue;  // Skip already processed pixels

        visited[currentIndex] = true;

        const ImU32 currentCol = g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex];

        if (paint) {
            if (!selectedIndexes.empty() && std::find(selectedIndexes.begin(), selectedIndexes.end(), currentIndex) == selectedIndexes.end())
                continue;

            if (g_util.ColorDifferenceSquared(currentCol, initialCol) > threshold)
                continue;

            g_canvas[g_cidx].tiles[g_canvas[g_cidx].selLayerIndex][currentIndex] = fillCol;
        }
        else {
            if (g_util.ColorDifferenceSquared(currentCol, initialCol) > threshold)
                continue;

            if (std::find(selectedIndexes.begin(), selectedIndexes.end(), currentIndex) == selectedIndexes.end())
                selectedIndexes.push_back(currentIndex);
        }

        // Add neighbors to the queue only if they are valid
        if (curX + 1 < g_canvas[g_cidx].width) queue.push({ curX + 1, curY });
        if (curX - 1 >= 0) queue.push({ curX - 1, curY });
        if (curY + 1 < g_canvas[g_cidx].height) queue.push({ curX, curY + 1 });
        if (curY - 1 >= 0) queue.push({ curX, curY - 1 });
    }

    printf("FloodFill: Completed successfully!\n");
}

std::string cUtils::RemoveFileExtension(const std::string& file_name) {
    // Initialize the set of supported extensions
    const std::unordered_set<std::string> supported_extensions = { ".png", ".bmp", ".jpg", ".tga" };
    
    const size_t last_dot = file_name.find_last_of('.');
    if (last_dot != std::string::npos) {
        const std::string extension = file_name.substr(last_dot);
        if (supported_extensions.find(extension) != supported_extensions.end())
            return file_name.substr(0, last_dot);
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

// Compress a vector of ImU32 using Run-Length Encoding (RLE)
std::vector<uint8_t> cUtils::CompressCanvasDataRLE(const std::vector<ImU32>& input) {
    std::vector<uint8_t> compressedData;
    if (input.empty()) return compressedData;

    size_t count = 1;
    ImU32 previous = input[0];

    for (size_t i = 1; i < input.size(); ++i) {
        if (input[i] == previous && count < 255)
            ++count;
        else {
            // Store the count and value in the compressed data
            compressedData.push_back(static_cast<uint8_t>(count));
            compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_R_SHIFT) & 0xFF));
            compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_G_SHIFT) & 0xFF));
            compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_B_SHIFT) & 0xFF));
            compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_A_SHIFT) & 0xFF));

            // Reset for the next value
            previous = input[i];
            count = 1;
        }
    }

    // Add the final run to the compressed data
    compressedData.push_back(static_cast<uint8_t>(count));
    compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_R_SHIFT) & 0xFF));
    compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_G_SHIFT) & 0xFF));
    compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_B_SHIFT) & 0xFF));
    compressedData.push_back(static_cast<uint8_t>((previous >> IM_COL32_A_SHIFT) & 0xFF));

    return compressedData;
}

// Decompress a vector of ImU32 using Run-Length Encoding (RLE)
std::vector<ImU32> cUtils::DecompressCanvasDataRLE(const std::vector<uint8_t>& input) {
    std::vector<ImU32> decompressedData;
    if (input.empty() || input.size() % 5 != 0) return decompressedData; // Each entry has 5 bytes: count and RGBA

    for (size_t i = 0; i < input.size(); i += 5) {
        const uint8_t count = input[i];
        const uint8_t r = input[i + 1];
        const uint8_t g = input[i + 2];
        const uint8_t b = input[i + 3];
        const uint8_t a = input[i + 4];

        const ImU32 color = IM_COL32(r, g, b, a);
        decompressedData.insert(decompressedData.end(), count, color);
    }

    return decompressedData;
}

std::vector<uint8_t> cUtils::CompressColorRLE(ImU32 color) {
    std::vector<uint8_t> compressedData;

    compressedData.push_back(static_cast<uint8_t>((color >> IM_COL32_R_SHIFT) & 0xFF));
    compressedData.push_back(static_cast<uint8_t>((color >> IM_COL32_G_SHIFT) & 0xFF));
    compressedData.push_back(static_cast<uint8_t>((color >> IM_COL32_B_SHIFT) & 0xFF));
    compressedData.push_back(static_cast<uint8_t>((color >> IM_COL32_A_SHIFT) & 0xFF));

    return compressedData;
}

// Decompress a single ImU32 using Run-Length Encoding (RLE)
ImU32 cUtils::DecompressColorRLE(const std::vector<uint8_t>& compressedData) {
    if (compressedData.size() != 4) return 0; // Invalid data size for RGBA

    const uint8_t r = compressedData[0];
    const uint8_t g = compressedData[1];
    const uint8_t b = compressedData[2];
    const uint8_t a = compressedData[3];

    return IM_COL32(r, g, b, a);
}

std::vector<uint8_t> cUtils::CompressCanvasDataZlib(const std::vector<uint8_t>& data) {
    uLong sourceLen = data.size();
    uLong destLen = compressBound(sourceLen);
    std::vector<uint8_t> compressedData(destLen);

    int result = compress(compressedData.data(), &destLen, data.data(), sourceLen);

    if (result != Z_OK) {
        printf("Error: zlib compression failed with code %d\n", result);
        return {};
    }

    compressedData.resize(destLen); // Resize to actual compressed size
    return compressedData;
}

std::vector<uint8_t> cUtils::DecompressCanvasDataZlib(const std::vector<uint8_t>& compressedData, size_t originalSize) {
    std::vector<uint8_t> decompressedData(originalSize);
    uLong destLen = originalSize;

    int result = uncompress(decompressedData.data(), &destLen, compressedData.data(), compressedData.size());

    if (result != Z_OK) {
        printf("Error: zlib decompression failed with code %d\n", result);
        return {};
    }

    return decompressedData;
}

std::vector<uint8_t> cUtils::ConvertLayerToByteArray(const std::vector<ImU32>& layer) {
    std::vector<uint8_t> byteArray(layer.size() * sizeof(ImU32));

    // Copy the contents of the ImU32 vector to the byte array
    std::memcpy(byteArray.data(), layer.data(), layer.size() * sizeof(ImU32));

    return byteArray;
}

std::vector<ImU32> cUtils::ConvertByteArrayToLayer(const std::vector<uint8_t>& byteArray) {
    std::vector<ImU32> layer(byteArray.size() / sizeof(ImU32));

    // Copy the byte array data back into the ImU32 vector
    std::memcpy(layer.data(), byteArray.data(), byteArray.size());

    return layer;
}

float cUtils::RandomFloat(float min, float max) {
    static std::random_device rd;   // Seed for random number generation
    static std::mt19937 generator(rd()); // Mersenne Twister random number generator
    std::uniform_real_distribution<float> distribution(min, max); // Uniform distribution

    return distribution(generator); // Return the random float between min and max
}