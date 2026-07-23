#pragma once
#include "ofMain.h"
#include "ofxTpTypes.h"

// Auto-packer: generates slices for surfaces that have none, into the free
// space of the layout's outputs (existing slices are respected).
//
// Strategy: repeatedly cut the largest piece (anchored top-left of the
// remaining surface region) that fits the best free rect across all outputs,
// guillotine-splitting the remainder. Reproduces the manual "biggest rect
// intact + strip-split the rest" style. With moduleSize > 0, cut sizes and
// placement origins snap to the module grid; pieces that cannot fit aligned
// are an error, never silently distorted.
class ofxTpPacker {
public:
	// returns false if any surface could not be fully packed (partial slices
	// are kept so the failure is inspectable; nothing is distorted)
	bool pack(ofxTp::LayoutDef & layout);
};
