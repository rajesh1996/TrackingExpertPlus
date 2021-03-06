/**
 * StructureCoreCaptureDevice.cpp
 * Capture device for the Structure Core
 *
 * David Wehr (dawehr@iastate.edu)
 * 2019.05.24
 */

// STL
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>

// Local
#include "StructureCoreCaptureDevice.h"


texpert::StructureCoreCaptureDevice::StructureCoreCaptureDevice()
{
	_distortion = CameraParameters::getDistortions();
	_intrinsic = CameraParameters::getIntrinsic();
	_color_height = -1;
	_color_width = -1;
	_depth_height = -1;
	_depth_width = -1;


	settings.source = ST::CaptureSessionSourceId::StructureCore;
	settings.structureCore.sensorInitializationTimeout = sensorConnectTimeout_ms;
	settings.structureCore.depthEnabled = true;
	settings.structureCore.visibleEnabled = true;
	settings.structureCore.depthResolution = ST::StructureCoreDepthResolution::_640x480;
	settings.structureCore.visibleResolution = ST::StructureCoreVisibleResolution::_640x480;
	settings.structureCore.depthRangeMode = ST::StructureCoreDepthRangeMode::Short;
	//ST::CaptureSessionSettings settings;
 //   settings.source = ST::CaptureSessionSourceId::StructureCore;
 //   settings.structureCore.depthEnabled = true;
 //   settings.structureCore.visibleEnabled = true;
  //   settings.structureCore.infraredEnabled = true;
      settings.structureCore.accelerometerEnabled = true;
	  settings.structureCore.gyroscopeEnabled = true;
 //   settings.structureCore.depthResolution = ST::StructureCoreDepthResolution::VGA;
 //   settings.structureCore.imuUpdateRate = ST::StructureCoreIMUUpdateRate::AccelAndGyro_200Hz;

	settings.structureCore.infraredAutoExposureEnabled = false;
	settings.structureCore.initialInfraredExposure = 0.0033f;
	settings.structureCore.initialInfraredGain = 3.0f;

	_color_height = 480;
	_color_width = 640;
	_depth_height = 480;
	_depth_width = 640;


	// Begin camera callbacks on delegate
	session.setDelegate(&delegate);
	if (!session.startMonitoring(settings)) {
		std::cerr << "Unable to start Structure Core camera" << std::endl;
		return;
	}

	// Wait until sensor ready if using real sensor
	auto start_wait_time = std::chrono::system_clock::now();
	auto end_wait_time = std::chrono::system_clock::now() + std::chrono::milliseconds(sensorConnectTimeout_ms);

	while (!delegate.sensorReady) 
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (std::chrono::system_clock::now() > end_wait_time) {
			std::cerr << "Timeout while waiting for sensor to connect" << std::endl;
			return;
		}
	}
	
	session.startStreaming();

	//std::unique_lock<std::mutex> lock(delegate.frameMutex);

	// Wait until there are valid frames
	while (!delegate.colorValid && !delegate.depthValid) {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}



	// Get the minimum and maximum depth ranges
	// float minDepthMm = 0.1, maxDepthMm = 10.0;
	//settings.minMaxDepthInMmOfDepthRangeMode(
	//	settings.structureCore.depthRangeMode,  // ST::StructureCoreDepthRangeMode::Default
	//	minDepthMm,
	//	maxDepthMm 
	//);

}

texpert::StructureCoreCaptureDevice::~StructureCoreCaptureDevice() {
	session.stopStreaming();

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	
}

void texpert::StructureCoreCaptureDevice::getRGBFrame( cv::Mat &mFrame)
{
	delegate.getLastColorFrame(mFrame);
	return;
}

void texpert::StructureCoreCaptureDevice::getDepthFrame( cv::Mat &mFrame)
{
	delegate.getLastDepthFrame(mFrame);
	return;
}


bool texpert::StructureCoreCaptureDevice::isOpen()
{
	return (delegate.depthValid && delegate.colorValid);
}


/*
Set a callback to be invoked as soon as a frame arrives
@param cb - function pointer for a callback. 
*/
void texpert::StructureCoreCaptureDevice::setCallbackPtr(std::function<void()> cb)
{
	delegate.callback_function = cb;
}



/*
Return the number of image rows in pixel
@param c - the requested camera component. 
@return - number of image rows in pixel. -1 if the component does not exist.
*/
int texpert::StructureCoreCaptureDevice::getRows(CaptureDeviceComponent c)
{
	switch (c) {
		case CaptureDeviceComponent::COLOR:
			return _color_height;
			break;
		case CaptureDeviceComponent::DEPTH:
			return _depth_height;
	}
	return -1;
}

/*
Return the number of image colums in pixel
@param c - the requested camera component. 
@return - number of image columns in pixel. -1 if the component does not exist.
*/
int texpert::StructureCoreCaptureDevice::getCols(CaptureDeviceComponent c)
{
	switch (c) {
		case CaptureDeviceComponent::COLOR:
			return _color_width;
			break;
		case CaptureDeviceComponent::DEPTH:
			return _depth_width;
	}
	return -1;
}


