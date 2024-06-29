#include "utils.h"

//MapToRect func
#include "canvas.h"
#include "camera.h"
#include "imgui.h"

cUtils g_util = cUtils();

bool cUtils::Hovering(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight) {
    const int mouseX = (int)ImGui::GetMousePos().x;
    const int mouseY = (int)ImGui::GetMousePos().y;

    if (mouseX >= iXStart && iWidth - 1 >= mouseX)
        if (mouseY >= iYStart && iHeight - 1 >= mouseY)
            return true;

    return false;
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

//This will also adapt to our camera position
// Fix coordinates according to our canvas [Credits: Javidx9]
glm::u32vec2 cUtils::MapCoordsToRect(glm::u32vec2 coord, const int& w, const int& h) {
    auto& io = ImGui::GetIO();

    glm::u16vec2 vViewPos;
    glm::u16vec2 vViewSize;
    glm::u16vec2 vWindowSize = { w, h };
    const float wasp = (float)(w) / (float)(h);

    vViewSize.x = (int32_t)vWindowSize.x;
    vViewSize.y = (int32_t)((float)vViewSize.x / wasp);

    if (vViewSize.y > vWindowSize.y)
    {
        vViewSize.y = vWindowSize.y;
        vViewSize.x = (int32_t)((float)vViewSize.y * wasp);
    }

    vViewPos = { (w - vViewSize.x) / 2, (h - vViewSize.y) / 2 };

    coord.x -= vViewPos.x + (int)g_cam.x;
    coord.y -= vViewPos.y + (int)g_cam.y;
    coord.x = (int32_t)(((float)coord.x / (float)(vWindowSize.x - (vViewPos.x * 2)) * (float)(g_canvas[g_cidx].width * g_canvas[g_cidx].TILE_SIZE)));
    coord.y = (int32_t)(((float)coord.y / (float)(vWindowSize.y - (vViewPos.y * 2)) * (float)(g_canvas[g_cidx].height * g_canvas[g_cidx].TILE_SIZE)));

    //coord.x = glm::clamp<int>(mouse.x, 0, w);
    //coord.y = glm::clamp<int>(mouse.y, 0, h);

    return coord;
}

// Function to calculate the squared distance between two colors to avoid taking the square root unnecessarily
int cUtils::ColorDistanceSquared(const ImU32 col1, const ImU32 col2) {
    const int r1 = (col1 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g1 = (col1 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b1 = (col1 >> IM_COL32_B_SHIFT) & 0xFF;
    const int r2 = (col2 >> IM_COL32_R_SHIFT) & 0xFF;
    const int g2 = (col2 >> IM_COL32_G_SHIFT) & 0xFF;
    const int b2 = (col2 >> IM_COL32_B_SHIFT) & 0xFF;
    return (r2 - r1) * (r2 - r1) + (g2 - g1) * (g2 - g1) + (b2 - b1) * (b2 - b1);
}
