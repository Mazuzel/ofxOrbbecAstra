//
//  ofxOrbbecAstra.h
//  ofxOrbbecAstra
//
//  Created by Matt Felsen on 10/24/15.
//
//

#include "ofxOrbbecAstra.h"


ofxOrbbecAstra::ofxOrbbecAstra() {
    cameraWidth = 640;
    cameraHeight = 480;
	nearClip = 300;
	farClip = 1800;
	maxDepth = 8000;

	bSetup = false;
	bIsFrameNew = false;
	bDepthImageEnabled = true;
	frameRate = 0.f;
}

ofxOrbbecAstra::~ofxOrbbecAstra() {
    if(reader.is_valid()) {

        reader.remove_listener(*this);
        astra::terminate();
    }
}

void ofxOrbbecAstra::setLicenseString(const string& license) {
    #ifndef TARGET_OSX
    orbbec_body_tracking_set_license(license.c_str());
    #endif
}

void ofxOrbbecAstra::setup() {
	setup("device/default");
}

void ofxOrbbecAstra::setup(const string& uri) {
    colorImage.allocate(cameraWidth, cameraHeight, OF_IMAGE_COLOR);
    depthImage.allocate(cameraWidth, cameraHeight, OF_IMAGE_GRAYSCALE);
    depthPixels.allocate(cameraWidth, cameraHeight, OF_IMAGE_GRAYSCALE);
    cachedCoords.resize(cameraWidth * cameraHeight);
	updateDepthLookupTable();

	astra_status_t status = astra::initialize();
	if (status != ASTRA_STATUS_SUCCESS) {
		ofLogError() << "Failed to initialize Astra camera, status id:  " << status;
	}
	else {
		astra_version_info_t info;
		astra_status_t s = astra_version(&info);
		ofLogNotice() << "Astra SDK successfully initialized.";
		ofLogNotice() << "Astra SDK version: " << info.friendlyVersionString;
	}


	streamset = astra::StreamSet(uri.c_str());
	reader = astra::StreamReader(streamset.create_reader());

	bSetup = true;
	reader.add_listener(*this);
}

void ofxOrbbecAstra::enableDepthImage(bool enable) {
	bDepthImageEnabled = enable;
}

void ofxOrbbecAstra::enableRegistration(bool useRegistration) {
	if (!bSetup) {
		ofLogWarning("ofxOrbbecAstra") << "Must call setup() before setRegistration()";
		return;
	}

	reader.stream<astra::DepthStream>().enable_registration(useRegistration);
}

void ofxOrbbecAstra::setDepthClipping(unsigned short _near, unsigned short _far) {
	nearClip = _near;
	farClip = _far;
	updateDepthLookupTable();
}

void ofxOrbbecAstra::initColorStream() {
	if (!bSetup) {
		ofLogWarning("ofxOrbbecAstra") << "Must call setup() before initColorStream()";
		return;
	}

    astra::ColorStream colorStream = reader.stream<astra::ColorStream>();
    bool isAvailable = colorStream.is_available();
    if(!isAvailable) {
        ofLogWarning("ofxOrbbecAstra") << "Colour stream is not available. Check if sensor is plugged in.";
        return;
    }

	astra::ImageStreamMode colorMode;

    colorMode.set_width(cameraWidth);
    colorMode.set_height(cameraHeight);
	colorMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_RGB888);
	colorMode.set_fps(30);
    ofImage colorImage;

	colorStream.set_mode(colorMode);
	colorStream.start();
}

void ofxOrbbecAstra::initDepthStream() {
	if (!bSetup) {
		ofLogWarning("ofxOrbbecAstra") << "Must call setup() before initDepthStream()";
		return;
	}

    astra::DepthStream depthStream = reader.stream<astra::DepthStream>();
    bool isAvailable = depthStream.is_available();
    if(!isAvailable) {
        ofLogWarning("ofxOrbbecAstra") << "Depth stream is not available. Check if sensor is plugged in.";
        return;
    }

	astra::ImageStreamMode depthMode;

    depthMode.set_width(cameraWidth);
    depthMode.set_height(cameraHeight);
	depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
	depthMode.set_fps(30);

	depthStream.set_mode(depthMode);
	depthStream.start();
}

void ofxOrbbecAstra::initPointStream() {
	if (!bSetup) {
		ofLogWarning("ofxOrbbecAstra") << "Must call setup() before initPointStream()";
		return;
	}

	reader.stream<astra::PointStream>().start();
}

