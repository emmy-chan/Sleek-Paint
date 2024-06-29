#include "data_manager.h"
#include <fstream>
#include <filesystem>

void DataManager::SaveDataToFile(const std::string& input, const std::string& data) {
    std::ofstream file;
    file.open(input);
    file << data;
    file.close();
}

bool DataManager::LoadColorData(const std::string& filepath, std::vector<ImU32>& data) {
    bool expectAlpha = false;
    
    if (!std::filesystem::exists(filepath))
        return false;

    std::vector<std::string> values;
    std::ifstream file(filepath);
    std::string value;

    // Push our data to our values vector
    while (file >> value)
        values.push_back(value);

    int jump_val = expectAlpha ? 4 : 3;
    data.clear();

    for (size_t i = 0; i + jump_val <= values.size(); i += jump_val) {
        try {
            int r = std::stoi(values[i]);
            int g = std::stoi(values[i + 1]);
            int b = std::stoi(values[i + 2]);
            int a = expectAlpha ? std::stoi(values[i + 3]) : 255;
            data.push_back(IM_COL32(r, g, b, a));
        }
        catch (const std::exception& e) {
            // Handle conversion error
            //std::cerr << "Error converting color data: " << e.what() << std::endl;
            return false;
        }
    }

    return true;
}