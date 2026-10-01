#ifndef CORE_SETTINGS_HPP
#define CORE_SETTINGS_HPP

#include <cstdint>

namespace settings {

inline constexpr int  WINDOW_WIDTH = 1600;
inline constexpr int  WINDOW_HEIGHT = 1200;
inline constexpr char WINDOW_TITLE[] = "Vulkan Engine";

inline constexpr uint32_t DRAW_IMAGE_WIDTH = 2560;
inline constexpr uint32_t DRAW_IMAGE_HEIGHT = 1440;

inline constexpr unsigned FRAME_OVERLAP = 2;


inline constexpr float CAMERA_FOV_DEGREES = 70.0f;
inline constexpr float CAMERA_NEAR = 0.1f;
inline constexpr float CAMERA_FAR = 100000.0f;  
} // namespace settings

#endif // CORE_SETTINGS_HPP