// ------------------------- OpenPose Library Tutorial - Real Time Pose Estimation -------------------------
// If the user wants to learn to use the OpenPose library, we highly recommend to start with the `examples/tutorial_*/` folders.
// This example summarizes all the funcitonality of the OpenPose library:
// 1. Read folder of images / video / webcam  (`producer` module)
// 2. Extract and render body keypoint / heatmap / PAF of that image (`pose` module)
// 3. Extract and render face keypoint / heatmap / PAF of that image (`face` module)
// 4. Save the results on disk (`filestream` module)
// 5. Display the rendered pose (`gui` module)
// Everything in a multi-thread scenario (`thread` module)
// Points 2 to 5 are included in the `wrapper` module
// In addition to the previous OpenPose modules, we also need to use:
// 1. `core` module:
// For the Array<float> class that the `pose` module needs
// For the Datum struct that the `thread` module sends between the queues
// 2. `utilities` module: for the error & logging functions, i.e. op::error & op::log respectively
// This file should only be used for the user to take specific examples.

// C++ std library dependencies
#include <chrono> // `std::chrono::` functions and classes, e.g. std::chrono::milliseconds
#include <thread> // std::this_thread
// Other 3rdparty dependencies
// GFlags: DEFINE_bool, _int32, _int64, _uint64, _double, _string
#include <gflags/gflags.h>
// Allow Google Flags in Ubuntu 14
#ifndef GFLAGS_GFLAGS_H_
namespace gflags = google;
#endif
// OpenPose dependencies
#include <openpose/headers.hpp>

// See all the available parameter options withe the `--help` flag. E.g. `build/examples/openpose/openpose.bin --help`
// Note: This command will show you flags for other unnecessary 3rdparty files. Check only the flags for the OpenPose
// executable. E.g. for `openpose.bin`, look for `Flags from examples/openpose/openpose.cpp:`.
// Debugging/Other
DEFINE_int32(logging_level, 3, "The logging level. Integer in the range [0, 255]. 0 will output any log() message, while"
	" 255 will not output any. Current OpenPose library messages are in the range 0-4: 1 for"
	" low priority messages and 4 for important ones.");
DEFINE_bool(disable_multi_thread, false, "It would slightly reduce the frame rate in order to highly reduce the lag. Mainly useful"
	" for 1) Cases where it is needed a low latency (e.g. webcam in real-time scenarios with"
	" low-range GPU devices); and 2) Debugging OpenPose when it is crashing to locate the"
	" error.");
// Producer
DEFINE_int32(camera, -1, "The camera index for cv::VideoCapture. Integer in the range [0, 9]. Select a negative"
	" number (by default), to auto-detect and open the first available camera.");
DEFINE_string(camera_resolution, "1280x720", "Size of the camera frames to ask for.");
DEFINE_double(camera_fps, 30.0, "Frame rate for the webcam (only used when saving video from webcam). Set this value to the"
	" minimum value between the OpenPose displayed speed and the webcam real frame rate.");
DEFINE_string(video, "", "Use a video file instead of the camera. Use `examples/media/video.avi` for our default"
	" example video.");
DEFINE_string(image_dir, "", "Process a directory of images. Use `examples/media/` for our default example folder with 20"
	" images. Read all standard formats (jpg, png, bmp, etc.).");
DEFINE_string(ip_camera, "", "String with the IP camera URL. It supports protocols like RTSP and HTTP.");
DEFINE_uint64(frame_first, 0, "Start on desired frame number. Indexes are 0-based, i.e. the first frame has index 0.");
DEFINE_uint64(frame_last, -1, "Finish on desired frame number. Select -1 to disable. Indexes are 0-based, e.g. if set to"
	" 10, it will process 11 frames (0-10).");
