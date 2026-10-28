#pragma once
#include <array>
#include <cstdint>
inline constexpr std::array<uint8_t,32> expected_generation_receipt={32,229,135,116,251,207,21,42,36,81,202,8,81,67,245,116,107,56,174,83,25,184,21,255,51,94,18,143,147,133,235,81};
inline constexpr std::array<uint8_t,32> expected_source_0={74,111,117,164,65,79,239,0,240,195,10,29,163,196,67,216,96,52,132,224,130,82,78,254,227,150,181,73,196,39,90,5};
inline constexpr std::array<uint8_t,32> expected_spirv_0={203,158,251,154,83,7,242,186,62,162,243,44,196,199,154,184,48,4,177,189,111,172,210,76,139,117,40,217,27,165,192,126};
inline constexpr std::array<uint8_t,32> expected_container_0={170,14,46,71,24,202,50,195,245,220,23,155,88,167,20,221,55,197,207,31,32,137,175,214,13,42,47,154,1,163,23,229};
inline constexpr std::array<uint8_t,32> expected_source_1={108,156,79,116,129,186,203,159,245,211,199,78,5,203,39,200,177,7,176,152,251,239,171,139,153,65,174,225,53,80,133,70};
inline constexpr std::array<uint8_t,32> expected_spirv_1={177,171,164,186,131,99,231,248,124,179,73,11,170,120,100,2,124,132,158,54,200,15,239,63,18,38,176,110,115,196,208,71};
inline constexpr std::array<uint8_t,32> expected_container_1={100,233,182,63,163,27,153,84,206,23,34,182,169,147,44,75,208,56,172,208,127,217,165,109,235,108,0,22,168,34,64,181};
inline constexpr std::array<std::array<uint8_t,32>,2> expected_sources={expected_source_0,expected_source_1};
inline constexpr std::array<std::array<uint8_t,32>,2> expected_spirvs={expected_spirv_0,expected_spirv_1};
inline constexpr std::array<std::array<uint8_t,32>,2> expected_containers={expected_container_0,expected_container_1};
