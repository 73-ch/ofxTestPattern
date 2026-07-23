#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxTestPattern example");
	ofBackground(30);
	// OFXTP_LAYOUT=<file> to test other layouts (e.g. layout-autopack.json)
	tp.load(getenv("OFXTP_LAYOUT") ? getenv("OFXTP_LAYOUT") : "layout.json");
	tp.autoPack();   // fills surfaces that have no slices; no-op when fully mapped
	if (getenv("OFXTP_SAVE")) tp.save("layout-packed.json");
	tp.setMode(ofxTestPattern::Mode::PATTERN);
	tp.enableKeyShortcuts();   // t: mode, p: pattern type, e: export
}

void ofApp::update() {
	// dummy app content, visible in CONTENT / OVERLAY modes
	float t = ofGetElapsedTimef();
	for (auto & name : tp.getSurfaceNames()) {
		float w = tp.getSurfaceFbo(name).getWidth();
		float h = tp.getSurfaceFbo(name).getHeight();
		tp.begin(name);
		ofClear(25, 20, 45, 255);
		ofSetColor(255, 120, 40);
		float r = h * 0.2f;
		ofDrawCircle(ofMap(sinf(t * 0.5f + w), -1, 1, r, w - r), h * 0.5f, r);
		tp.end();
	}
	tp.update();

	// OFXTP_SNAPSHOT=1 / OFXTP_EXPORT=1 -> render 30 frames, write files, quit
	if (ofGetFrameNum() == 30 && (getenv("OFXTP_SNAPSHOT") || getenv("OFXTP_EXPORT"))) {
		if (getenv("OFXTP_SNAPSHOT")) saveSnapshots();
		if (getenv("OFXTP_EXPORT")) tp.exportAll("export");
		ofExit();
	}
}

void ofApp::saveSnapshots() {
	for (auto & name : tp.getOutputNames()) {
		ofPixels px;
		tp.getOutputFbo(name).readToPixels(px);
		ofSaveImage(px, "snap_" + name + ".png");
	}
	ofLogNotice() << "snapshots saved to " << ofToDataPath("", true);
}

void ofApp::draw() {
	auto outputs = tp.getOutputNames();
	std::string viewInfo;
	if (viewOutput >= 0 && viewOutput < (int)outputs.size()) {
		auto & name = outputs[viewOutput];
		auto & fbo = tp.getOutputFbo(name);
		if (fitView) {
			float scale = std::min(ofGetWidth() / fbo.getWidth(), ofGetHeight() / fbo.getHeight());
			float dw = fbo.getWidth() * scale;
			tp.drawOutput(name, std::round((ofGetWidth() - dw) / 2),
			              std::round((ofGetHeight() - fbo.getHeight() * scale) / 2), dw);
			viewInfo = name + "  fit " + ofToString(scale * 100, 0) + "%";
		} else {
			// 1:1 actual pixels, top-left origin
			tp.drawOutput(name, 0, 0);
			viewInfo = name + "  1:1";
		}
	} else {
		tp.drawPreview(ofRectangle(0, 0, ofGetWidth(), ofGetHeight()));
		viewInfo = "overview";
	}
	// HUD is a pure overlay: never offsets or scales the image under it
	ofDrawBitmapStringHighlight(
	    "[t] mode  [p] pattern  [f] fit/1:1  [e] export  [0] overview  [1-" + ofToString(outputs.size())
	    + "] output    " + viewInfo + "  |  " + tp.getStatusString(),
	    10, 20, ofColor(0, 200), ofColor(255));
}

void ofApp::keyPressed(int key) {
	if (key == 's') saveSnapshots();
	else if (key == 'f') fitView = !fitView;
	else if (key == '0') viewOutput = -1;
	else if (key >= '1' && key <= '9') {
		int idx = key - '1';
		if (idx < (int)tp.getOutputNames().size()) viewOutput = idx;
	}
}