DEFINE_bool(frame_flip, false, "Flip/mirror each frame (e.g. for real time webcam demonstrations).");
DEFINE_int32(frame_rotate, 0, "Rotate each frame, 4 possible values: 0, 90, 180, 270.");
DEFINE_bool(frames_repeat, false, "Repeat frames when finished.");
DEFINE_bool(process_real_time, false, "Enable to keep the original source frame rate (e.g. for video). If the processing time is"
	" too long, it will skip frames. If it is too fast, it will slow it down.");
// OpenPose
DEFINE_string(model_folder, "Assets/Dependencies/models/", "Folder path (absolute or relative) where the models (pose, face, ...) are located.");
DEFINE_string(output_resolution, "-1x-1", "The image resolution (display and output). Use \"-1x-1\" to force the program to use the"
	" input image resolution.");
DEFINE_int32(num_gpu, -1, "The number of GPU devices to use. If negative, it will use all the available GPUs in your"
	" machine.");
DEFINE_int32(num_gpu_start, 0, "GPU device start number.");
DEFINE_int32(keypoint_scale, 0, "Scaling of the (x,y) coordinates of the final pose data array, i.e. the scale of the (x,y)"
	" coordinates that will be saved with the `write_keypoint` & `write_keypoint_json` flags."
	" Select `0` to scale it to the original source resolution, `1`to scale it to the net output"
	" size (set with `net_resolution`), `2` to scale it to the final output size (set with"
	" `resolution`), `3` to scale it in the range [0,1], and 4 for range [-1,1]. Non related"
	" with `scale_number` and `scale_gap`.");
DEFINE_bool(identification, false, "Whether to enable people identification across frames. Not available yet, coming soon.");
// OpenPose Body Pose
DEFINE_bool(body_disable, false, "Disable body keypoint detection. Option only possible for faster (but less accurate) face"
	" keypoint detection.");
DEFINE_string(model_pose, "COCO", "Model to be used. E.g. `COCO` (18 keypoints), `MPI` (15 keypoints, ~10% faster), "
	"`MPI_4_layers` (15 keypoints, even faster but less accurate).");
DEFINE_string(net_resolution, "-1x368", "Multiples of 16. If it is increased, the accuracy potentially increases. If it is"
	" decreased, the speed increases. For maximum speed-accuracy balance, it should keep the"
	" closest aspect ratio possible to the images or videos to be processed. Using `-1` in"
	" any of the dimensions, OP will choose the optimal aspect ratio depending on the user's"
	" input value. E.g. the default `-1x368` is equivalent to `656x368` in 16:9 resolutions,"
	" e.g. full HD (1980x1080) and HD (1280x720) resolutions.");
DEFINE_int32(scale_number, 1, "Number of scales to average.");
DEFINE_double(scale_gap, 0.3, "Scale gap between scales. No effect unless scale_number > 1. Initial scale is always 1."
	" If you want to change the initial scale, you actually want to multiply the"
	" `net_resolution` by your desired initial scale.");
DEFINE_bool(heatmaps_add_parts, false, "If true, it will add the body part heatmaps to the final op::Datum::poseHeatMaps array,"
	" and analogously face & hand heatmaps to op::Datum::faceHeatMaps & op::Datum::handHeatMaps"
	" (program speed will decrease). Not required for our library, enable it only if you intend"
	" to process this information later. If more than one `add_heatmaps_X` flag is enabled, it"
	" will place then in sequential memory order: body parts + bkg + PAFs. It will follow the"
	" order on POSE_BODY_PART_MAPPING in `include/openpose/pose/poseParameters.hpp`.");
DEFINE_bool(heatmaps_add_bkg, false, "Same functionality as `add_heatmaps_parts`, but adding the heatmap corresponding to"
	" background.");
DEFINE_bool(heatmaps_add_PAFs, false, "Same functionality as `add_heatmaps_parts`, but adding the PAFs.");
// OpenPose Face
DEFINE_bool(face, false, "Enables face keypoint detection. It will share some parameters from the body pose, e.g."
	" `model_folder`. Note that this will considerable slow down the performance and increse"
	" the required GPU memory. In addition, the greater number of people on the image, the"
	" slower OpenPose will be.");
