#pragma once
#include "imgui.h"
#include "app.h"
#include "includes.h"
#include <unordered_set>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

extern uint8_t g_cidx; //The id of our current selected canvas project!
inline FT_Face face;
inline ID3D11Texture2D* canvasTexture = nullptr;
inline ID3D11ShaderResourceView* canvasSRV = nullptr;
inline ID3D11SamplerState* g_pSamplerStatePoint = nullptr;
inline std::vector<int> selectedIndexes;
inline std::unordered_map<int, std::vector<uint8_t>> copiedTiles; // Store copied tiles and their colors
inline std::vector<ImVec2> freeformPath;
inline float pen_pressure = 0;
inline uint8_t TILE_SIZE = 16;
inline uint8_t paintToolSelected = 0;
inline uint8_t brush_size = 1;
inline uint8_t line_size = 1;
inline uint8_t magic_wand_threshold = 1;
inline uint8_t bucket_fill_threshold = 1;
inline uint8_t bandaid_strength = 50;
inline uint8_t text_size = 10;
inline uint8_t rect_rounding = 0;
inline bool constrain_proportions = 0;

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
	TOOL_ELIPSE,
	TOOL_TEXT,
	TOOL_PAN,
	TOOL_ZOOM,
	TOOL_BANDAID,
	TOOL_FREEFORM_SELECT
};

struct CompressedLayer {
	std::vector<uint8_t> data;  // Compressed data
	size_t originalIndex;       // Index of the original layer
};

class cCanvas
{
public:
	cCanvas(const std::string& new_name, uint16_t new_width = 0, uint16_t new_height = 0, const ImU32& color = IM_COL32(0, 0, 0, 0), const std::vector<ImU32>&initial_data = {}) { name = new_name; LoadColorPalette("palettes/default.pal"); Initialize(initial_data, new_width, new_height, color); }; //Set our idx to new scene idx!
	std::string name;
	uint8_t canvas_idx = 1; // Canvas history index
	std::vector<std::vector<uint8_t>> previousCanvases;
	std::vector<ImU32> myCols;
	uint16_t selColIndex = 0;
	uint8_t selLayerIndex = 0;
	std::vector<std::vector<ImU32>> tiles;
	std::vector<CompressedLayer> compressedTiles; // 2D vector to store compressed layers when hidden
	std::vector<uint8_t> layerVisibility;
	std::vector<std::string> layerNames;
	std::vector<uint8_t> layerOpacity;
	uint16_t width;
	uint16_t height;
	void CreateCanvasTexture(ID3D11Device* device, uint32_t width, uint32_t height);
	void Initialize(const std::vector<ImU32>& initial_data = {}, const uint16_t& new_width = 0, const uint16_t& new_height = 0, const ImU32& color = IM_COL32(0, 0, 0, 0));
	void NewLayer(std::string name = "", const std::vector<ImU32>& initial_data = {}, ImU32 color = IM_COL32(0, 0, 0, 0));
	void Clear();
	void AdaptNewSize(int newWidth, int newHeight, bool maintainAspectRatio = false);
	void UpdateCanvasHistory();
	void LoadColorPalette(std::string input);
	void DestroyCanvas();
	bool IsImageInClipboard();
	void PasteImageFromClipboard();
	void PasteSelection();
	void CopySelection();
	void DeleteSelection();
	void UpdateZoom(float value);
	void Editor();
};

extern std::vector<cCanvas> g_canvas;