void ofxOrbbecAstra::initHandStream() {
	if (!bSetup) {
		ofLogWarning("ofxOrbbecAstra") << "Must call setup() before initHandStream()";
		return;
	}

	reader.stream<astra::HandStream>().start();
}


void ofxOrbbecAstra::initBodyStream() {
    if (!bSetup) {
        ofLogWarning("ofxOrbbecAstra") << "Must call setup() before initBodyStream()";
        return;
    }
#ifndef TARGET_OSX
   reader.stream<astra::BodyStream>().start();
   astra::SkeletonOptimization optimization = reader.stream<astra::BodyStream>().get_skeleton_optimization();
   ofLogNotice() << "Skeleton Optimization (1-9) = " << static_cast<int>(optimization);

   astra::SkeletonProfile profile = reader.stream<astra::BodyStream>().get_skeleton_profile();
   if(profile == astra::SkeletonProfile::Basic) ofLogNotice() << "Skeleton Profile = Only four basic joints: Head, MidSpine, LeftHand, RightHand";
   else if(profile == astra::SkeletonProfile::UpperBody) ofLogNotice() << "Skeleton Profile = Upper body only";
   else if(profile == astra::SkeletonProfile::Full) ofLogNotice() << "Skeleton Profile = All supported joints";

#endif
}



void ofxOrbbecAstra::initVideoGrabber(int deviceID) {
	bUseVideoGrabber = true;

	grabber = make_shared<ofVideoGrabber>();
	grabber->setDeviceID(deviceID);
    grabber->setup(cameraWidth, cameraHeight);
}

void ofxOrbbecAstra::update(){
	// See on_frame_ready() for more processing
	bIsFrameNew = false;
#ifndef TARGET_OSX
    astra_update();
#else
    astra_temp_update();
#endif

	if (bUseVideoGrabber && grabber) {
		grabber->update();
		if (grabber->isFrameNew()) {
			colorImage.setFromPixels(grabber->getPixels());
			colorImage.mirror(false, true);
			colorImage.update();
		}
	}
}

void ofxOrbbecAstra::draw(float x, float y, float w, float h){
    if (!w) w = cameraWidth;
    if (!h) h = cameraHeight;
	colorImage.draw(x, y, w, h);
}

void ofxOrbbecAstra::drawDepth(float x, float y, float w, float h){
    if (!w) w = cameraWidth;
    if (!h) h = cameraHeight;
	depthImage.draw(x, y, w, h);
}

bool ofxOrbbecAstra::isFrameNew() {
	return bIsFrameNew;
}

void ofxOrbbecAstra::on_frame_ready(astra::StreamReader& reader,
									astra::Frame& frame)
{
	bIsFrameNew = true;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	frameRate		= 1000.f / (std::chrono::duration<double, std::milli>(now - lastFrameTime).count());
	lastFrameTime	= now;

	auto colorFrame = frame.get<astra::ColorFrame>();
    auto depthFrame = frame.get<astra::DepthFrame>();
	auto pointFrame = frame.get<astra::PointFrame>();
	auto handFrame = frame.get<astra::HandFrame>();
#ifndef TARGET_OSX
    auto bodyFrame = frame.get<astra::BodyFrame>();
#endif
    
	if (colorFrame.is_valid()) {
		colorFrame.copy_to((astra::RgbPixel*) colorImage.getPixels().getData());
		colorImage.update();
	}

	if (depthFrame.is_valid()) {
		depthFrame.copy_to((short*) depthPixels.getData());

		if (bDepthImageEnabled) {
			// TODO do this with a shader so it's fast?
			for (int i = 0; i < depthPixels.size(); i++) {
				short depth = depthPixels.getColor(i).r;
				depth = ofClamp(depth, 0, maxDepth-1);
				float val = depthLookupTable[depth];
				depthImage.setColor(i, ofColor(val));
			}
			depthImage.update();
		}
	}

	if (pointFrame.is_valid()) {
		pointFrame.copy_to((astra::Vector3f*) cachedCoords.data());
	}

	if (handFrame.is_valid()) {
		handMapDepth.clear();
		handMapWorld.clear();
		auto& list = handFrame.handpoints();

		for (auto& handPoint : list) {
			const auto& id = handPoint.tracking_id();

			if (handPoint.status() == HAND_STATUS_TRACKING) {
				const auto& depthPos = handPoint.depth_position();
				const auto& worldPos = handPoint.world_position();

				handMapDepth[id] = ofDefaultVec2(depthPos.x, depthPos.y);
				handMapWorld[id] = ofDefaultVec3(worldPos.x, worldPos.y, worldPos.z);
			} else {
				handMapDepth.erase(id);
				handMapWorld.erase(id);
			}
		}
	}

#ifndef TARGET_OSX
    if (bodyFrame.is_valid()) {
        joints.clear();

        const auto& bodies = bodyFrame.bodies();
        numBodies = bodies.size();

        size_t id = 0;
        for (auto& body : bodies)
        {
            //const auto& id = body.id();
            vector<astra::Joint> newjoints;
            joints.push_back(newjoints);
            for(auto& joint : body.joints())
            {
                joints[id].push_back(joint);
            }
            id++;
        }
    } else {
        numBodies = 0;
    }
#endif
}

