#include "ofxTestPattern.h"

using namespace ofxTp;

ofxTestPattern::~ofxTestPattern() {
	disableKeyShortcuts();
}

//--------------------------------------------------------------
// layout IO

bool ofxTestPattern::load(const std::string & path) {
	if (!ofFile::doesFileExist(path)) {
		ofLogError("ofxTestPattern") << "layout not found: " << path;
		return false;
	}
	ofJson j = ofLoadJson(path);
	LayoutDef def;
	def.name = j.value("name", "untitled");
	if (j.contains("constraints")) {
		def.moduleSize = j["constraints"].value("moduleSize", 0.0f);
	}
	if (j.contains("style")) {
		def.style.fontPath = j["style"].value("font", "");
		def.style.borderColor = parseColor(j["style"].value("borderColor", "#00FF00"), ofColor(0, 255, 0));
	}
	for (auto & js : j.value("surfaces", ofJson::array())) {
		SurfaceDef s;
		s.name = js.at("name").get<std::string>();
		s.width = js.at("width").get<int>();
		s.height = js.at("height").get<int>();
		s.gridSize = js.value("gridSize", 0.0f);
		s.color = parseColor(js.value("color", "#00FFFF"), ofColor(0, 255, 255));
		def.surfaces.push_back(s);
	}
	for (auto & jo : j.value("outputs", ofJson::array())) {
		OutputDef o;
		o.name = jo.at("name").get<std::string>();
		o.width = jo.at("width").get<int>();
		o.height = jo.at("height").get<int>();
		def.outputs.push_back(o);
	}
	for (auto & jsl : j.value("slices", ofJson::array())) {
		SliceDef sl;
		sl.id = jsl.value("id", "");
		sl.surface = jsl.at("surface").get<std::string>();
		sl.output = jsl.at("output").get<std::string>();
		auto src = jsl.at("src");
		sl.src.set(src.at(0), src.at(1), src.at(2), src.at(3));
		auto dst = jsl.at("dst");
		sl.dst = { dst.at(0).get<float>(), dst.at(1).get<float>() };
		sl.rot = jsl.value("rot", 0);
		def.slices.push_back(sl);
	}
	setup(def);
	ofLogNotice("ofxTestPattern") << "loaded '" << def.name << "': "
	    << def.surfaces.size() << " surfaces, " << def.outputs.size() << " outputs, "
	    << def.slices.size() << " slices";
	return validate();
}

bool ofxTestPattern::save(const std::string & path) {
	ofJson j;
	j["name"] = layout.name;
	j["constraints"]["moduleSize"] = layout.moduleSize;
	j["style"]["font"] = layout.style.fontPath;
	j["style"]["borderColor"] = colorToHex(layout.style.borderColor);
	j["surfaces"] = ofJson::array();
	for (auto & s : layout.surfaces) {
		j["surfaces"].push_back({
			{ "name", s.name }, { "width", s.width }, { "height", s.height },
			{ "gridSize", s.gridSize }, { "color", colorToHex(s.color) }
		});
	}
	j["outputs"] = ofJson::array();
	for (auto & o : layout.outputs) {
		j["outputs"].push_back({ { "name", o.name }, { "width", o.width }, { "height", o.height } });
	}
	j["slices"] = ofJson::array();
	for (auto & sl : layout.slices) {
		j["slices"].push_back({
			{ "id", sl.id }, { "surface", sl.surface }, { "output", sl.output },
			{ "src", { (int)sl.src.x, (int)sl.src.y, (int)sl.src.width, (int)sl.src.height } },
			{ "dst", { (int)sl.dst.x, (int)sl.dst.y } },
			{ "rot", sl.rot }
		});
	}
	return ofSavePrettyJson(path, j);
}

void ofxTestPattern::setup(const LayoutDef & layoutDef) {
	layout = layoutDef;
	renderer.setup(layout.style.fontPath);
	exporter.setup(&renderer);
	allocate();
	rebuildSliceIndex();
}

bool ofxTestPattern::autoPack() {
	ofxTpPacker packer;
	bool ok = packer.pack(layout);
	rebuildSliceIndex();
	validate();
	return ok;
}

bool ofxTestPattern::exportAll(const std::string & dirPath) {
	update();   // make sure patterns are rendered
	return exporter.exportAll(dirPath, layout, [this](const std::string & name) -> ofTexture & {
		auto * s = findSurface(name);
		static ofTexture dummy;
		return s ? s->pattern.getTexture() : dummy;
	});
}

//--------------------------------------------------------------
// allocation / indexing

