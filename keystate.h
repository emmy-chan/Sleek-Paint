#pragma once
#include <WinUser.h>
#include <unordered_map>

class KeyState {
public:
    // Update the state of all keys
    void update() {
        for (auto& key : keyStates) {
            const bool is_down_now = (GetAsyncKeyState(key.first) & 0x8000) != 0;
            key.second.was_pressed = !key.second.is_down && is_down_now;
            key.second.was_released = key.second.is_down && !is_down_now;
            key.second.is_down = is_down_now;
        }
    }

    // Check if a key is currently held down
    bool is_key_held(int key) {
        return keyStates[key].is_down;
    }

    // Check if a key was pressed (transition from up to down)
    bool key_pressed(int key) {
        return keyStates[key].was_pressed;
    }

    // Check if a key was released (transition from down to up)
    bool key_released(int key) {
        return keyStates[key].was_released;
    }

private:
    struct KeyStateInfo {
        bool is_down = false;
        bool was_pressed = false;
        bool was_released = false;
    };

    std::unordered_map<int, KeyStateInfo> keyStates;
};

inline KeyState key_state;