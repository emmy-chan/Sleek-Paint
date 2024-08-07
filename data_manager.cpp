#include "data_manager.h"
#include <fstream>
#include <filesystem>
#include <iostream>

void DataManager::SaveDataToFile(const std::string& input, const std::string& data) {
    std::ofstream file; file.open(input); file << data; file.close();
}

std::string DataManager::LoadDataFromFile(const std::string& input) {
    std::ifstream file(input);
    if (!file.is_open()) {
        std::cerr << "Error opening file for reading: " << input << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::cout << "File " << input << " loaded successfully." << std::endl;
    return buffer.str();
}

bool DataManager::LoadColorData(const std::string& filepath, std::vector<ImU32>& data) {
    bool expectAlpha = false;
    if (!std::filesystem::exists(filepath)) return false;
    std::vector<std::string> values; std::ifstream file(filepath); std::string value;

    // Push our data to our values vector
    while (file >> value) values.push_back(value);
    data.clear();

    // Create our two main colors at bottom
    std::vector<ImU32> main_cols = { IM_COL32(0, 0, 0, 255), IM_COL32(255, 255, 255, 255) };

    // Push our two main colors to the array
    for (auto& c : main_cols) data.push_back(c);
    const int jump_val = expectAlpha ? 4 : 3;

    for (size_t i = 0; i + jump_val <= values.size(); i += jump_val) {
        try {
            const int r = std::stoi(values[i]);
            const int g = std::stoi(values[i + 1]);
            const int b = std::stoi(values[i + 2]);
            const int a = expectAlpha ? std::stoi(values[i + 3]) : 255;
            data.push_back(IM_COL32(r, g, b, a));
        }
        catch (const std::exception& e) {
            //std::cerr << "Error converting color data: " << e.what() << std::endl; // Handle conversion error
            return false;
        }
    }

    return true;
}