#include "ofxTpExporter.h"

using namespace ofxTp;

// deterministic per-surface slice palette: A=magenta, B=red, C=green, D=blue, ...
ofColor ofxTpExporter::sliceColor(int i) {
	static const ofColor palette[] = {
		ofColor(255, 0, 255), ofColor(255, 0, 0), ofColor(0, 200, 0), ofColor(0, 0, 255),
		ofColor(255, 128, 0), ofColor(0, 180, 180), ofColor(200, 180, 0), ofColor(140, 0, 255)
	};
	return palette[i % 8];
}

bool ofxTpExporter::exportAll(const std::string & dirPath, const LayoutDef & layout,
                              const std::function<ofTexture &(const std::string &)> & patternTexture) {
	if (!renderer) {
		ofLogError("ofxTestPattern") << "exporter not set up";
		return false;
	}
	std::string dir = ofToDataPath(dirPath, true);
	ofDirectory::createDirectory(dir + "/masks", false, true);

	// slice outline colors, indexed per surface (front-A magenta, front-B red, ...)
	std::map<std::string, int> orderInSurface;
	std::map<const SliceDef *, ofColor> colors;
	for (auto & sl : layout.slices) colors[&sl] = sliceColor(orderInSurface[sl.surface]++);

	const std::string p = dir + "/" + layout.name + "_";
	const std::string pm = dir + "/masks/" + layout.name + "_mask_";
	int written = 0;

	// ---- per surface: src / overlay src / parts src
	for (auto & s : layout.surfaces) {
		ofFbo fbo;
		fbo.allocate(s.width, s.height, GL_RGBA8);

		fbo.begin();
		ofClear(0, 0, 0, 255);
		ofSetColor(255);
		patternTexture(s.name).draw(0, 0);
		fbo.end();
		written += saveFbo(fbo, p + s.name + "_src.png");

		fbo.begin();
		for (auto & sl : layout.slices) {
			if (sl.surface != s.name) continue;
			drawOutline(sl.src, colors[&sl], 2);
			drawSliceLabel(sl, sl.src, ofColor(255));
		}
		fbo.end();
		written += saveFbo(fbo, p + "overlay_" + s.name + "_src.png");

		// parts: transparent background so it composites over anything;
		// labels in the slice color stay visible on white and black alike
		fbo.begin();
		ofClear(0, 0, 0, 0);
		for (auto & sl : layout.slices) {
			if (sl.surface != s.name) continue;
			drawOutline(sl.src, colors[&sl], 1);
			drawSliceLabel(sl, sl.src, colors[&sl]);
		}
		fbo.end();
		written += saveFbo(fbo, p + "parts_" + s.name + "_src.png", true);
	}

	// ---- per output: mapped / overlay mapped / parts mapped / masks
	for (auto & o : layout.outputs) {
		ofFbo fbo;
		fbo.allocate(o.width, o.height, GL_RGBA8);
		auto dstRect = [](const SliceDef & sl) {
			return ofRectangle(sl.dst.x, sl.dst.y, sl.src.width, sl.src.height);
		};

		fbo.begin();
		ofClear(0, 0, 0, 255);
		ofSetColor(255);
		for (auto & sl : layout.slices) {
			if (sl.output != o.name) continue;
			patternTexture(sl.surface).drawSubsection(sl.dst.x, sl.dst.y,
			    sl.src.width, sl.src.height, sl.src.x, sl.src.y);
		}
		fbo.end();
		written += saveFbo(fbo, p + o.name + "_mapped.png");

		fbo.begin();
		for (auto & sl : layout.slices) {
			if (sl.output != o.name) continue;
			drawOutline(dstRect(sl), colors[&sl], 2);
			drawSliceLabel(sl, dstRect(sl), ofColor(255));
		}
		fbo.end();
		written += saveFbo(fbo, p + "overlay_" + o.name + "_mapped.png");

		fbo.begin();
		ofClear(0, 0, 0, 0);
		for (auto & sl : layout.slices) {
			if (sl.output != o.name) continue;
			drawOutline(dstRect(sl), colors[&sl], 1);
			drawSliceLabel(sl, dstRect(sl), colors[&sl]);
		}
		fbo.end();
		written += saveFbo(fbo, p + "parts_" + o.name + "_mapped.png", true);

		for (auto & sl : layout.slices) {
			if (sl.output != o.name) continue;
			fbo.begin();
			ofClear(0, 0, 0, 255);
			ofSetColor(255);
			ofFill();
			ofDrawRectangle(dstRect(sl));
			fbo.end();
			written += saveFbo(fbo, pm + (sl.id.empty() ? sl.surface : sl.id) + ".png");
		}
		fbo.begin();
		ofClear(0, 0, 0, 255);
		ofSetColor(255);
		ofFill();
		for (auto & sl : layout.slices) {
			if (sl.output == o.name) ofDrawRectangle(dstRect(sl));
		}
		fbo.end();
		written += saveFbo(fbo, pm + o.name + ".png");
	}

	bool cropsOk = writeCrops(dir, layout);
	ofLogNotice("ofxTestPattern") << "exported " << written << " images + crops.json/csv to " << dir;
	return cropsOk && written > 0;
}