DEFINE_string(face_net_resolution, "368x368", "Multiples of 16 and squared. Analogous to `net_resolution` but applied to the face keypoint"
	" detector. 320x320 usually works fine while giving a substantial speed up when multiple"
	" faces on the image.");
// OpenPose Hand
DEFINE_bool(hand, false, "Enables hand keypoint detection. It will share some parameters from the body pose, e.g."
	" `model_folder`. Analogously to `--face`, it will also slow down the performance, increase"
	" the required GPU memory and its speed depends on the number of people.");
DEFINE_string(hand_net_resolution, "368x368", "Multiples of 16 and squared. Analogous to `net_resolution` but applied to the hand keypoint"
	" detector.");
DEFINE_int32(hand_scale_number, 1, "Analogous to `scale_number` but applied to the hand keypoint detector. Our best results"
	" were found with `hand_scale_number` = 6 and `hand_scale_range` = 0.4");
DEFINE_double(hand_scale_range, 0.4, "Analogous purpose than `scale_gap` but applied to the hand keypoint detector. Total range"
	" between smallest and biggest scale. The scales will be centered in ratio 1. E.g. if"
	" scaleRange = 0.4 and scalesNumber = 2, then there will be 2 scales, 0.8 and 1.2.");
DEFINE_bool(hand_tracking, false, "Adding hand tracking might improve hand keypoints detection for webcam (if the frame rate"
	" is high enough, i.e. >7 FPS per GPU) and video. This is not person ID tracking, it"
	" simply looks for hands in positions at which hands were located in previous frames, but"
	" it does not guarantee the same person ID among frames");
// OpenPose Rendering
DEFINE_int32(part_to_show, 0, "Prediction channel to visualize (default: 0). 0 for all the body parts, 1-18 for each body"
	" part heat map, 19 for the background heat map, 20 for all the body part heat maps"
	" together, 21 for all the PAFs, 22-40 for each body part pair PAF");
DEFINE_bool(disable_blending, false, "If enabled, it will render the results (keypoint skeletons or heatmaps) on a black"
	" background, instead of being rendered into the original image. Related: `part_to_show`,"
	" `alpha_pose`, and `alpha_pose`.");
// OpenPose Rendering Pose
DEFINE_double(render_threshold, 0.05, "Only estimated keypoints whose score confidences are higher than this threshold will be"
	" rendered. Generally, a high threshold (> 0.5) will only render very clear body parts;"
	" while small thresholds (~0.1) will also output guessed and occluded keypoints, but also"
	" more false positives (i.e. wrong detections).");
DEFINE_int32(render_pose, 2, "Set to 0 for no rendering, 1 for CPU rendering (slightly faster), and 2 for GPU rendering"
	" (slower but greater functionality, e.g. `alpha_X` flags). If rendering is enabled, it will"
	" render both `outputData` and `cvOutputData` with the original image and desired body part"
	" to be shown (i.e. keypoints, heat maps or PAFs).");
DEFINE_double(alpha_pose, 0.6, "Blending factor (range 0-1) for the body part rendering. 1 will show it completely, 0 will"
	" hide it. Only valid for GPU rendering.");
DEFINE_double(alpha_heatmap, 0.7, "Blending factor (range 0-1) between heatmap and original frame. 1 will only show the"
	" heatmap, 0 will only show the frame. Only valid for GPU rendering.");
// OpenPose Rendering Face
DEFINE_double(face_render_threshold, 0.4, "Analogous to `render_threshold`, but applied to the face keypoints.");
DEFINE_int32(face_render, -1, "Analogous to `render_pose` but applied to the face. Extra option: -1 to use the same"
	" configuration that `render_pose` is using.");
DEFINE_double(face_alpha_pose, 0.6, "Analogous to `alpha_pose` but applied to face.");
DEFINE_double(face_alpha_heatmap, 0.7, "Analogous to `alpha_heatmap` but applied to face.");
// OpenPose Rendering Hand
DEFINE_double(hand_render_threshold, 0.2, "Analogous to `render_threshold`, but applied to the hand keypoints.");
DEFINE_int32(hand_render, -1, "Analogous to `render_pose` but applied to the hand. Extra option: -1 to use the same"
	" configuration that `render_pose` is using.");
