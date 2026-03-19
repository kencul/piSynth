#pragma once
#include <span>

class MasterBus {
public:
	void process(std::span<float> mix_l, std::span<float> mix_r);
};