void ofxTestPattern::allocate() {
	surfaces.clear();
	outputs.clear();
	for (auto & def : layout.surfaces) {
		auto s = std::make_unique<Surface>();
		s->def = def;

		ofFboSettings content;
		content.width = def.width;
		content.height = def.height;
		content.internalformat = GL_RGBA8;
		s->content.allocate(content);
		s->content.begin();
		ofClear(0, 0, 0, 255);
		s->content.end();

		// MSAA smooths circles; 1px rect-lines on integer coords are unaffected
		ofFboSettings pattern;
		pattern.width = def.width;
		pattern.height = def.height;
		pattern.internalformat = GL_RGBA8;
		pattern.numSamples = std::min(4, ofFbo::maxSamples());
		s->pattern.allocate(pattern);
		s->patternDirty = true;
		surfaces.push_back(std::move(s));
	}
	for (auto & def : layout.outputs) {
		auto o = std::make_unique<Output>();
		o->def = def;
		ofFboSettings fs;
		fs.width = def.width;
		fs.height = def.height;
		fs.internalformat = GL_RGBA8;
		o->fbo.allocate(fs);
		o->fbo.begin();
		ofClear(0, 0, 0, 255);
		o->fbo.end();
		outputs.push_back(std::move(o));
	}
}

void ofxTestPattern::rebuildSliceIndex() {
	for (auto & o : outputs) o->slices.clear();
	for (auto & sl : layout.slices) {
		if (auto * o = findOutput(sl.output)) o->slices.push_back(&sl);
	}
}

ofxTestPattern::Surface * ofxTestPattern::findSurface(const std::string & name) {
	for (auto & s : surfaces) if (s->def.name == name) return s.get();
	return nullptr;
}

const ofxTestPattern::Surface * ofxTestPattern::findSurface(const std::string & name) const {
	for (auto & s : surfaces) if (s->def.name == name) return s.get();
	return nullptr;
}

ofxTestPattern::Output * ofxTestPattern::findOutput(const std::string & name) {
	for (auto & o : outputs) if (o->def.name == name) return o.get();
	return nullptr;
}

//--------------------------------------------------------------
// validation

bool ofxTestPattern::validate() {
	bool ok = true;
	std::string tag = "ofxTestPattern";
	for (auto & sl : layout.slices) {
		std::string label = "slice '" + (sl.id.empty() ? sl.surface : sl.id) + "'";
		auto * s = findSurface(sl.surface);
		auto * o = findOutput(sl.output);
		if (!s) { ofLogError(tag) << label << ": unknown surface '" << sl.surface << "'"; ok = false; continue; }
		if (!o) { ofLogError(tag) << label << ": unknown output '" << sl.output << "'"; ok = false; continue; }
		if (sl.src.x < 0 || sl.src.y < 0
		    || sl.src.getRight() > s->def.width || sl.src.getBottom() > s->def.height) {
			ofLogError(tag) << label << ": src rect exceeds surface bounds";
			ok = false;
		}
		if (sl.dst.x < 0 || sl.dst.y < 0
		    || sl.dst.x + sl.src.width > o->def.width || sl.dst.y + sl.src.height > o->def.height) {
			ofLogError(tag) << label << ": dst rect exceeds output bounds";
			ok = false;
		}
		if (layout.moduleSize > 0) {
			auto aligned = [&](float v) { return std::fmod(v, layout.moduleSize) == 0; };
			if (!aligned(sl.src.x) || !aligned(sl.src.y) || !aligned(sl.dst.x) || !aligned(sl.dst.y)) {
				ofLogWarning(tag) << label << ": not aligned to moduleSize " << layout.moduleSize;
			}
		}
		if (sl.rot != 0) {
			ofLogWarning(tag) << label << ": rot != 0 not supported yet, ignored";
		}
	}
	// pairwise overlap: src rects within a surface, dst rects within an output.
	// touching edges are not overlaps
	auto overlaps = [](const ofRectangle & a, const ofRectangle & b) {
		return a.x < b.x + b.width && a.x + a.width > b.x
		    && a.y < b.y + b.height && a.y + a.height > b.y;
	};
	for (size_t i = 0; i < layout.slices.size(); i++) {
		for (size_t j = i + 1; j < layout.slices.size(); j++) {
			auto & a = layout.slices[i];
			auto & b = layout.slices[j];
			if (a.surface == b.surface && overlaps(a.src, b.src)) {
				ofLogError(tag) << "slices '" << a.id << "' and '" << b.id << "': src rects overlap in surface '" << a.surface << "'";
				ok = false;
			}
			ofRectangle da(a.dst.x, a.dst.y, a.src.width, a.src.height);
			ofRectangle db(b.dst.x, b.dst.y, b.src.width, b.src.height);
			if (a.output == b.output && overlaps(da, db)) {
				ofLogError(tag) << "slices '" << a.id << "' and '" << b.id << "': dst rects overlap in output '" << a.output << "'";
				ok = false;
			}
		}
	}
	// coverage: with no overlaps, uncovered px = surface area - sum of clipped src areas
	for (auto & s : surfaces) {
		double covered = 0;
		bool mapped = false;
		for (auto & sl : layout.slices) {
			if (sl.surface != s->def.name) continue;
			mapped = true;
			ofRectangle c = sl.src.getIntersection(ofRectangle(0, 0, s->def.width, s->def.height));
			covered += (double)c.width * c.height;
		}
		if (!mapped) {
			ofLogWarning(tag) << "surface '" << s->def.name << "' has no slices (not routed to any output)";
		} else {
			double uncovered = (double)s->def.width * s->def.height - covered;
			if (uncovered > 0) {
				ofLogWarning(tag) << "surface '" << s->def.name << "': " << (long long)uncovered
				    << " px not covered by any slice";
			}
		}
	}
	return ok;
}

