#pragma once

struct NoteEvent {
	enum class Type { NoteOn, NoteOff } type;
	int note;
	int velocity;
};