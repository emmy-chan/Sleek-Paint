#pragma once
#include "imgui.h"

#include <vector>
#include <string>

class DataManager {
public:
	void SaveDataToFile(const std::string& input, const std::string& data);
	bool LoadColorData(const std::string& filepath, std::vector<ImU32>& data);
};
