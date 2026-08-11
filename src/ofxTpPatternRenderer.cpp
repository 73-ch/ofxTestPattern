#include "ofxTpPatternRenderer.h"

using namespace ofxTp;

void ofxTpPatternRenderer::setup(const std::string & path) {
	fontPath = path;
}

ofTrueTypeFont & ofxTpPatternRenderer::font(float px) {
	int key = std::max(6, (int)std::round(px));
	auto it = fonts.find(key);
	if (it != fonts.end()) return it->second;

	auto & f = fonts[key];
	ofTrueTypeFontSettings settings(fontPath.empty() ? OF_TTF_SANS : fontPath, key);
	settings.antialiased = true;
	if (!f.load(settings)) {
		ofLogWarning("ofxTestPattern") << "font '" << fontPath << "' failed, falling back to system sans";
		f.load(ofTrueTypeFontSettings(OF_TTF_SANS, key));
	}
	return f;
}

void ofxTpPatternRenderer::drawTextCentered(const std::string & str, float px, float cx, float baselineY) {
	auto & f = font(px);
	float w = f.stringWidth(str);
	f.drawString(str, std::round(cx - w / 2), std::round(baselineY));
}

void ofxTpPatternRenderer::render(ofFbo & fbo, const SurfaceDef & s, PatternType type,
                                  const StyleDef & style, float time) {
	fbo.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	ofEnableAlphaBlending();
	switch (type) {
		case PatternType::GRID:       drawGrid(s, style); break;
		case PatternType::COLOR_BARS: drawColorBars(0, 0, s.width, s.height, false); break;
		case PatternType::WHITE:
			ofSetColor(255);
			ofFill();
			ofDrawRectangle(0, 0, s.width, s.height);
			break;
		case PatternType::CHECKER:    drawChecker(s); break;
		case PatternType::GRADIENT_H: drawGradient(s, true); break;
		case PatternType::GRADIENT_V: drawGradient(s, false); break;
		case PatternType::GRADIENT_H_INV: drawGradient(s, true, true); break;
		case PatternType::GRADIENT_V_INV: drawGradient(s, false, true); break;
		case PatternType::GRADIENT_H_DITHER: drawGradientDithered(s, true); break;
		case PatternType::GRADIENT_V_DITHER: drawGradientDithered(s, false); break;
		case PatternType::MOVING:     drawMoving(s, time); break;
	}
	ofPopStyle();
	fbo.end();
}

