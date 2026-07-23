#pragma once
#include "ofMain.h"
#include "ofxTpTypes.h"
#include "ofxTpPatternRenderer.h"

// Writes the export suite to a directory:
//   {name}_{surface}_src.png            surface-native pattern
//   {name}_{output}_mapped.png          composited output
//   {name}_overlay_{...}.png            + slice outlines / ids / coords baked in
//   {name}_parts_{...}.png              slice outlines + ids, transparent background
//   masks/{name}_mask_{sliceId}.png     output-res, slice area = white
//   masks/{name}_mask_{output}.png      output-res, all used area = white
//   crops.json / crops.csv              machine-readable slice data
class ofxTpExporter {
public:
	void setup(ofxTpPatternRenderer * renderer) { this->renderer = renderer; }

	// patternTexture: surface name -> rendered pattern texture (native resolution)
	bool exportAll(const std::string & dirPath, const ofxTp::LayoutDef & layout,
	               const std::function<ofTexture &(const std::string &)> & patternTexture);

private:
	void drawOutline(const ofRectangle & r, const ofColor & c, int px);
	void drawSliceLabel(const ofxTp::SliceDef & sl, const ofRectangle & rect, const ofColor & textColor);
	bool saveFbo(ofFbo & fbo, const std::string & absPath, bool keepAlpha = false);
	bool writeCrops(const std::string & absDir, const ofxTp::LayoutDef & layout);
	static ofColor sliceColor(int indexInSurface);

	ofxTpPatternRenderer * renderer = nullptr;
};
