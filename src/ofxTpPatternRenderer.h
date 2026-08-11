#pragma once
#include "ofMain.h"
#include "ofxTpTypes.h"

// Renders test patterns into a surface-sized FBO.
// Pixel-perfect policy: 1px lines are drawn as fills at integer coords,
// background is transparent (composited black by the facade in PATTERN mode).
class ofxTpPatternRenderer {
public:
	void setup(const std::string & fontPath = "");
	void render(ofFbo & fbo, const ofxTp::SurfaceDef & surface, ofxTp::PatternType type,
	            const ofxTp::StyleDef & style, float time = 0);

	// shared text helpers (also used by the exporter for annotations)
	ofTrueTypeFont & font(float px);
	void drawTextCentered(const std::string & str, float px, float cx, float baselineY);

private:
	void drawGrid(const ofxTp::SurfaceDef & s, const ofxTp::StyleDef & style);
	void drawColorBars(float x, float y, float w, float h, bool frame);
	void drawChecker(const ofxTp::SurfaceDef & s);
	void drawGradient(const ofxTp::SurfaceDef & s, bool horizontal, bool inverted = false);
	void drawGradientDithered(const ofxTp::SurfaceDef & s, bool horizontal);
	void drawMoving(const ofxTp::SurfaceDef & s, float time);
	void drawBorder(float w, float h, const ofColor & c);

	std::string fontPath;
	std::map<int, ofTrueTypeFont> fonts;
};
