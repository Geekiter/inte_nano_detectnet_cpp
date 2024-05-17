
#include "jetson-utils/videoSource.h"
#include "jetson-utils/videoOutput.h"
#include "jetson-utils/imageLoader.h"
#include "jetson-inference/detectNet.h"
#include "jetson-inference/objectTracker.h"
#include "cJSON.h"
#include "jetson-utils/Socket.h"
#include <signal.h>
#include <vector>
using namespace std;

struct PredObj
{
	float x1;
	float y1;
	float x2;
	float y2;
	float score;
	int classID;
	float width;
	float height;
	const char *classDesc;
};

bool signal_recieved = false;

void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		LogVerbose("received SIGINT\n");
		signal_recieved = true;
	}
}

int usage()
{
	printf("usage: detectnet [--help] [--network=NETWORK] [--threshold=THRESHOLD] ...\n");
	printf("                 input [output]\n\n");
	printf("Locate objects in a video/image stream using an object detection DNN.\n");
	printf("See below for additional arguments that may not be shown above.\n\n");
	printf("positional arguments:\n");
	printf("    input           resource URI of input stream  (see videoSource below)\n");
	printf("    output          resource URI of output stream (see videoOutput below)\n\n");

	printf("%s", detectNet::Usage());
	printf("%s", objectTracker::Usage());
	printf("%s", videoSource::Usage());
	printf("%s", videoOutput::Usage());
	printf("%s", Log::Usage());

	return 0;
}

