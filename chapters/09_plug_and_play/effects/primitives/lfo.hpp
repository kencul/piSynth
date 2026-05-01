#pragma once

class LFO {
public:
	enum class Shape { Sine, Triangle, Square, SawUp, SawDown };

	explicit LFO();

	void set_rate(float hz);
	void set_shape(Shape shape);
	void set_phase_offset(float offset);
	void reset();    // resets phase to 0
	float process(); // returns -1.0 to 1.0

private:
	float phase     = 0.0f;
	float phase_inc = 0.0f;
	Shape shape     = Shape::Sine;
};