void ofxTpPatternRenderer::drawGrid(const SurfaceDef & s, const StyleDef & style) {
	const float w = s.width, h = s.height;
	const float g = s.gridSize > 0 ? s.gridSize : autoGridSize(s.width, s.height);
	const float cx = w * 0.5f, cy = h * 0.5f;

	// full-cell counts per direction (center-anchored square grid)
	const int fullL = (int)std::floor(cx / g);
	const int fullR = (int)std::floor((w - cx) / g);
	const int fullU = (int)std::floor(cy / g);
	const int fullD = (int)std::floor((h - cy) / g);

	ofFill();
	ofSetColor(s.color);
	// grid lines: 1px fills at integer coords (pixel-perfect)
	for (int k = 1; cx + k * g < w; k++) ofDrawRectangle(std::round(cx + k * g), 0, 1, h);
	for (int k = 1; cx - k * g > 0; k++) ofDrawRectangle(std::round(cx - k * g), 0, 1, h);
	for (int k = 1; cy + k * g < h; k++) ofDrawRectangle(0, std::round(cy + k * g), w, 1);
	for (int k = 1; cy - k * g > 0; k++) ofDrawRectangle(0, std::round(cy - k * g), w, 1);
	// center cross, 3px
	ofDrawRectangle(std::round(cx) - 1, 0, 3, h);
	ofDrawRectangle(0, std::round(cy) - 1, w, 3);

	// circles: diameter = surface height, tangent chain across the width
	ofNoFill();
	ofSetColor(255);
	ofSetCircleResolution(240);
	int kmax = (int)std::ceil((w * 0.5f) / h) + 1;
	for (int k = -kmax; k <= kmax; k++) {
		float x = cx + k * h;
		if (x + h * 0.5f < 0 || x - h * 0.5f > w) continue;
		ofDrawCircle(x, cy, h * 0.5f - 1);
	}
	ofFill();

	// coordinate labels
	const float numSize = g * 0.22f;
	ofSetColor(255);
	for (int k = 1; k <= fullR; k++) {
		drawTextCentered(ofToString(k), numSize, cx + (k - 0.5f) * g, g * 0.35f);
		drawTextCentered(ofToString(k), numSize, cx + (k - 0.5f) * g, h - g * 0.18f);
	}
	for (int k = 1; k <= fullL; k++) {
		drawTextCentered(ofToString(-k), numSize, cx - (k - 0.5f) * g, g * 0.35f);
		drawTextCentered(ofToString(-k), numSize, cx - (k - 0.5f) * g, h - g * 0.18f);
	}
	for (int k = 1; k <= std::min(fullU, 26); k++) {
		std::string t(1, (char)('A' + k - 1));
		float y = cy - (k - 0.5f) * g + numSize * 0.4f;
		drawTextCentered(t, numSize, g * 0.18f, y);
		drawTextCentered(t, numSize, w - g * 0.18f, y);
	}
	for (int k = 1; k <= std::min(fullD, 26); k++) {
		std::string t(1, (char)('a' + k - 1));
		float y = cy + (k - 0.5f) * g + numSize * 0.4f;
		drawTextCentered(t, numSize, g * 0.18f, y);
		drawTextCentered(t, numSize, w - g * 0.18f, y);
	}

	// label block
	ofSetColor(255);
	drawTextCentered(s.name, h * 0.095f, cx, h * 0.245f);
	drawTextCentered(ofToString(s.width) + "px x " + ofToString(s.height) + "px", h * 0.048f, cx, h * 0.335f);
	drawTextCentered(ofToString(w / h, 2) + ":1", h * 0.048f, cx, h * 0.405f);
	drawTextCentered(ofToString(fullL + fullR) + " x " + ofToString(fullU + fullD) + " full squares",
	                 h * 0.048f, cx, h * 0.475f);

	// color bars, lower half
	drawColorBars(std::round(cx - w * 0.25f), std::round(h * 0.65f - h * 0.08f),
	              std::round(w * 0.5f), std::round(h * 0.16f), true);

	drawBorder(w, h, style.borderColor);
}

void ofxTpPatternRenderer::drawColorBars(float x, float y, float w, float h, bool frame) {
	static const ofColor bars[8] = {
		ofColor(255, 255, 255), ofColor(255, 255, 0), ofColor(0, 255, 255), ofColor(0, 255, 0),
		ofColor(255, 0, 255), ofColor(255, 0, 0), ofColor(0, 0, 255), ofColor(0, 0, 0)
	};
	ofFill();
	if (frame) {
		float m = std::max(4.0f, h * 0.04f);
		ofSetColor(18);
		ofDrawRectangle(x - m, y - m, w + m * 2, h + m * 2);
		ofSetColor(70);
		ofDrawRectangle(x - m, y - m, w + m * 2, 1);
		ofDrawRectangle(x - m, y + h + m - 1, w + m * 2, 1);
		ofDrawRectangle(x - m, y - m, 1, h + m * 2);
		ofDrawRectangle(x + w + m - 1, y - m, 1, h + m * 2);
	}
	for (int i = 0; i < 8; i++) {
		float x0 = std::round(x + i * w / 8.0f);
		float x1 = std::round(x + (i + 1) * w / 8.0f);
		ofSetColor(bars[i]);
		ofDrawRectangle(x0, y, x1 - x0, h);
	}
}

