#include <iostream>
#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>

void main() {

	rs2::colorizer c;
	rs2::pipeline p;
	rs2::config cfg;
	cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);
	cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);

	p.start(cfg);

	while (true) {
		rs2::frameset frames = p.wait_for_frames();

		rs2::align align(RS2_STREAM_COLOR);
		auto aligned_frames = align.process(frames);

		rs2::frame color = frames.get_color_frame();
		rs2::frame depth = aligned_frames.get_depth_frame();

		if (!depth) continue;

		cv::Mat color_image(cv::Size(640, 480), CV_8UC3, (void*)color.get_data(), cv::Mat::AUTO_STEP);
		cv::Mat depth_image(cv::Size(640, 480), CV_8UC3, (void*)depth.get_data(), cv::Mat::AUTO_STEP);

		cv::imshow("color", color_image);
		cv::imshow("depth", depth_image);

		int key = cv::waitKey(1);
		if (key == 113) {
			break;
		}
	}

	cv::destroyAllWindows();
}