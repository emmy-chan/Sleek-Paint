#pragma once
#include "imgui.h"
#include <cstdint>
#include <vector>

#include "glm.hpp"

class cUtils {
public:
	bool Holding(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t mouseButton);
	bool Hovering(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight);
	bool Clicked(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t mouseButton);
	bool MousePressed(const uint8_t& btn);
	bool MouseReleased(const uint8_t& btn);
	glm::u32vec2 MapCoordsToRect(glm::u32vec2 coord, const int& w, const int& h);
	int ColorDistanceSquared(const ImU32& col1, const ImU32& col2);
	int ColorDifference(const ImU32& col1, const ImU32& col2);
	bool IsTilesEqual(const std::vector<ImU32>& a, const std::vector<ImU32>& b);
	bool IsClickingOutsideCanvas();
	//bool InitializeInputState(void);
	//void UpdateInputState();
	//int m_mouse_x;
	//int m_mouse_y;
//private:
	/// Information about the state of the mouse
	//uint8_t m_uCurrentMouseState;
	//uint8_t m_uPreviousMouseState;

	//uint8_t m_piPreviousMouse[4];

	///--- Last frame Keyboard state
	//uint8_t m_piPreviousInput[SDL_NUM_SCANCODES];
	///--- Current frame keyboard state
	//uint8_t m_piCurrentInput[SDL_NUM_SCANCODES];
};

extern cUtils g_util;