void ofxTpPatternRenderer::drawChecker(const SurfaceDef & s) {
	const float w = s.width, h = s.height;
	const float g = s.gridSize > 0 ? s.gridSize : autoGridSize(s.width, s.height);
	const float cx = w * 0.5f, cy = h * 0.5f;
	// cell boundaries anchored at center; parity anchored at center cell
	int kx0 = -(int)std::ceil(cx / g), kx1 = (int)std::ceil((w - cx) / g);
	int ky0 = -(int)std::ceil(cy / g), ky1 = (int)std::ceil((h - cy) / g);
	ofFill();
	ofSetColor(255);
	for (int kx = kx0; kx < kx1; kx++) {
		for (int ky = ky0; ky < ky1; ky++) {
			if (((kx + ky) % 2 + 2) % 2 == 1) continue;
			float x0 = std::max(0.0f, std::round(cx + kx * g));
			float y0 = std::max(0.0f, std::round(cy + ky * g));
			float x1 = std::min(w, std::round(cx + (kx + 1) * g));
			float y1 = std::min(h, std::round(cy + (ky + 1) * g));
			if (x1 > x0 && y1 > y0) ofDrawRectangle(x0, y0, x1 - x0, y1 - y0);
		}
	}
}

void ofxTpPatternRenderer::drawGradient(const SurfaceDef & s, bool horizontal, bool inverted) {
	ofMesh m;
	m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	const float w = s.width, h = s.height;
	const ofFloatColor c0(inverted ? 1 : 0);
	const ofFloatColor c1(inverted ? 0 : 1);
	if (horizontal) {
		m.addVertex({ 0, 0, 0 }); m.addColor(c0);
		m.addVertex({ 0, h, 0 }); m.addColor(c0);
		m.addVertex({ w, 0, 0 }); m.addColor(c1);
		m.addVertex({ w, h, 0 }); m.addColor(c1);
	} else {
		m.addVertex({ 0, 0, 0 }); m.addColor(c0);
		m.addVertex({ w, 0, 0 }); m.addColor(c0);
		m.addVertex({ 0, h, 0 }); m.addColor(c1);
		m.addVertex({ w, h, 0 }); m.addColor(c1);
	}
	m.draw();
}

// deterministic per-pixel hash -> [0,1). same coords always give the same value,
// so exports are reproducible across builds/runs
static inline float tpHash01(int x, int y) {
	uint32_t hv = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
	hv = (hv ^ (hv >> 13)) * 1274126177u;
	hv ^= hv >> 16;
	return (hv & 0xFFFFFF) / 16777216.0f;
}

void ofxTpPatternRenderer::drawGradientDithered(const SurfaceDef & s, bool horizontal) {
	const int w = s.width, h = s.height;
	ofPixels pix;
	pix.allocate(w, h, OF_PIXELS_RGB);
	unsigned char * p = pix.getData();
	const int span = horizontal ? (w > 1 ? w - 1 : 1) : (h > 1 ? h - 1 : 1);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			// continuous ramp value in [0,255], threshold-dithered to 8bit
			const float v = 255.0f * (horizontal ? x : y) / span;
			const int q = (int)ofClamp(std::floor(v + tpHash01(x, y)), 0, 255);
			const size_t i = ((size_t)y * w + x) * 3;
			p[i] = p[i + 1] = p[i + 2] = (unsigned char)q;
		}
	}
	ofImage img;
	img.setUseTexture(true);
	img.setFromPixels(pix);
	img.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
	ofSetColor(255);
	img.draw(0, 0);
}

void ofxTpPatternRenderer::drawMoving(const SurfaceDef & s, float time) {
	const float w = s.width, h = s.height;
	const float g = s.gridSize > 0 ? s.gridSize : autoGridSize(s.width, s.height);
	// bar sweeps the full width every 2 seconds
	float x = std::round(fmodf(time * w / 2.0f, w + g) - g);
	ofFill();
	ofSetColor(255);
	ofDrawRectangle(x, 0, g, h);
	drawTextCentered(ofToString(ofGetFrameNum()), h * 0.12f, w * 0.5f, h * 0.30f);
	drawTextCentered(s.name, h * 0.05f, w * 0.5f, h * 0.40f);
	drawBorder(w, h, ofColor(0, 255, 0));
}

void ofxTpPatternRenderer::drawBorder(float w, float h, const ofColor & c) {
	ofFill();
	ofSetColor(c);
	ofDrawRectangle(0, 0, w, 1);
	ofDrawRectangle(0, h - 1, w, 1);
	ofDrawRectangle(0, 0, 1, h);
	ofDrawRectangle(w - 1, 0, 1, h);
}
