// hand_motion_tracking.cpp : 簡易的な手の動きの検出を行う

#include "stdafx.h"
#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>	
#include <iostream>
#include <Windows.h>

//取得する画像サイズ
const static int IMG_WIDTH = 640;
const static int IMG_HEIGHT = 480;

//背景除去を行う距離
const static float CLIPPING_DISTANCE = 1.0f;

//モーション判定をするフレーム数
const static int NUM_JUDGE_FRAMES = 5;

//どれだけの移動で検知するかの閾値
const static int THRESHOLD_X = 10;
const static int THRESHOLD_Y = 10;
const static float THRESHOLD_Z = 0.015f;

//モーション
const static int NO_MOTION = 0;
const static int PULL = 1;
const static int PUSH = 2;
const static int LEFT = 3;
const static int RIGHT = 4;
const static int DOWN = 5;
const static int UP = 6;

using namespace std;

//手の位置情報を格納する構造体
typedef struct HandPosition {
	float dist;
	int pos_x;
	int pos_y;
} HandPosition;

//デプススケールの取得
float getDepthScale(rs2::device dev) {

	for (rs2::depth_sensor sensor : dev.query_sensors()) {
		if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>()) {
			return dpt.get_depth_scale();
		}
	}
	throw runtime_error("Device does not have a depth sensor");
}

//画像の背景除去
void removeBackground(rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame, float depth_scale) {

	//デプスデータを16ビット長の無符号整数型のポインタに変換
	const uint16_t* p_depth_frame = reinterpret_cast<const uint16_t*>(depth_frame.get_data());
	//他のデータ(今回はカラーデータ)を8ビット長の無符号整数型のポインタに変換
	uint8_t* p_other_frame = reinterpret_cast<uint8_t*>(const_cast<void*>(other_frame.get_data()));

	//高さ、幅、ピクセル辺りのバイト数を取得
	int width = other_frame.get_width();
	int height = other_frame.get_height();
	int other_bpp = other_frame.get_bytes_per_pixel();

//openMPを用いて、for文のスレッド処理を行う
#pragma omp parallel for schedule(dynamic)
	for (int y = 0; y < height; y++) {
		auto depth_pixel_index = y * width;
		for (int x = 0; x < width; x++, ++depth_pixel_index) {
			//現在のピクセルの深度データを取得する
			//デプススケールをかけることで距離の単位を直す
			auto pixels_distance = depth_scale * p_depth_frame[depth_pixel_index];

			//距離データか無効か、閾値以上かチェック
			if (pixels_distance <= 0.f || pixels_distance > CLIPPING_DISTANCE) {
				//カラーフレームのオフセットを計算する
				auto offset = depth_pixel_index * other_bpp;

				//ピクセルをグレーにセット
				//メモリブロックに計算したオフセットからピクセルのバイト分、0x99をセット
				memset(&p_other_frame[offset], 0x99, other_bpp);
			}
		}
	}
}

//カスケード分類器と深度情報による手の検出を行う
HandPosition detectHandInImage(cv::Mat &image, std::string &cascade_file, rs2::depth_frame &depth) {

	//カスケード分類器による手の検出
	cv::CascadeClassifier cascade;
	cascade.load(cascade_file);
	vector<cv::Rect> hands;
	cascade.detectMultiScale(image, hands, 1.2, 3, 0, cv::Size(30, 30));

	//検出したオブジェクトの中で最も近いものを判定
	int nearest_hand = 0;
	float nearest_dist = 100.0;

	if (hands.size() != NULL) {
		for (int i = 0; i < hands.size(); i++) {
			if (hands.size() != 0 && depth) {
				float tmp = depth.get_distance(hands[i].x + (hands[i].width / 2), hands[i].y + (hands[i].height / 2));
				if (nearest_dist > tmp) {
					nearest_dist = tmp;
					nearest_hand = i;
				}
			}
		}

		if (hands.size() != 0 && nearest_dist <= CLIPPING_DISTANCE) {
			//一番近いもののみ囲う
			rectangle(image, cv::Point(hands[nearest_hand].x, hands[nearest_hand].y),
							cv::Point(hands[nearest_hand].x + hands[nearest_hand].width, hands[nearest_hand].y
							+ hands[nearest_hand].height), cv::Scalar(0, 200, 0), 3, CV_AA);
		}

		HandPosition hand_pos = { 
			nearest_dist,
			hands[nearest_hand].x + (hands[nearest_hand].width / 2),
			hands[nearest_hand].y + (hands[nearest_hand].height / 2) 
		};

		return hand_pos;
	}
}

