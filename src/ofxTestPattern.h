#pragma once
#include "ofMain.h"
#include "ofxTpTypes.h"
#include "ofxTpPatternRenderer.h"
#include "ofxTpExporter.h"
#include "ofxTpPacker.h"

// Facade: layout load/save, surface/output FBO management, pattern display
// modes and slice compositing. See DESIGN.md.
class ofxTestPattern {
public:
	using Mode = ofxTp::Mode;
	using PatternType = ofxTp::PatternType;

	~ofxTestPattern();

	bool load(const std::string & path);
	bool save(const std::string & path);
	void setup(const ofxTp::LayoutDef & layoutDef);   // programmatic alternative to load()

	// draw app content into a surface (visible in CONTENT / OVERLAY modes)
	void begin(const std::string & surfaceName);
	void end();

	// re-renders dirty patterns and composites all outputs. call once per frame.
	void update();

	void setMode(Mode m);
	Mode getMode() const { return mode; }
	void cycleMode();
	void setPatternType(const std::string & surfaceName, PatternType t);  // "*" = all
	PatternType getPatternType(const std::string & surfaceName) const;
	void cyclePatternType();                                              // all surfaces

	void drawSurface(const std::string & name, float x, float y, float w = 0);
	void drawOutput(const std::string & name, float x, float y, float w = 0);
	void drawPreview(const ofRectangle & area);   // all surfaces + outputs, labeled

	ofFbo & getSurfaceFbo(const std::string & name);  // content fbo
	ofFbo & getOutputFbo(const std::string & name);   // composited output

	std::vector<std::string> getSurfaceNames() const;
	std::vector<std::string> getOutputNames() const;
	const ofxTp::LayoutDef & getLayout() const { return layout; }
	std::string getStatusString() const;   // "MODE / PATTERN" for HUDs

	// generates slices for surfaces that have none (moduleSize-aware, see
	// ofxTpPacker). call save() afterwards to persist the result
	bool autoPack();

	// writes PNG suite (src/mapped/overlay/parts/masks) + crops.json/csv.
	// uses each surface's current pattern type; dirPath is relative to bin/data
	bool exportAll(const std::string & dirPath = "export");

	// opt-in key shortcuts: t = cycle mode, p = cycle pattern type, e = exportAll
	void enableKeyShortcuts();
	void disableKeyShortcuts();

	bool validate();   // reference/bounds/module/coverage/overlap checks, logs problems

private:
	struct Surface {
		ofxTp::SurfaceDef def;
		ofFbo content;
		ofFbo pattern;
		PatternType type = PatternType::GRID;
		bool patternDirty = true;
	};
	struct Output {
		ofxTp::OutputDef def;
		ofFbo fbo;
		std::vector<const ofxTp::SliceDef *> slices;
	};

	void allocate();
	void rebuildSliceIndex();
	void onKeyPressed(ofKeyEventArgs & e);
	Surface * findSurface(const std::string & name);
	const Surface * findSurface(const std::string & name) const;
	Output * findOutput(const std::string & name);

	ofxTp::LayoutDef layout;
	std::vector<std::unique_ptr<Surface>> surfaces;
	std::vector<std::unique_ptr<Output>> outputs;
	ofxTpPatternRenderer renderer;
	ofxTpExporter exporter;

	Mode mode = Mode::PATTERN;
	Surface * currentBegun = nullptr;
	bool keysEnabled = false;
	int patternCycleIndex = 0;
};