int main(int argc, char **argv)
{

	/*
	 * parse command line
	 */
	commandLine cmdLine(argc, argv);

	if (cmdLine.GetFlag("help"))
		return usage();

	/*
	 * create detection network
	 */
	detectNet *net = detectNet::Create(cmdLine);

	if (!net)
	{
		LogError("detectnet:  failed to load detectNet model\n");
		return 1;
	}

	// parse overlay flags
	const uint32_t overlayFlags = detectNet::OverlayFlagsFromStr(cmdLine.GetString("overlay", "box,labels,conf"));

	Socket *socket = Socket::Create(SOCKET_TCP);
	if (!socket)
	{
		printf("failed to create socket\n");
		return 0;
	}

	// bind the socket to a local port
	if (!socket->Bind("192.168.31.189", 8899))
	{
		printf("failed to bind socket\n");
		return 0;
	}

	if (!socket->Accept(0))
	{
		printf("failed to accept socket\n");
		return 0;
	}
	printf("server is running\n");

	if (cmdLine.GetFlag("help"))
		return usage();

	/*
	 * attach signal handler
	 */
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		LogError("can't catch SIGINT\n");

	/*
	 * create input stream
	 */

	while (true)
	{
		// receive a message
		uint8_t buffer[1024];
		uint32_t remoteIP;
		uint16_t remotePort;
		size_t bytes = socket->Recieve(buffer, sizeof(buffer), &remoteIP, &remotePort);
		if (bytes == 0)
		{
			printf("close client\n");
			return 0;
		}
		else
		{
			const char *receiveData = NULL;
			printf("received message: %s\n", buffer);
			cJSON *cjson_receive = cJSON_Parse((char *)buffer);
			receiveData = cJSON_Print(cjson_receive);
			printf("%s\n", receiveData);
			SAFE_DELETE(receiveData);

			// cJSON *fStickLat = cJSON_GetObjectItem(cjson_receive, "fStickLat");
			// double m_fStickLat = cJSON_GetNumberValue(fStickLat);
			// printf("%.10f\n", m_fStickLat);
			// printf("m_fStickLat: %.10f\n", m_fStickLat);
			cJSON *fpath = cJSON_GetObjectItem(cjson_receive, "fpath");
			char *m_fpath = cJSON_GetStringValue(fpath);
			printf("m_fpath: %s\n", m_fpath);

			// 检测到socket等于shutdown，关闭socket
			if (strcmp((char *)buffer, "shutdown") == 0)
			{
				printf("shutting down\n");
				break;
			}
			char av1[] = "detectnet";
			char av2[] = "--network=ssd-mobilenet-v2";
			char av3[] = "/root/dev/py/jetson-inference/examples/socket_example/test1.jpg";
			int len = strlen(m_fpath);
			char av3_2[len];
			strcpy(av3_2, m_fpath);
			// char *fake_argv[3] = {av1, av2, m_fpath};
			char *fake_argv[3] = {av1, av2, av3_2};
			commandLine cmdLine(3, fake_argv);
			printf("t1\n");
			cJSON_Delete(cjson_receive);
			// cJSON_Delete(fpath);

			// SAFE_DELETE(m_fpath);
			printf("t2\n");

			/*
			 * create input stream
			 */

			videoSource *input = videoSource::Create(cmdLine, ARG_POSITION(0));

			if (!input)
			{
				LogError("detectnet:  failed to create input stream\n");
				// return 1;
				continue;
			}

			/*
			 * create output stream
			 */
			videoOutput *output = videoOutput::Create(cmdLine, ARG_POSITION(1));

			if (!output)
			{
				LogError("detectnet:  failed to create output stream\n");
				// return 1;
				continue;
			}
			/*
			 * processing loop
			 */
			// while( !signal_recieved )
			//{
			//  capture next image
			uchar3 *image = NULL;
			int status = 0;

			input->Capture(&image, &status);

			// if (!input->Capture(&image, &status))
			// {
			// 	// if( status == videoSource::TIMEOUT )
			// 	// continue;

			// 	// break; // EOS
			// }

			// detect objects in the frame
			detectNet::Detection *detections = NULL;

			const int numDetections = net->Detect(image, input->GetWidth(), input->GetHeight(), &detections, overlayFlags);
			vector<PredObj> predObjList;

			if (numDetections > 0)
			{
				LogVerbose("%i objects detected\n", numDetections);

				for (int n = 0; n < numDetections; n++)
				{
					LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n", n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID), detections[n].Confidence);
					LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n", n, detections[n].Left, detections[n].Top, detections[n].Right, detections[n].Bottom, detections[n].Width(), detections[n].Height());

					if (detections[n].TrackID >= 0) // is this a tracked object?
						LogVerbose("tracking  ID %i  status=%i  frames=%i  lost=%i\n", detections[n].TrackID, detections[n].TrackStatus, detections[n].TrackFrames, detections[n].TrackLost);

					PredObj predObj;
					predObj.x1 = detections[n].Left;
					predObj.y1 = detections[n].Top;
					predObj.x2 = detections[n].Right;
					predObj.y2 = detections[n].Bottom;
					predObj.score = detections[n].Confidence;
					predObj.classID = detections[n].ClassID;
					predObj.width = detections[n].Width();
					predObj.height = detections[n].Height();
					predObj.classDesc = net->GetClassDesc(detections[n].ClassID);
					predObjList.push_back(predObj);
				}
			}

			// predObjList to json
			cJSON *cjson_send = cJSON_CreateObject();
			cJSON *cjson_array = cJSON_CreateArray();
			for (int i = 0; i < predObjList.size(); i++)
			{
				cJSON *cjson_obj = cJSON_CreateObject();
				cJSON_AddNumberToObject(cjson_obj, "x1", predObjList[i].x1);
				cJSON_AddNumberToObject(cjson_obj, "y1", predObjList[i].y1);
				cJSON_AddNumberToObject(cjson_obj, "x2", predObjList[i].x2);
				cJSON_AddNumberToObject(cjson_obj, "y2", predObjList[i].y2);
				cJSON_AddNumberToObject(cjson_obj, "score", predObjList[i].score);
				cJSON_AddNumberToObject(cjson_obj, "classID", predObjList[i].classID);
				cJSON_AddNumberToObject(cjson_obj, "width", predObjList[i].width);
				cJSON_AddNumberToObject(cjson_obj, "height", predObjList[i].height);
				cJSON_AddStringToObject(cjson_obj, "classDesc", predObjList[i].classDesc);
				cJSON_AddItemToArray(cjson_array, cjson_obj);
			}
			cJSON_AddItemToObject(cjson_send, "predObjList", cjson_array);
			char *json_str = cJSON_Print(cjson_send);

			if (!socket->Send((void *)json_str, strlen(json_str), remoteIP, remotePort))
			{
				// printf("failed to send message\n");
				printf("client is disconnected\n");
				break;
			}
			printf("t3\n");
			// cJSON_Delete(cjson_send);
			cJSON_Delete(cjson_array);
			SAFE_DELETE(json_str);

			// render outputs
			if (false && output != NULL)
			{
				output->Render(image, input->GetWidth(), input->GetHeight());

				// update the status bar
				char str[256];
				sprintf(str, "TensorRT %i.%i.%i | %s | Network %.0f FPS", NV_TENSORRT_MAJOR, NV_TENSORRT_MINOR, NV_TENSORRT_PATCH, precisionTypeToStr(net->GetPrecision()), net->GetNetworkFPS());
				output->SetStatus(str);

				// check if the user quit
				// if( !output->IsStreaming() )
				// break;
			}

			// print out timing info
			net->PrintProfilerTimes();
			printf("t4\n");

			SAFE_DELETE(input);
			SAFE_DELETE(output);
			// SAFE_DELETE(image);

			//}
		}
	}
	/*
	 * destroy resources
	 */
	LogVerbose("detectnet:  shutting down...\n");

	SAFE_DELETE(net);

	LogVerbose("detectnet:  shutdown complete.\n");
	return 0;
}