//--------------------------------------------------------------
// content drawing

void ofxTestPattern::begin(const std::string & surfaceName) {
	if (currentBegun) {
		ofLogError("ofxTestPattern") << "begin('" << surfaceName << "') called before end()";
		return;
	}
	auto * s = findSurface(surfaceName);
	if (!s) {
		ofLogError("ofxTestPattern") << "begin: unknown surface '" << surfaceName << "'";
		return;
	}
	currentBegun = s;
	s->content.begin();
}

void ofxTestPattern::end() {
	if (!currentBegun) return;
	currentBegun->content.end();
	currentBegun = nullptr;
}

//--------------------------------------------------------------
// modes / patterns

void ofxTestPattern::setMode(Mode m) {
	mode = m;
}

void ofxTestPattern::cycleMode() {
	mode = (Mode)(((int)mode + 1) % 3);
	ofLogNotice("ofxTestPattern") << "mode: " << toString(mode);
}

void ofxTestPattern::setPatternType(const std::string & surfaceName, PatternType t) {
	for (auto & s : surfaces) {
		if (surfaceName == "*" || s->def.name == surfaceName) {
			s->type = t;
			s->patternDirty = true;
		}
	}
}

ofxTestPattern::PatternType ofxTestPattern::getPatternType(const std::string & surfaceName) const {
	if (auto * s = findSurface(surfaceName)) return s->type;
	return PatternType::GRID;
}

void ofxTestPattern::cyclePatternType() {
	static const PatternType order[] = {
		PatternType::GRID, PatternType::COLOR_BARS, PatternType::WHITE, PatternType::CHECKER,
		PatternType::GRADIENT_H, PatternType::GRADIENT_V, PatternType::MOVING
	};
	patternCycleIndex = (patternCycleIndex + 1) % 7;
	setPatternType("*", order[patternCycleIndex]);
	ofLogNotice("ofxTestPattern") << "pattern: " << toString(order[patternCycleIndex]);
}

//--------------------------------------------------------------
// update / compositing

void ofxTestPattern::update() {
	float t = ofGetElapsedTimef();
	for (auto & s : surfaces) {
		if (s->patternDirty || s->type == PatternType::MOVING) {
			renderer.render(s->pattern, s->def, s->type, layout.style, t);
			s->patternDirty = false;
		}
	}
	for (auto & o : outputs) {
		o->fbo.begin();
		ofClear(0, 0, 0, 255);
		ofSetColor(255);
		for (auto * sl : o->slices) {
			auto * s = findSurface(sl->surface);
			if (!s) continue;
			// 1:1 texel copy, no scaling (pixel-perfect)
			if (mode != Mode::PATTERN) {
				s->content.getTexture().drawSubsection(sl->dst.x, sl->dst.y,
				    sl->src.width, sl->src.height, sl->src.x, sl->src.y);
			}
			if (mode != Mode::CONTENT) {
				s->pattern.getTexture().drawSubsection(sl->dst.x, sl->dst.y,
				    sl->src.width, sl->src.height, sl->src.x, sl->src.y);
			}
		}
		o->fbo.end();
	}
}

//--------------------------------------------------------------
// drawing

