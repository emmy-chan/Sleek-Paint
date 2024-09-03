#pragma once
#include "imgui.h"
#include <cstdint>
#include <vector>
#include <string>

#include "glm.hpp"

class cUtils {
public:
	bool Holding(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t mouseButton);
	bool Hovering(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight);
	bool Clicked(const short& iXStart, const short& iYStart, const short& iWidth, const short& iHeight, uint8_t mouseButton);
	bool MousePressed(const uint8_t& btn);
	bool MouseReleased(const uint8_t& btn);
	int ColorDistanceSquared(const ImU32& col1, const ImU32& col2);
	int ColorDifference(const ImU32& col1, const ImU32& col2);
	bool IsTilesEqual(const std::vector<ImU32>& a, const std::vector<ImU32>& b);
	const bool IsClickingOutsideCanvas(ImVec2 mouse);
	std::vector<uint64_t> GeneratePermutation(uint64_t size, uint64_t seed);
	ImU32 XorColor(ImU32 color, ImU32 key);
	void GenerateRandomKeyAndSeed(ImU32& key, uint64_t& seed);
	ImU32 AdjustSaturation(ImU32 color, float saturationFactor);
	ImU32 AdjustContrast(ImU32 color, float contrastFactor);
	ImU32 ApplyFloydSteinbergDithering(ImU32 color, uint64_t x, uint64_t y);
	ImU32 BlendColor(ImU32 baseColor, uint8_t glyphAlpha);
	void FloodFill(const int& x, const int& y, bool paint);
	std::string RemoveFileExtension(const std::string& file_name);
	void LoadImageFileToCanvas(const std::string& filepath, const std::string& filename);
	std::vector<uint8_t> CompressCanvasDataRLE(const std::vector<ImU32>& input);
	std::vector<ImU32> DecompressCanvasDataRLE(const std::vector<uint8_t>& input);
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