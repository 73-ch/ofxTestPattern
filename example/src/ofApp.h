#pragma once
#include "ofMain.h"
#include "ofxTestPattern.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void saveSnapshots();

	ofxTestPattern tp;
	int viewOutput = -1;      // -1 = preview, >= 0 = single output
	bool fitView = false;     // false = 1:1 (actual pixels), true = fit to window
};