//X軸方向のモーションを検出する
int detectHandMoved_X(HandPosition hand_pos) {

	//X軸の座標情報を保持するリスト
	static vector<int> horizon_vec;

	//距離の情報を追加
	horizon_vec.push_back(hand_pos.pos_x);

	//指定フレーム分溜まっていなければ処理は終了
	if (horizon_vec.size() <= NUM_JUDGE_FRAMES) return 0;

	//一番古い要素を削除
	horizon_vec.erase(horizon_vec.begin());

	//フレームごとの変化量を取得
	int motion_flg = 0;
	for (int i = 0; i < horizon_vec.size() - 1; i++) {
		if (horizon_vec[i + 1] - horizon_vec[i] > THRESHOLD_X) {
			motion_flg++;
		} else if (horizon_vec[i + 1] - horizon_vec[i] < -THRESHOLD_X) {
			motion_flg--;
		} else {
			return NO_MOTION;
		}
	}

	//全て一定以上の変化量であればモーションあり
	//保持フレームを空にする
	horizon_vec.clear();

	if (motion_flg == NUM_JUDGE_FRAMES - 1) {
		cout << "LEFT" << endl;
		return LEFT;
	} else if (motion_flg == -(NUM_JUDGE_FRAMES - 1)) {
		cout << "RIGHT" << endl;
		return RIGHT;
	}

	return NO_MOTION;
}

//Y軸方向のモーションを検出する
int detectHandMoved_Y(HandPosition hand_pos) {

	//Y軸の座標情報を保持するリスト
	static vector<int> vertical_vec;

	//距離の情報を追加
	vertical_vec.push_back(hand_pos.pos_y);

	//指定フレーム分溜まっていなければ処理は終了
	if (vertical_vec.size() <= NUM_JUDGE_FRAMES) return 0;

	//一番古い要素を削除
	vertical_vec.erase(vertical_vec.begin());

	//フレームごとの変化量を取得
	int motion_flg = 0;
	for (int i = 0; i < vertical_vec.size() - 1; i++) {
		if (vertical_vec[i + 1] - vertical_vec[i] > THRESHOLD_Y) {
			motion_flg++;
		} else if (vertical_vec[i + 1] - vertical_vec[i] < -THRESHOLD_Y) {
			motion_flg--;
		} else {
			return NO_MOTION;
		}
	}

	//全て一定以上の変化量であればモーションあり
	//保持フレームを空にする
	vertical_vec.clear();

	if (motion_flg == NUM_JUDGE_FRAMES - 1) {
		cout << "DOWN" << endl;
		return DOWN;
	} else if (motion_flg == -(NUM_JUDGE_FRAMES - 1)) {
		cout << "UP" << endl;
		return UP;
	}

	return NO_MOTION;
}

//Z軸方向のモーションを検出する
int detectHandMoved_Z(HandPosition hand_pos) {

	//距離情報を保持するリスト
	static vector<float> dists_vec;

	//距離の情報を追加
	dists_vec.push_back(hand_pos.dist);

	//指定フレーム分溜まっていなければ処理は終了
	if (dists_vec.size() <= NUM_JUDGE_FRAMES) return 0;

	//一番古い要素を削除
	dists_vec.erase(dists_vec.begin());

	//フレームごとの変化量を取得
	int motion_flg = 0;
	for (int i = 0; i < dists_vec.size() - 1; i++) {
		if (dists_vec[i + 1] - dists_vec[i] > THRESHOLD_Z) {
			motion_flg++;
		} else if (dists_vec[i + 1] - dists_vec[i] < -THRESHOLD_Z) {
			motion_flg--;
		} else {
			return NO_MOTION;
		}
	}

	//全て一定以上の変化量であればモーションあり
	//保持フレームを空にする
	dists_vec.clear();

	if (motion_flg == NUM_JUDGE_FRAMES - 1) {
		cout << "PULL" << endl;
		return PULL;
	} else if (motion_flg == -(NUM_JUDGE_FRAMES - 1)) {
		cout << "PUSH" << endl;
		return PUSH;
	}

	return NO_MOTION;
}

//名前付きパイプによるプロセス間通信を行う
void sendMessageToServer(HANDLE pipe_handle, int message) {
	if (message == 0) return;

	string send_mes;
	switch (message) {
	case 0:
		break;
	case 1:
		send_mes = "pull";
		break;
	case 2:
		send_mes = "push";
		break;
	case 3:
		send_mes = "left";
		break;
	case 4:
		send_mes = "right";
		break;
	case 5:
		send_mes = "down";
		break;
	case 6:
		send_mes = "up";
		break;
	default:
		break;
	}

	const char* buff = send_mes.c_str();
	DWORD dw_bytes_written;
	if (!WriteFile(pipe_handle, buff, strlen(buff), &dw_bytes_written, NULL)) {
		cout << "送信に失敗" << endl;
	}
}

