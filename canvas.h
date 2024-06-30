#pragma once
#include "imgui.h"
#include "app.h"
#include "includes.h"
#include <string>

//The id of our current selected canvas project!
extern uint16_t g_cidx;

class cCanvas
{
public:
	cCanvas(const std::string& new_name) { name = new_name; LoadColorPalette("palettes/default.pal"); Initialize(); }; //Set our idx to new scene idx!
	std::string name;
	float zoom = 0.f;
	uint16_t TILE_SIZE = 16;
	uint8_t paintToolSelected = 0;
	uint16_t canvas_idx = 1;
	uint8_t brush_size = 1;
	std::vector<std::vector<ImU32>> previousCanvases;
	std::vector<ImU32> myCols;
	uint16_t selColIndex = 0;
	uint8_t selLayerIndex = 0;
	std::vector<std::vector<ImU32>> tiles;
	uint16_t width = 1000;
	uint16_t height = 1000;
	void Initialize();
	void NewLayer();
	void AdaptNewSize();
	void Clear();
	void UpdateCanvasHistory();
	void LoadColorPalette(std::string input);
	void DestroyCanvas();
	void PasteSelection();
	void CopySelection();
	void DeleteSelection();
	void Editor();
};

extern std::vector<cCanvas> g_canvas;