DEFINE_double(hand_alpha_pose, 0.6, "Analogous to `alpha_pose` but applied to hand.");
DEFINE_double(hand_alpha_heatmap, 0.7, "Analogous to `alpha_heatmap` but applied to hand.");
// Display
DEFINE_bool(fullscreen, false, "Run in full-screen mode (press f during runtime to toggle).");
DEFINE_bool(no_gui_verbose, false, "Do not write text on output images on GUI (e.g. number of current frame and people). It"
	" does not affect the pose rendering.");
DEFINE_bool(no_display, false, "Do not open a display window. Useful if there is no X server and/or to slightly speed up"
	" the processing if visual output is not required.");
// Result Saving
DEFINE_string(write_images, "", "Directory to write rendered frames in `write_images_format` image format.");
DEFINE_string(write_images_format, "png", "File extension and format for `write_images`, e.g. png, jpg or bmp. Check the OpenCV"
	" function cv::imwrite for all compatible extensions.");
DEFINE_string(write_video, "", "Full file path to write rendered frames in motion JPEG video format. It might fail if the"
	" final path does not finish in `.avi`. It internally uses cv::VideoWriter.");
DEFINE_string(write_keypoint, "", "Directory to write the people body pose keypoint data. Set format with `write_keypoint_format`.");
DEFINE_string(write_keypoint_format, "yml", "File extension and format for `write_keypoint`: json, xml, yaml & yml. Json not available"
	" for OpenCV < 3.0, use `write_keypoint_json` instead.");
DEFINE_string(write_keypoint_json, "", "Directory to write people pose data in *.json format, compatible with any OpenCV version.");
DEFINE_string(write_coco_json, "", "Full file path to write people pose data with *.json COCO validation format.");
DEFINE_string(write_heatmaps, "", "Directory to write body pose heatmaps in *.png format. At least 1 `add_heatmaps_X` flag"
	" must be enabled.");
DEFINE_string(write_heatmaps_format, "png", "File extension and format for `write_heatmaps`, analogous to `write_images_format`."
	" Recommended `png` or any compressed and lossless format.");