void ofxOrbbecAstra::updateDepthLookupTable() {
	// From product specs, range is 8m
	maxDepth = 8000;
	depthLookupTable.resize(maxDepth);

	// Depth values of 0 should be discarded, so set the LUT value to 0 as well
	depthLookupTable[0] = 0;

	// Set the rest
	for (int i = 1; i < maxDepth; i++) {
		depthLookupTable[i] = ofMap(i, nearClip, farClip, 255, 0, true);
	}

}

ofDefaultVec3 ofxOrbbecAstra::getWorldCoordinateAt(int x, int y) {
    return cachedCoords[int(y) * cameraWidth + int(x)];
}

#ifndef TARGET_OSX
int ofxOrbbecAstra::getNumBodies() {
    if(numBodies == 0) return 0;
    return numBodies;
}

int ofxOrbbecAstra::getNumJoints(int body_id) {
    if(numBodies == 0) return 0;
    if(body_id > (numBodies-1)) return 0;
    return joints.at(body_id).size();
}

vector<astra::Joint>& ofxOrbbecAstra::getJointPositions(int body_id) {
    return joints.at(body_id);
}

ofDefaultVec2 ofxOrbbecAstra::getNomalisedJointPosition(int body_id, int joint_id) {
    return ofDefaultVec2(joints[body_id][joint_id].depth_position().x/cameraWidth,joints[body_id][joint_id].depth_position().y/cameraHeight);
}

ofDefaultVec2 ofxOrbbecAstra::getJointPosition(int body_id, int joint_id) {
    return ofDefaultVec2(joints[body_id][joint_id].depth_position().x/cameraWidth*ofGetWidth(),joints[body_id][joint_id].depth_position().y/cameraHeight*ofGetHeight());
}

astra::JointType ofxOrbbecAstra::getJointType(int body_id, int joint_id) {
    return joints[body_id][joint_id].type();
}

string ofxOrbbecAstra::getJointName(astra::JointType id) {
    switch(static_cast<int>(id))
    {
        case 0:
            return "Head";
        case 1:
            return "ShoulderSpine";
        case 2:
            return "LeftShoulder";
        case 3:
            return "LeftElbow";
        case 4:
            return "LeftHand";
        case 5:
            return "RightShoulder";
        case 6:
            return "RightElbow";
        case 7:
            return "RightHand";
        case 8:
            return "MidSpine";
        case 9:
            return "BaseSpine";
        case 10:
            return "LeftHip";
        case 11:
            return "LeftKnee";
        case 12:
            return "LeftFoot";
        case 13:
            return "RightHip";
        case 14:
            return "RightKnee";
        case 15:
            return "RightFoot";
        case 16:
            return "LeftWrist";
        case 17:
            return "RightWrist";
        case 18:
            return "Neck";
        case 255:
        default:
            return "Unknown";
    }
}

#endif

unsigned short ofxOrbbecAstra::getNearClip() {
	return nearClip;
}

unsigned short ofxOrbbecAstra::getFarClip() {
	return farClip;
}

ofShortPixels& ofxOrbbecAstra::getRawDepth() {
	return depthPixels;
}

ofImage& ofxOrbbecAstra::getDepthImage() {
	return depthImage;
}

ofImage& ofxOrbbecAstra::getColorImage() {
	return colorImage;
}

vector<ofDefaultVec3>& ofxOrbbecAstra::getPoints()
{
	return cachedCoords;
}

map<int32_t,ofDefaultVec2>& ofxOrbbecAstra::getHandsDepth() {
	return handMapDepth;
}

map<int32_t,ofDefaultVec3>& ofxOrbbecAstra::getHandsWorld() {
	return handMapWorld;
}
