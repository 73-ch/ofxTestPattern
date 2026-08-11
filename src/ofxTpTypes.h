#pragma once
#include "ofMain.h"

namespace ofxTp {

enum class Mode {
	CONTENT,   // show host-drawn content only
	PATTERN,   // replace with test pattern
	OVERLAY    // content + pattern on top
};

enum class PatternType {
	GRID,        // numbered grid + circles + label + color bars (pixelmap style)
	COLOR_BARS,
	WHITE,
	CHECKER,
	GRADIENT_H,
	GRADIENT_V,
	GRADIENT_H_INV,  // reversed (white -> black)
	GRADIENT_V_INV,
	GRADIENT_H_DITHER,  // spatially dithered (deterministic hash, breaks 8-bit banding)
	GRADIENT_V_DITHER,
	MOVING       // moving bar + frame counter (latency / tearing check)
};

struct SurfaceDef {
	std::string name;
	int width = 0;
	int height = 0;
	float gridSize = 0;               // px, square, center-anchored. 0 = auto
	ofColor color = ofColor(0, 255, 255);
};

struct OutputDef {
	std::string name;
	int width = 0;
	int height = 0;
};

struct SliceDef {
	std::string id;
	std::string surface;
	std::string output;
	ofRectangle src;                  // rect in surface pixels
	glm::vec2 dst { 0, 0 };           // position in output pixels
	int rot = 0;                      // degrees, reserved (0 only for now)
};

struct StyleDef {
	std::string fontPath;             // empty = system sans
	ofColor borderColor = ofColor(0, 255, 0);
};

struct LayoutDef {
	std::string name = "untitled";
	float moduleSize = 0;             // snap unit for slice cuts/placement. 0 = off
	StyleDef style;
	std::vector<SurfaceDef> surfaces;
	std::vector<OutputDef> outputs;
	std::vector<SliceDef> slices;
};

inline std::string toString(Mode m) {
	switch (m) {
		case Mode::CONTENT: return "CONTENT";
		case Mode::PATTERN: return "PATTERN";
		case Mode::OVERLAY: return "OVERLAY";
	}
	return "?";
}

inline std::string toString(PatternType t) {
	switch (t) {
		case PatternType::GRID:       return "GRID";
		case PatternType::COLOR_BARS: return "COLOR_BARS";
		case PatternType::WHITE:      return "WHITE";
		case PatternType::CHECKER:    return "CHECKER";
		case PatternType::GRADIENT_H: return "GRADIENT_H";
		case PatternType::GRADIENT_V: return "GRADIENT_V";
		case PatternType::GRADIENT_H_INV: return "GRADIENT_H_INV";
		case PatternType::GRADIENT_V_INV: return "GRADIENT_V_INV";
		case PatternType::GRADIENT_H_DITHER: return "GRADIENT_H_DITHER";
		case PatternType::GRADIENT_V_DITHER: return "GRADIENT_V_DITHER";
		case PatternType::MOVING:     return "MOVING";
	}
	return "?";
}

// pick a round grid size close to h/8
inline float autoGridSize(int w, int h) {
	static const float candidates[] = { 40, 50, 64, 80, 100, 128, 160, 200, 256, 320, 400, 512 };
	float target = h / 8.0f;
	float best = candidates[0];
	for (float c : candidates) {
		if (std::abs(c - target) < std::abs(best - target)) best = c;
	}
	return best;
}

inline ofColor parseColor(const std::string & hex, const ofColor & fallback) {
	std::string s = hex;
	if (!s.empty() && s[0] == '#') s = s.substr(1);
	if (s.size() != 6) return fallback;
	return ofColor::fromHex((int)strtol(s.c_str(), nullptr, 16));
}

inline std::string colorToHex(const ofColor & c) {
	char buf[8];
	snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
	return std::string(buf);
}

} // namespace ofxTp
