#pragma once

#include "ofMain.h"
#include "ofxOrbbecAstra.h"

class ofApp : public ofBaseApp{

public:

	void setup();
	void update();
	void draw();

	void keyPressed(int key);

	ofxOrbbecAstra astra;



};
