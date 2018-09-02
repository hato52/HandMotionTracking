#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>

using namespace std;

int main() {
	//デバイスを抽象化するパイプラインを構築
	rs2::pipeline pipe;

	//パイプラインの設定
	rs2::config cfg;

	//明示的にストリームを有効化する
	//640×480のカラーストリーム画像を、BGR8フォーマット、30fpsで取得するように設定
	cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);

	//設定を適用してパイプラインストリーミングを開始
	pipe.start(cfg);

	while (true) {
		//ストリームがフレームセットを取得するまで待機
		rs2::frameset frames = pipe.wait_for_frames();

		//フレームセットからカラーフレームを取得
		rs2::frame color = frames.get_color_frame();

		//カラーフレームから、OpenCVのMatを作成
		cv::Mat frame(cv::Size(640, 480), CV_8UC3, (void*)color.get_data(), cv::Mat::AUTO_STEP);

		//画像を表示
		cv::imshow("window", frame);

		//qを押すと終了
		int key = cv::waitKey(1);
		if (key == 113) {
			break;
		}
	}

	//全てのウィンドウを閉じる
	cv::destroyAllWindows();

	return 0;
}
