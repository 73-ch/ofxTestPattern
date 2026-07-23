#include "ofxTpPacker.h"
#include <deque>
#include <set>

using namespace ofxTp;

namespace {

bool rectsIntersect(const ofRectangle & a, const ofRectangle & b) {
	return a.x < b.getRight() && a.getRight() > b.x
	    && a.y < b.getBottom() && a.getBottom() > b.y;
}

bool rectContains(const ofRectangle & a, const ofRectangle & b) {
	return b.x >= a.x && b.y >= a.y && b.getRight() <= a.getRight() && b.getBottom() <= a.getBottom();
}

// free space as a maximal-rectangles set (rects may overlap each other)
struct FreeSet {
	std::vector<ofRectangle> rects;

	void init(float w, float h) { rects = { ofRectangle(0, 0, w, h) }; }

	void subtract(const ofRectangle & r) {
		std::vector<ofRectangle> out;
		for (auto & f : rects) {
			if (!rectsIntersect(f, r)) {
				out.push_back(f);
				continue;
			}
			float top = r.y - f.y;
			float bottom = f.getBottom() - r.getBottom();
			float left = r.x - f.x;
			float right = f.getRight() - r.getRight();
			if (top > 0) out.push_back(ofRectangle(f.x, f.y, f.width, top));
			if (bottom > 0) out.push_back(ofRectangle(f.x, r.getBottom(), f.width, bottom));
			if (left > 0) out.push_back(ofRectangle(f.x, f.y, left, f.height));
			if (right > 0) out.push_back(ofRectangle(r.getRight(), f.y, right, f.height));
		}
		// prune rects contained in another (keep first of exact duplicates)
		rects.clear();
		for (size_t i = 0; i < out.size(); i++) {
			bool redundant = false;
			for (size_t j = 0; j < out.size() && !redundant; j++) {
				if (i == j) continue;
				if (rectContains(out[j], out[i]) && (out[j] != out[i] || j < i)) redundant = true;
			}
			if (!redundant) rects.push_back(out[i]);
		}
	}
};

std::string sliceLetter(int index) {
	return index < 26 ? std::string(1, (char)('A' + index)) : ofToString(index + 1);
}

} // namespace

bool ofxTpPacker::pack(LayoutDef & layout) {
	const std::string tag = "ofxTestPattern";
	const float m = layout.moduleSize;
	auto floorTo = [m](float v) { return m > 0 ? std::floor(v / m) * m : v; };
	auto ceilTo = [m](float v) { return m > 0 ? std::ceil(v / m) * m : v; };

	// free space per output, minus existing slices
	std::vector<FreeSet> freeSets(layout.outputs.size());
	for (size_t i = 0; i < layout.outputs.size(); i++) {
		freeSets[i].init(layout.outputs[i].width, layout.outputs[i].height);
	}
	std::set<std::string> mapped;
	for (auto & sl : layout.slices) {
		mapped.insert(sl.surface);
		for (size_t i = 0; i < layout.outputs.size(); i++) {
			if (layout.outputs[i].name == sl.output) {
				freeSets[i].subtract(ofRectangle(sl.dst.x, sl.dst.y, sl.src.width, sl.src.height));
			}
		}
	}

	bool allOk = true;
	int packedSurfaces = 0;
	for (auto & s : layout.surfaces) {
		if (mapped.count(s.name)) continue;

		std::deque<ofRectangle> regions { ofRectangle(0, 0, s.width, s.height) };
		int pieceCount = 0;
		double unpacked = 0;
		while (!regions.empty()) {
			ofRectangle reg = regions.front();
			regions.pop_front();
			if (reg.width <= 0 || reg.height <= 0) continue;

			// best piece = largest area cut of this region that fits some free rect,
			// module-aligned in both cut size and placement origin
			int bestOut = -1;
			float bestArea = 0, bestW = 0, bestH = 0;
			glm::vec2 bestDst;
			for (size_t i = 0; i < layout.outputs.size(); i++) {
				for (auto & f : freeSets[i].rects) {
					float ax = ceilTo(f.x), ay = ceilTo(f.y);
					float aw = f.getRight() - ax, ah = f.getBottom() - ay;
					if (aw <= 0 || ah <= 0) continue;
					float pw = reg.width <= aw ? reg.width : floorTo(aw);   // only cuts snap
					float ph = reg.height <= ah ? reg.height : floorTo(ah);
					if (pw <= 0 || ph <= 0) continue;
					if (pw * ph > bestArea) {
						bestArea = pw * ph;
						bestOut = (int)i;
						bestW = pw;
						bestH = ph;
						bestDst = { ax, ay };
					}
				}
			}
			if (bestOut < 0) {
				unpacked += (double)reg.width * reg.height;
				for (auto & r : regions) unpacked += (double)r.width * r.height;
				ofLogError(tag) << "autoPack: surface '" << s.name << "': piece "
				    << reg.width << "x" << reg.height << " at (" << reg.x << "," << reg.y
				    << ") fits no output" << (m > 0 ? " with moduleSize " + ofToString(m) : "")
				    << " — " << (long long)unpacked << " px left over, add or enlarge an output";
				break;
			}

			SliceDef sl;
			sl.id = ofToLower(s.name) + "-" + sliceLetter(pieceCount++);
			sl.surface = s.name;
			sl.output = layout.outputs[bestOut].name;
			sl.src = ofRectangle(reg.x, reg.y, bestW, bestH);
			sl.dst = bestDst;
			layout.slices.push_back(sl);
			freeSets[bestOut].subtract(ofRectangle(bestDst.x, bestDst.y, bestW, bestH));

			// guillotine remainder: right strip (piece height), bottom strip (full width)
			if (reg.width - bestW > 0) regions.push_back(ofRectangle(reg.x + bestW, reg.y, reg.width - bestW, bestH));
			if (reg.height - bestH > 0) regions.push_back(ofRectangle(reg.x, reg.y + bestH, reg.width, reg.height - bestH));
		}

		if (unpacked > 0) {
			allOk = false;
		} else if (pieceCount > 0) {
			packedSurfaces++;
			ofLogNotice(tag) << "autoPack: surface '" << s.name << "' -> " << pieceCount << " slice(s)";
		}
	}

	if (packedSurfaces > 0 || !allOk) {
		for (auto & o : layout.outputs) {
			double used = 0;
			for (auto & sl : layout.slices) {
				if (sl.output == o.name) used += (double)sl.src.width * sl.src.height;
			}
			if (used > 0) {
				ofLogNotice(tag) << "autoPack: output '" << o.name << "' fill "
				    << ofToString(used * 100.0 / ((double)o.width * o.height), 1) << "%";
			}
		}
	}
	return allOk;
}