void ofxTpExporter::drawOutline(const ofRectangle & r, const ofColor & c, int px) {
	ofPushStyle();
	ofFill();
	ofSetColor(c);
	// inside-aligned 1px/2px frame, pixel-perfect
	ofDrawRectangle(r.x, r.y, r.width, px);
	ofDrawRectangle(r.x, r.y + r.height - px, r.width, px);
	ofDrawRectangle(r.x, r.y, px, r.height);
	ofDrawRectangle(r.x + r.width - px, r.y, px, r.height);
	ofPopStyle();
}

void ofxTpExporter::drawSliceLabel(const SliceDef & sl, const ofRectangle & rect, const ofColor & textColor) {
	float px = ofClamp(std::min(rect.width, rect.height) * 0.12f, 10, 44);
	float lh = px * 1.35f;
	float cx = rect.getCenter().x;
	// large slices: anchor label near the bottom edge, clear of title block and color bars
	float cy = rect.height > 1000 ? rect.getBottom() - lh * 3.4f : rect.getCenter().y;
	ofPushStyle();
	ofSetColor(textColor);
	renderer->drawTextCentered("[" + (sl.id.empty() ? sl.surface : sl.id) + "]", px, cx, cy - lh + px * 0.35f);
	renderer->drawTextCentered("W" + ofToString((int)sl.src.width), px, cx, cy + px * 0.35f);
	renderer->drawTextCentered("H" + ofToString((int)sl.src.height), px, cx, cy + lh + px * 0.35f);
	// origin of this rect in its own space (src coords on _src, dst coords on _mapped)
	float cpx = std::max(9.0f, px * 0.6f);
	auto & f = renderer->font(cpx);
	f.drawString("(" + ofToString((int)rect.x) + "," + ofToString((int)rect.y) + ")",
	             std::round(rect.x + 4), std::round(rect.y + cpx + 3));
	ofPopStyle();
}

bool ofxTpExporter::saveFbo(ofFbo & fbo, const std::string & absPath, bool keepAlpha) {
	ofPixels px;
	fbo.readToPixels(px);
	if (!keepAlpha) px.setImageType(OF_IMAGE_COLOR);
	if (!ofSaveImage(px, absPath)) {
		ofLogError("ofxTestPattern") << "failed to save " << absPath;
		return false;
	}
	return true;
}

bool ofxTpExporter::writeCrops(const std::string & absDir, const LayoutDef & layout) {
	ofJson j;
	j["name"] = layout.name;
	j["surfaces"] = ofJson::array();
	for (auto & s : layout.surfaces) {
		j["surfaces"].push_back({ { "name", s.name }, { "width", s.width }, { "height", s.height } });
	}
	j["outputs"] = ofJson::array();
	for (auto & o : layout.outputs) {
		j["outputs"].push_back({ { "name", o.name }, { "width", o.width }, { "height", o.height } });
	}
	j["slices"] = ofJson::array();
	for (auto & sl : layout.slices) {
		j["slices"].push_back({
			{ "id", sl.id }, { "surface", sl.surface },
			{ "src", { { "x", (int)sl.src.x }, { "y", (int)sl.src.y },
			           { "w", (int)sl.src.width }, { "h", (int)sl.src.height } } },
			{ "output", sl.output },
			{ "dst", { { "x", (int)sl.dst.x }, { "y", (int)sl.dst.y } } },
			{ "rot", sl.rot }
		});
	}
	bool ok = ofSavePrettyJson(absDir + "/crops.json", j);

	// flat table, TouchDesigner Table DAT friendly
	std::string csv = "id,surface,src_x,src_y,src_w,src_h,output,dst_x,dst_y,rot\n";
	for (auto & sl : layout.slices) {
		csv += sl.id + "," + sl.surface + ","
		    + ofToString((int)sl.src.x) + "," + ofToString((int)sl.src.y) + ","
		    + ofToString((int)sl.src.width) + "," + ofToString((int)sl.src.height) + ","
		    + sl.output + ","
		    + ofToString((int)sl.dst.x) + "," + ofToString((int)sl.dst.y) + ","
		    + ofToString(sl.rot) + "\n";
	}
	ok = ofBufferToFile(absDir + "/crops.csv", ofBuffer(csv.c_str(), csv.size())) && ok;
	return ok;
}
