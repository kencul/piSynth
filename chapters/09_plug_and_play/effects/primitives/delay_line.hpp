#pragma once
#include <vector>

class DelayLine {
public:
	void init(int max_samples);
	void write(float sample);
	float read(float delay_samples) const;
	void clear();

private:
	std::vector<float> buffer;
	int write_pos = 0;
};