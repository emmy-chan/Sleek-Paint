#pragma once
#include "imgui.h"
#include "app.h"
#include "includes.h"
#include <string>

//The id of our current selected canvas project!
extern uint16_t g_cidx;

enum eTools {
	TOOL_BRUSH,
	TOOL_BUCKET,
	TOOL_ERASER,
	TOOL_DROPPER,
	TOOL_SELECT,
	TOOL_WAND,
	TOOL_MOVE,
	TOOL_LINE,
	TOOL_SQUARE,
	TOOL_ELIPSE
};

class cCanvas
{
public:
	cCanvas(const std::string& new_name) { name = new_name; LoadColorPalette("palettes/default.pal"); Initialize(); }; //Set our idx to new scene idx!
	std::string name;
	uint16_t TILE_SIZE = 16;
	uint8_t paintToolSelected = 0;
	uint16_t canvas_idx = 1;
	uint8_t brush_size = 1;
	uint8_t magic_wand_threshold = 1;
	uint8_t bucket_fill_threshold = 1;
	std::vector<std::vector<ImU32>> previousCanvases;
	std::vector<ImU32> myCols;
	uint16_t selColIndex = 0;
	uint8_t selLayerIndex = 0;
	std::vector<std::vector<ImU32>> tiles;
	std::vector<uint8_t> layerVisibility;
	uint16_t width = 1000;
	uint16_t height = 1000;
	void Initialize();
	void NewLayer();
	void AdaptNewSize(int width, int height);
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