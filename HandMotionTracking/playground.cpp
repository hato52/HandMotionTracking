//実験用環境

#include "stdafx.h"
#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>		//基本的な関数群
#include <librealsense2/rsutil.h>	//マッピング用の関数群
#include <iostream>


using namespace std;
using namespace cv;

int main()
{
	//Contruct a pipeline which abstracts the device
	rs2::pipeline pipe;

	//Create a configuration for configuring the pipeline with a non default profile
	rs2::config cfg;

	//Add desired streams to configuration
	cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);
	cfg.enable_stream(RS2_STREAM_INFRARED, 640, 480, RS2_FORMAT_Y8, 30);
	cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);

	//Instruct pipeline to start streaming with the requested configuration
	pipe.start(cfg);

	// Camera warmup - dropping several first frames to let auto-exposure stabilize
	rs2::frameset frames;

	while (true) {
		//Wait for all configured streams to produce a frame
		frames = pipe.wait_for_frames();

		//align処理
		rs2::align align(RS2_STREAM_COLOR);
		auto aligned_frames = align.process(frames);

		//Get each frame
		rs2::frame color_frame = frames.get_color_frame();
		//rs2::frame ir_frame = aligned_frames.get_infrared_frame();
		rs2::depth_frame depth_frame = aligned_frames.get_depth_frame();

		// Creating OpenCV matrix from IR image
		Mat color(Size(640, 480), CV_8UC3, (void*)color_frame.get_data(), Mat::AUTO_STEP);
		Mat depth(Size(640, 480), CV_8UC1, (void*)depth_frame.get_data(), Mat::AUTO_STEP);

		// Apply Histogram Equalization
		//equalizeHist(ir, i);
		//applyColorMap(ir, ir, COLORMAP_JET);

		// Display the image in GUI
		//namedWindow("IR Image", WINDOW_AUTOSIZE);
		namedWindow("Color Image", WINDOW_AUTOSIZE);
		//imshow("IR Image", ir);
		imshow("Depth Image", depth);
		imshow("Color Image", color);

		//qを押すと終了
		int key = cv::waitKey(1);
		if (key == 113) {
			break;
		}
	}
		
	return 0;
}