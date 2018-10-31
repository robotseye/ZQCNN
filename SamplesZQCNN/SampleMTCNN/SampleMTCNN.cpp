#include "ZQ_CNN_Net.h"
#include "ZQ_CNN_MTCNN_old.h"
#include "ZQ_CNN_MTCNN.h"
#include <vector>
#include <iostream>
#include "opencv2\opencv.hpp"
#include "ZQ_CNN_CompileConfig.h"
#if ZQ_CNN_USE_BLAS_GEMM
#include <cblas.h>
#pragma comment(lib,"libopenblas.lib")
#endif
using namespace ZQ;
using namespace std;
using namespace cv;

static void Draw(cv::Mat &image, const std::vector<ZQ_CNN_BBox>& thirdBbox)
{
	std::vector<ZQ_CNN_BBox>::const_iterator it = thirdBbox.begin();
	for (; it != thirdBbox.end(); it++)
	{
		if ((*it).exist)
		{
			if (it->score > 0.7)
			{
				cv::rectangle(image, cv::Point((*it).col1, (*it).row1), cv::Point((*it).col2, (*it).row2), cv::Scalar(0, 0, 255), 2, 8, 0);
			}
			else
			{
				cv::rectangle(image, cv::Point((*it).col1, (*it).row1), cv::Point((*it).col2, (*it).row2), cv::Scalar(0, 255, 0), 2, 8, 0);
			}

			for (int num = 0; num < 5; num++)
				circle(image, cv::Point(*(it->ppoint + num) + 0.5f, *(it->ppoint + num + 5) + 0.5f), 3, cv::Scalar(0, 255, 255), -1);
		}
		else
		{
			printf("not exist!\n");
		}
	}
}

int main()
{
	int num_threads = 1;
#if ZQ_CNN_USE_BLAS_GEMM
	openblas_set_num_threads(num_threads);
#endif
	Mat image0 = cv::imread("data\\2.jpg", 1);
	if (image0.empty())
	{
		cout << "empty image\n";
		return EXIT_FAILURE;
	}
	/* TIPS: when finding tiny faces for very big image, gaussian blur is very useful for Pnet*/
	bool run_blur = true;
	int kernel_size = 3, sigma = 2;
	if (image0.cols * image0.rows > 2500 * 1600)
	{
		run_blur = true;
		kernel_size = 5;
		sigma = 3;
	}
	else if (image0.cols * image0.rows > 1920 * 1080)
	{
		run_blur = true;
		kernel_size = 3;
		sigma = 2;
	}
	else
	{
		run_blur = false;
	}

	if (run_blur)
	{
		cv::Mat blur_image0;
		int nBlurIters = 1000;
		double t00 = omp_get_wtime();
		for (int i = 0; i < nBlurIters; i++)
			cv::GaussianBlur(image0, blur_image0, cv::Size(kernel_size, kernel_size), sigma, sigma);
		double t01 = omp_get_wtime();
		printf("[%d] blur cost %.3f secs, 1 blur costs %.3f ms\n", nBlurIters, t01 - t00, 1000 * (t01 - t00) / nBlurIters);
		cv::GaussianBlur(image0, image0, cv::Size(kernel_size, kernel_size), sigma, sigma);
	}

	std::vector<ZQ_CNN_BBox> thirdBbox;
	ZQ_CNN_MTCNN mtcnn;
	std::string result_name;

	const int use_pnet20 = true;
	int thread_num = 8;
	bool special_handle_very_big_face = false;
	result_name = "resultdet.jpg";
	if (use_pnet20)
	{
		if (!mtcnn.Init("model\\det1-dw20.zqparams", "model\\det1-dw20-13.nchwbin",
			"model\\det2.zqparams", "model\\det2_bgr.nchwbin",
			"model\\det3.zqparams", "model\\det3_bgr.nchwbin", thread_num))
		{
			cout << "failed to init!\n";
			return EXIT_FAILURE;
		}

		mtcnn.SetPara(image0.cols, image0.rows, 20, 0.6, 0.7, 0.9, 0.4, 0.5, 0.5, 0.709, 3, 20, 4, special_handle_very_big_face);
	}
	else
	{
		if (!mtcnn.Init("model\\det1.zqparams", "model\\det1_bgr.nchwbin",
			"model\\det2.zqparams", "model\\det2_bgr.nchwbin",
			"model\\det3.zqparams", "model\\det3_bgr.nchwbin", thread_num))
		{
			cout << "failed to init!\n";
			return EXIT_FAILURE;
		}

		mtcnn.SetPara(image0.cols, image0.rows, 20, 0.6, 0.7, 0.9, 0.4, 0.5, 0.5, 0.709, 4, 12, 2, special_handle_very_big_face);
	}

	//mtcnn.TurnOnShowDebugInfo();
	int iters = 1;
	double t1 = omp_get_wtime();
	for (int i = 0; i < iters; i++)
	{
		if (i == iters / 2)
			mtcnn.TurnOnShowDebugInfo();
		else
			mtcnn.TurnOffShowDebugInfo();
		if (!mtcnn.Find(image0.data, image0.cols, image0.rows, image0.step[0], thirdBbox))
		{
			cout << "failed to find face!\n";
			//return EXIT_FAILURE;
			continue;
		}
	}
	double t2 = omp_get_wtime();
	printf("total %.3f s / %d = %.3f ms\n", t2 - t1, iters, 1000 * (t2 - t1) / iters);

	namedWindow("result");
	Draw(image0, thirdBbox);
	imwrite(result_name, image0);
	imshow("result", image0);

	waitKey(0);
	return EXIT_SUCCESS;
}