void texpert::StructureCoreCaptureDevice::SessionDelegate::captureSessionEventDidOccur(ST::CaptureSession *session, ST::CaptureSessionEventId event) {
	switch (event) {
		case ST::CaptureSessionEventId::Booting: break;
		case ST::CaptureSessionEventId::Ready:
			sensorReady = true;
			break;
		case ST::CaptureSessionEventId::Disconnected:
		case ST::CaptureSessionEventId::Error:
			printf("Capture session error\n");
			exit(1);
			break;
		case ST::CaptureSessionEventId::EndOfFile:
			// When streaming and looping, will reach end of file. No problem.
			break;
		default:
			std::cout << "Capture session event unhandled: " << ST::CaptureSessionSample::toString(event);
	}
}

void texpert::StructureCoreCaptureDevice::SessionDelegate::captureSessionDidOutputSample(ST::CaptureSession *, const ST::CaptureSessionSample& sample) {
	// Obtain lock so latestColorFrame and latestDepthFrame are not modified while being accessed elsewhere

	frame_couter++;

	switch (sample.type) {
		case ST::CaptureSessionSample::Type::DepthFrame:
			 //printf("Depth frame: size %dx%d\n", sample.depthFrame.width(), sample.depthFrame.height());
			 if (sample.depthFrame.isValid()) {
				latestDepthFrame = sample.depthFrame;
				copyDepthToMat();
				depthValid = true;
			}
			break;
		case ST::CaptureSessionSample::Type::VisibleFrame:
			//printf("Visible frame: size %dx%d\n", sample.visibleFrame.width(), sample.visibleFrame.height());
			if (sample.visibleFrame.isValid()) {
				latestColorFrame = sample.visibleFrame;
				copyColorToMat();
				colorValid = true;
			}
			break;
		case ST::CaptureSessionSample::Type::SynchronizedFrames:
			//printf("Synchronized frames: depth %dx%d visible %dx%d infrared %dx%d\n", sample.depthFrame.width(), sample.depthFrame.height(), sample.visibleFrame.width(), sample.visibleFrame.height(), sample.infraredFrame.width(), sample.infraredFrame.height());

			if (sample.visibleFrame.isValid()) {
				latestColorFrame = sample.visibleFrame;
				copyColorToMat();
				colorValid = true;
			}
			if (sample.depthFrame.isValid()) {
				latestDepthFrame = sample.depthFrame;
				copyDepthToMat();
				depthValid = true;
			}
			break;
		default:
			//printf("Structure Core sample type unhandled\n");
			break;
	}

	if (++frame_couter % 2 == 0) {
		if(this->callback_function != NULL) callback_function();
	}

}

void texpert::StructureCoreCaptureDevice::SessionDelegate::getLastColorFrame(cv::Mat& mFrame) 
{
	frameMutex.lock();
	mFrame = colorFrame;
	frameMutex.unlock();
}

void texpert::StructureCoreCaptureDevice::SessionDelegate::getLastDepthFrame(cv::Mat& mFrame) {
	depthMutex.lock();
	mFrame = depthFrame;
	depthMutex.unlock();
}


void texpert::StructureCoreCaptureDevice::SessionDelegate::copyColorToMat()
{
	frameMutex.lock();

	cv::Mat cameraFrame(latestColorFrame.height(), latestColorFrame.width(), CV_8UC3, (void*)latestColorFrame.rgbData());
	cv::cvtColor(cameraFrame, colorFrame, cv::COLOR_RGB2BGR);

	frameMutex.unlock();
}
			
void texpert::StructureCoreCaptureDevice::SessionDelegate::copyDepthToMat()
{
	depthMutex.lock();

	cv::Mat cameraFrame(latestDepthFrame.height(), latestDepthFrame.width(), CV_32F, (void*)latestDepthFrame.depthInMillimeters());
	depthFrame = cameraFrame.clone();

	depthMutex.unlock();
}



/*!
Read camera parameters for the depth camera from a file.
@param path_and_file - string with a relative or absolute path pointing to the 
camera parameters
*/
bool  texpert::StructureCoreCaptureDevice::readCameraParameters(std::string path_and_file, bool verbose)
{
	bool ret = CameraParameters::Read(path_and_file, verbose);

	if (ret) {
		_intrinsic = CameraParameters::getIntrinsic();
		_distortion = CameraParameters::getDistortions();
	}	
	return ret;
}


/*!
Return the intrinsic camera parameters
@return 3x3 cv::Mat with
	[ fx 0 cx ]
	[ 0 fy cy ]
	[ 0 0  1  ]
*/
cv::Mat& texpert::StructureCoreCaptureDevice::getCameraParam(void)
{
	return _intrinsic;
}