void ofxTestPattern::drawSurface(const std::string & name, float x, float y, float w) {
	auto * s = findSurface(name);
	if (!s) return;
	float scale = w > 0 ? w / s->def.width : 1;
	ofPushMatrix();
	ofPushStyle();
	ofTranslate(x, y);
	ofScale(scale, scale);
	if (mode == Mode::PATTERN) {
		ofSetColor(0);
		ofFill();
		ofDrawRectangle(0, 0, s->def.width, s->def.height);
	}
	ofSetColor(255);
	if (mode != Mode::PATTERN) s->content.draw(0, 0);
	if (mode != Mode::CONTENT) s->pattern.draw(0, 0);
	ofPopStyle();
	ofPopMatrix();
}

void ofxTestPattern::drawOutput(const std::string & name, float x, float y, float w) {
	auto * o = findOutput(name);
	if (!o) return;
	float scale = w > 0 ? w / o->def.width : 1;
	ofPushStyle();
	ofSetColor(255);
	o->fbo.draw(x, y, o->def.width * scale, o->def.height * scale);
	ofPopStyle();
}

void ofxTestPattern::drawPreview(const ofRectangle & area) {
	const float labelH = 18;
	const float pad = 8;
	auto drawRow = [&](std::vector<std::string> names, bool isOutput, float rowY, float rowH) {
		if (names.empty()) return;
		float cw = area.width / names.size();
		for (size_t i = 0; i < names.size(); i++) {
			float iw = 0, ih = 0;
			if (isOutput) {
				auto * o = findOutput(names[i]);
				iw = o->def.width; ih = o->def.height;
			} else {
				auto * s = findSurface(names[i]);
				iw = s->def.width; ih = s->def.height;
			}
			float availW = cw - pad * 2, availH = rowH - labelH - pad * 2;
			float scale = std::min(availW / iw, availH / ih);
			float dw = iw * scale, dh = ih * scale;
			float dx = area.x + i * cw + (cw - dw) / 2;
			float dy = rowY + (rowH - labelH - dh) / 2;
			if (isOutput) drawOutput(names[i], dx, dy, dw);
			else drawSurface(names[i], dx, dy, dw);
			ofPushStyle();
			ofNoFill();
			ofSetColor(90);
			ofDrawRectangle(dx - 1, dy - 1, dw + 2, dh + 2);
			ofPopStyle();
			std::string label = names[i] + "  " + ofToString((int)iw) + "x" + ofToString((int)ih);
			ofDrawBitmapStringHighlight(label, dx, dy + dh + labelH - 3, ofColor(0, 200), ofColor(255));
		}
	};
	float half = area.height / 2;
	drawRow(getSurfaceNames(), false, area.y, half);
	drawRow(getOutputNames(), true, area.y + half, half);
}

//--------------------------------------------------------------
// accessors

ofFbo & ofxTestPattern::getSurfaceFbo(const std::string & name) {
	auto * s = findSurface(name);
	static ofFbo dummy;
	return s ? s->content : dummy;
}

ofFbo & ofxTestPattern::getOutputFbo(const std::string & name) {
	auto * o = findOutput(name);
	static ofFbo dummy;
	return o ? o->fbo : dummy;
}

std::vector<std::string> ofxTestPattern::getSurfaceNames() const {
	std::vector<std::string> names;
	for (auto & s : surfaces) names.push_back(s->def.name);
	return names;
}

std::vector<std::string> ofxTestPattern::getOutputNames() const {
	std::vector<std::string> names;
	for (auto & o : outputs) names.push_back(o->def.name);
	return names;
}

std::string ofxTestPattern::getStatusString() const {
	std::string s = toString(mode);
	if (mode != Mode::CONTENT && !surfaces.empty()) {
		s += " / " + toString(surfaces.front()->type);
	}
	return s;
}

//--------------------------------------------------------------
// key shortcuts

void ofxTestPattern::enableKeyShortcuts() {
	if (keysEnabled) return;
	ofAddListener(ofEvents().keyPressed, this, &ofxTestPattern::onKeyPressed);
	keysEnabled = true;
}

void ofxTestPattern::disableKeyShortcuts() {
	if (!keysEnabled) return;
	ofRemoveListener(ofEvents().keyPressed, this, &ofxTestPattern::onKeyPressed);
	keysEnabled = false;
}

void ofxTestPattern::onKeyPressed(ofKeyEventArgs & e) {
	if (e.key == 't') cycleMode();
	else if (e.key == 'p') cyclePatternType();
	else if (e.key == 'e') exportAll();
}