extern "C" {
	__declspec(dllexport) int openPoseDemo()
	{
		// logging_level
		op::check(0 <= FLAGS_logging_level && FLAGS_logging_level <= 255, "Wrong logging_level value.",
			__LINE__, __FUNCTION__, __FILE__);
		op::ConfigureLog::setPriorityThreshold((op::Priority)FLAGS_logging_level);
		// op::ConfigureLog::setPriorityThreshold(op::Priority::None); // To print all logging messages

		op::log("Starting pose estimation demo.", op::Priority::High);
		const auto timerBegin = std::chrono::high_resolution_clock::now();

		// Applying user defined configuration - Google flags to program variables
		// outputSize
		const auto outputSize = op::flagsToPoint(FLAGS_output_resolution, "-1x-1");
		// netInputSize
		const auto netInputSize = op::flagsToPoint(FLAGS_net_resolution, "-1x368");
		// faceNetInputSize
		const auto faceNetInputSize = op::flagsToPoint(FLAGS_face_net_resolution, "368x368 (multiples of 16)");
		// handNetInputSize
		const auto handNetInputSize = op::flagsToPoint(FLAGS_hand_net_resolution, "368x368 (multiples of 16)");
		// producerType
		const auto producerSharedPtr = op::flagsToProducer(FLAGS_image_dir, FLAGS_video, FLAGS_ip_camera, FLAGS_camera,
			FLAGS_camera_resolution, FLAGS_camera_fps);
		// poseModel
		const auto poseModel = op::flagsToPoseModel(FLAGS_model_pose);
		// keypointScale
		const auto keypointScale = op::flagsToScaleMode(FLAGS_keypoint_scale);
		// heatmaps to add
		const auto heatMapTypes = op::flagsToHeatMaps(FLAGS_heatmaps_add_parts, FLAGS_heatmaps_add_bkg,
			FLAGS_heatmaps_add_PAFs);
		// Enabling Google Logging
		const bool enableGoogleLogging = true;
		// Logging
		op::log("", op::Priority::Low, __LINE__, __FUNCTION__, __FILE__);

		// OpenPose wrapper
		op::log("Configuring OpenPose wrapper.", op::Priority::Low, __LINE__, __FUNCTION__, __FILE__);
		op::Wrapper<std::vector<op::Datum>> opWrapper;
		// Pose configuration (use WrapperStructPose{} for default and recommended configuration)
		const op::WrapperStructPose wrapperStructPose{ !FLAGS_body_disable, netInputSize, outputSize, keypointScale,
			FLAGS_num_gpu, FLAGS_num_gpu_start, FLAGS_scale_number,
			(float)FLAGS_scale_gap, op::flagsToRenderMode(FLAGS_render_pose),
			poseModel, !FLAGS_disable_blending, (float)FLAGS_alpha_pose,
			(float)FLAGS_alpha_heatmap, FLAGS_part_to_show, FLAGS_model_folder,
			heatMapTypes, op::ScaleMode::UnsignedChar,
			(float)FLAGS_render_threshold, enableGoogleLogging,
			FLAGS_identification };
		// Face configuration (use op::WrapperStructFace{} to disable it)
		const op::WrapperStructFace wrapperStructFace{ FLAGS_face, faceNetInputSize,
			op::flagsToRenderMode(FLAGS_face_render, FLAGS_render_pose),
			(float)FLAGS_face_alpha_pose, (float)FLAGS_face_alpha_heatmap,
			(float)FLAGS_face_render_threshold };
		// Hand configuration (use op::WrapperStructHand{} to disable it)
		const op::WrapperStructHand wrapperStructHand{ FLAGS_hand, handNetInputSize, FLAGS_hand_scale_number,
			(float)FLAGS_hand_scale_range, FLAGS_hand_tracking,
			op::flagsToRenderMode(FLAGS_hand_render, FLAGS_render_pose),
			(float)FLAGS_hand_alpha_pose, (float)FLAGS_hand_alpha_heatmap,
			(float)FLAGS_hand_render_threshold };
		// Producer (use default to disable any input)
		const op::WrapperStructInput wrapperStructInput{ producerSharedPtr, FLAGS_frame_first, FLAGS_frame_last,
			FLAGS_process_real_time, FLAGS_frame_flip, FLAGS_frame_rotate,
			FLAGS_frames_repeat };
		// Consumer (comment or use default argument to disable any output)
		const op::WrapperStructOutput wrapperStructOutput{ !FLAGS_no_display, !FLAGS_no_gui_verbose, FLAGS_fullscreen,
			FLAGS_write_keypoint,
			op::stringToDataFormat(FLAGS_write_keypoint_format),
			FLAGS_write_keypoint_json, FLAGS_write_coco_json,
			FLAGS_write_images, FLAGS_write_images_format, FLAGS_write_video,
			FLAGS_write_heatmaps, FLAGS_write_heatmaps_format };
		// Configure wrapper
		opWrapper.configure(wrapperStructPose, wrapperStructFace, wrapperStructHand, wrapperStructInput,
			wrapperStructOutput);
		// Set to single-thread running (to debug and/or reduce latency)
		if (FLAGS_disable_multi_thread)
			opWrapper.disableMultiThreading();

		// Start processing
		// Two different ways of running the program on multithread environment
		op::log("Starting thread(s)", op::Priority::High);
		// Option a) Recommended - Also using the main thread (this thread) for processing (it saves 1 thread)
		// Start, run & stop threads
		opWrapper.exec();  // It blocks this thread until all threads have finished

						   // // Option b) Keeping this thread free in case you want to do something else meanwhile, e.g. profiling the GPU
						   // memory
						   // // VERY IMPORTANT NOTE: if OpenCV is compiled with Qt support, this option will not work. Qt needs the main
						   // // thread to plot visual results, so the final GUI (which uses OpenCV) would return an exception similar to:
						   // // `QMetaMethod::invoke: Unable to invoke methods with return values in queued connections`
						   // // Start threads
						   // opWrapper.start();
						   // // Profile used GPU memory
						   //     // 1: wait ~10sec so the memory has been totally loaded on GPU
						   //     // 2: profile the GPU memory
						   // const auto sleepTimeMs = 10;
						   // for (auto i = 0 ; i < 10000/sleepTimeMs && opWrapper.isRunning() ; i++)
						   //     std::this_thread::sleep_for(std::chrono::milliseconds{sleepTimeMs});
						   // op::Profiler::profileGpuMemory(__LINE__, __FUNCTION__, __FILE__);
						   // // Keep program alive while running threads
						   // while (opWrapper.isRunning())
						   //     std::this_thread::sleep_for(std::chrono::milliseconds{sleepTimeMs});
						   // // Stop and join threads
						   // op::log("Stopping thread(s)", op::Priority::High);
						   // opWrapper.stop();

						   // Measuring total time
		const auto now = std::chrono::high_resolution_clock::now();
		const auto totalTimeSec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(now - timerBegin).count()
			* 1e-9;
		const auto message = "Real-time pose estimation demo successfully finished. Total time: "
			+ std::to_string(totalTimeSec) + " seconds.";
		op::log(message, op::Priority::High);

		return 0;
	}
}