int main() {

	//カスケードファイルの用意
	string cascadefile = "aGest.xml";
	
	//パイプラインの作成
	rs2::pipeline pipe;

	//プロセス間通信用のパイプへ接続する
	HANDLE pipe_handle = CreateFile(L"\\\\.\\pipe\\mypipe", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pipe_handle == INVALID_HANDLE_VALUE) {
		cout << "パイプへの接続に失敗しました" << endl;
	}

	//キャプチャするフレームの設定
	rs2::config cfg;
	cfg.enable_stream(RS2_STREAM_COLOR, IMG_WIDTH, IMG_HEIGHT, RS2_FORMAT_BGR8, 30);
	cfg.enable_stream(RS2_STREAM_DEPTH, IMG_WIDTH, IMG_HEIGHT, RS2_FORMAT_Z16, 30);

	//後処理用のフィルタ設定
	rs2::decimation_filter dec_filter;	//サンプリング周波数の低減
	//dec_filter.set_option(RS2_OPTION_FILTER_MAGNITUDE, 2.0f);

	rs2::spatial_filter spa_filter;		//画像の平滑化
	//spa_filter.set_option(RS2_OPTION_FILTER_MAGNITUDE, 2.0f);
	//spa_filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.5f);
	//spa_filter.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA, 20.0f);
	spa_filter.set_option(RS2_OPTION_HOLES_FILL, 1);

	rs2::temporal_filter tem_filter;	//時間的なノイズの低減
	//tem_filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.4f);
	//tem_filter.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA, 20.0f);
	tem_filter.set_option(RS2_OPTION_HOLES_FILL, 3);

	rs2::hole_filling_filter hole_filter;
	hole_filter.set_option(RS2_OPTION_HOLES_FILL, 1);

	//ストリーミングの開始
	rs2::pipeline_profile profile = pipe.start(cfg);

	//デプススケールの取得
	rs2::device device = profile.get_device();
	float depth_scale = getDepthScale(device);

	//プリセットの設定(あまり効果なさそう)
	//auto sensors = device.query_sensors(); //センサー情報の取得
	//for (rs2::sensor sensor : sensors) {
	//	string tmp = sensor.get_info(RS2_CAMERA_INFO_NAME);
	//	if (tmp == "Stereo Module") {
	//		sensor.set_option(RS2_OPTION_VISUAL_PRESET, 2);	//Handを指定
	//	}
	//}

	while (true) {
		//カメラ映像のキャプチャ
		rs2::frameset frames = pipe.wait_for_frames();

		//デプスフレームをRGBフレームに合わせる
		rs2::align align(RS2_STREAM_COLOR);
		auto aligned_frames = align.process(frames);

		//RGBフレームを取得
		rs2::video_frame color = frames.get_color_frame();
		//デプスフレームを取得
		rs2::depth_frame depth = aligned_frames.get_depth_frame();

		//フィルタを適用(デシメーションフィルタのみ例外が発生する)
		rs2::depth_frame filtered_depth = depth;
		//filtered_depth = dec_filter.process(filtered_depth);
		filtered_depth = spa_filter.process(filtered_depth);
		filtered_depth = tem_filter.process(filtered_depth);
		//filtered_depth = hole_filter.process(filtered_depth);

		//取得した画像の背景除去
		removeBackground(color, filtered_depth, depth_scale);

		//キャプチャ画像をフレームに代入
		cv::Mat frame(cv::Size(640, 480), CV_8UC3, (void*)color.get_data(), cv::Mat::AUTO_STEP);

		//フレームデータを用いて手の検出
		HandPosition hand_pos = detectHandInImage(frame, cascadefile, depth);

		//モーションの検出
		int message = NO_MOTION;
		if (message == NO_MOTION) {
			message = detectHandMoved_X(hand_pos);
		}
		if (message == NO_MOTION) {
			message = detectHandMoved_Y(hand_pos);
		}
		if (message == NO_MOTION) {
			message = detectHandMoved_Z(hand_pos);
		}

		//結果を通知
		sendMessageToServer(pipe_handle, message);

		cv::imshow("window", frame);

		//qを押すと終了
		int key = cv::waitKey(1);
		if (key == 113) break;
	}

	cv::destroyAllWindows();
	CloseHandle(pipe_handle);

	return 0;
}