/*
int main(int argc, char *argv[])
{
	// Parsing command line flags
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	// Running openPoseDemo
	return openPoseDemo();
}
*/






/*
// ------------------------- OpenPose Library Tutorial - Pose - Example 1 - Extract from Image -------------------------
// This first example shows the user how to:
    // 1. Load an image (`filestream` module)
    // 2. Extract the pose of that image (`pose` module)
    // 3. Render the pose on a resized copy of the input image (`pose` module)
    // 4. Display the rendered pose (`gui` module)
// In addition to the previous OpenPose modules, we also need to use:
    // 1. `core` module: for the Array<float> class that the `pose` module needs
    // 2. `utilities` module: for the error & logging functions, i.e. op::error & op::log respectively


// 3rdparty dependencies
// GFlags: DEFINE_bool, _int32, _int64, _uint64, _double, _string
#include <gflags/gflags.h>
// Allow Google Flags in Ubuntu 14
#ifndef GFLAGS_GFLAGS_H_
    namespace gflags = google;
#endif
// OpenPose dependencies
#include <openpose/core/headers.hpp>
#include <openpose/filestream/headers.hpp>
#include <openpose/gui/headers.hpp>
#include <openpose/pose/headers.hpp>
#include <openpose/utilities/headers.hpp>

//Custom dependencies
#include <iostream>

// See all the available parameter options withe the `--help` flag. E.g. `build/examples/openpose/openpose.bin --help`
// Note: This command will show you flags for other unnecessary 3rdparty files. Check only the flags for the OpenPose
// executable. E.g. for `openpose.bin`, look for `Flags from examples/openpose/openpose.cpp:`.
// Debugging/Other
DEFINE_int32(logging_level,             3,              "The logging level. Integer in the range [0, 255]. 0 will output any log() message, while"
                                                        " 255 will not output any. Current OpenPose library messages are in the range 0-4: 1 for"
                                                        " low priority messages and 4 for important ones.");
// Producer ///////////////////////////////////////////////////////////////////////////


DEFINE_string(image_path, "C:/Users/RHVR3.RHVR3/Desktop/Projects/Jerry/Masquerade-OpenPose/OpenPoseTesting/COCO_image.jpg",     "Process the desired image.");
//DEFINE_string(image_path, "/COCO_image.jpg",     "Process the desired image.");



// OpenPose
DEFINE_string(model_pose,               "COCO",         "Model to be used. E.g. `COCO` (18 keypoints), `MPI` (15 keypoints, ~10% faster), "
                                                        "`MPI_4_layers` (15 keypoints, even faster but less accurate).");
DEFINE_string(model_folder,				"C:/Users/RHVR3.RHVR3/Desktop/Projects/Jerry/Masquerade-OpenPose/OpenPoseTesting/openpose-master/models/",      
//DEFINE_string(model_folder,				"Dependencies/models/",      

														"Folder path (absolute or relative) where the models (pose, face, ...) are located.");
DEFINE_string(net_resolution,           "-1x368",       "Multiples of 16. If it is increased, the accuracy potentially increases. If it is"
                                                        " decreased, the speed increases. For maximum speed-accuracy balance, it should keep the"
                                                        " closest aspect ratio possible to the images or videos to be processed. Using `-1` in"
                                                        " any of the dimensions, OP will choose the optimal aspect ratio depending on the user's"
                                                        " input value. E.g. the default `-1x368` is equivalent to `656x368` in 16:9 resolutions,"
                                                        " e.g. full HD (1980x1080) and HD (1280x720) resolutions.");
DEFINE_string(output_resolution,        "-1x-1",        "The image resolution (display and output). Use \"-1x-1\" to force the program to use the"
                                                        " input image resolution.");
DEFINE_int32(num_gpu_start,             0,              "GPU device start number.");
DEFINE_double(scale_gap,                0.3,            "Scale gap between scales. No effect unless scale_number > 1. Initial scale is always 1."
                                                        " If you want to change the initial scale, you actually want to multiply the"
                                                        " `net_resolution` by your desired initial scale.");
DEFINE_int32(scale_number,              1,              "Number of scales to average.");
// OpenPose Rendering
DEFINE_bool(disable_blending,           false,          "If enabled, it will render the results (keypoint skeletons or heatmaps) on a black"
                                                        " background, instead of being rendered into the original image. Related: `part_to_show`,"
                                                        " `alpha_pose`, and `alpha_pose`.");
DEFINE_double(render_threshold,         0.05,           "Only estimated keypoints whose score confidences are higher than this threshold will be"
                                                        " rendered. Generally, a high threshold (> 0.5) will only render very clear body parts;"
                                                        " while small thresholds (~0.1) will also output guessed and occluded keypoints, but also"
                                                        " more false positives (i.e. wrong detections).");
DEFINE_double(alpha_pose,               0.6,            "Blending factor (range 0-1) for the body part rendering. 1 will show it completely, 0 will"
                                                        " hide it. Only valid for GPU rendering.");

extern "C" {
	__declspec(dllexport) float openPoseDemo()
	{
		std::cout << "I'm here" << std::endl;


		op::log("OpenPose Library Tutorial - Example 1.", op::Priority::High);
		// ------------------------- INITIALIZATION -------------------------
		// Step 1 - Set logging level
			// - 0 will output all the logging messages
			// - 255 will output nothing
		op::check(0 <= FLAGS_logging_level && FLAGS_logging_level <= 255, "Wrong logging_level value.",
			__LINE__, __FUNCTION__, __FILE__);
		op::ConfigureLog::setPriorityThreshold((op::Priority)FLAGS_logging_level);
		op::log("", op::Priority::Low, __LINE__, __FUNCTION__, __FILE__);
		// Step 2 - Read Google flags (user defined configuration)
		// outputSize
		const auto outputSize = op::flagsToPoint(FLAGS_output_resolution, "-1x-1");
		// netInputSize
		const auto netInputSize = op::flagsToPoint(FLAGS_net_resolution, "-1x368");
		// poseModel
		const auto poseModel = op::flagsToPoseModel(FLAGS_model_pose);
		// Check no contradictory flags enabled
		if (FLAGS_alpha_pose < 0. || FLAGS_alpha_pose > 1.)
			op::error("Alpha value for blending must be in the range [0,1].", __LINE__, __FUNCTION__, __FILE__);
		if (FLAGS_scale_gap <= 0. && FLAGS_scale_number > 1)
			op::error("Incompatible flag configuration: scale_gap must be greater than 0 or scale_number = 1.",
				__LINE__, __FUNCTION__, __FILE__);
		// Enabling Google Logging
		const bool enableGoogleLogging = true;
		// Logging
		op::log("", op::Priority::Low, __LINE__, __FUNCTION__, __FILE__);
		// Step 3 - Initialize all required classes
		op::ScaleAndSizeExtractor scaleAndSizeExtractor(netInputSize, outputSize, FLAGS_scale_number, FLAGS_scale_gap);
		op::CvMatToOpInput cvMatToOpInput;
		op::CvMatToOpOutput cvMatToOpOutput;
		op::PoseExtractorCaffe poseExtractorCaffe{ poseModel, FLAGS_model_folder,
												  FLAGS_num_gpu_start, {}, op::ScaleMode::ZeroToOne, enableGoogleLogging };
		op::PoseCpuRenderer poseRenderer{ poseModel, (float)FLAGS_render_threshold, !FLAGS_disable_blending,
										 (float)FLAGS_alpha_pose };
		op::OpOutputToCvMat opOutputToCvMat;
		op::FrameDisplayer frameDisplayer{ "OpenPose Tutorial - Example 1", outputSize };
		// Step 4 - Initialize resources on desired thread (in this case single thread, i.e. we init resources here)
		poseExtractorCaffe.initializationOnThread();
		poseRenderer.initializationOnThread();

		// ------------------------- POSE ESTIMATION AND RENDERING -------------------------
		// Step 1 - Read and load image, error if empty (possibly wrong path)
		// Alternative: cv::imread(FLAGS_image_path, CV_LOAD_IMAGE_COLOR);
		cv::Mat inputImage = op::loadImage(FLAGS_image_path, CV_LOAD_IMAGE_COLOR);
		if (inputImage.empty())
			op::error("Could not open or find the image: " + FLAGS_image_path, __LINE__, __FUNCTION__, __FILE__);
		const op::Point<int> imageSize{ inputImage.cols, inputImage.rows };
		// Step 2 - Get desired scale sizes
		std::vector<double> scaleInputToNetInputs;
		std::vector<op::Point<int>> netInputSizes;
		double scaleInputToOutput;
		op::Point<int> outputResolution;
		std::tie(scaleInputToNetInputs, netInputSizes, scaleInputToOutput, outputResolution)
			= scaleAndSizeExtractor.extract(imageSize);
		// Step 3 - Format input image to OpenPose input and output formats
		const auto netInputArray = cvMatToOpInput.createArray(inputImage, scaleInputToNetInputs, netInputSizes);
		auto outputArray = cvMatToOpOutput.createArray(inputImage, scaleInputToOutput, outputResolution);
		// Step 4 - Estimate poseKeypoints
		poseExtractorCaffe.forwardPass(netInputArray, imageSize, scaleInputToNetInputs);
		const auto poseKeypoints = poseExtractorCaffe.getPoseKeypoints();

		// Step 5 - Render poseKeypoints
		poseRenderer.renderPose(outputArray, poseKeypoints, scaleInputToOutput);

		// Step 6 - OpenPose output format to cv::Mat
		auto outputImage = opOutputToCvMat.formatToCvMat(outputArray);



		// ------------------------- SHOWING RESULT AND CLOSING -------------------------
		// Step 1 - Show results
		frameDisplayer.displayFrame(outputImage, 0); // Alternative: cv::imshow(outputImage) + cv::waitKey(0)
		// Step 2 - Logging information message
		op::log("Example 1 successfully finished.", op::Priority::High);
		// Return successful message
		return 0;
	}
}

*/

/*
int main(int argc, char *argv[])
{
    // Parsing command line flags
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // Running openPoseTutorialPose1
    return openPoseDemo();
}
*/






