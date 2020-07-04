# SFND 3D Object Tracking

Welcome to the final project of the camera course. By completing all the lessons, you now have a solid understanding of keypoint detectors, descriptors, and methods to match them between successive images. Also, you know how to detect objects in an image using the YOLO deep-learning framework. And finally, you know how to associate regions in a camera image with Lidar points in 3D space. Let's take a look at our program schematic to see what we already have accomplished and what's still missing.

<img src="images/course_code_structure.png" width="779" height="414" />

In this final project, you will implement the missing parts in the schematic. To do this, you will complete four major tasks: 
1. First, you will develop a way to match 3D objects over time by using keypoint correspondences. 
2. Second, you will compute the TTC based on Lidar measurements. 
3. You will then proceed to do the same using the camera, which requires to first associate keypoint matches to regions of interest and then to compute the TTC based on those matches. 
4. And lastly, you will conduct various tests with the framework. Your goal is to identify the most suitable detector/descriptor combination for TTC estimation and also to search for problems that can lead to faulty measurements by the camera or Lidar sensor. In the last course of this Nanodegree, you will learn about the Kalman filter, which is a great way to combine the two independent TTC measurements into an improved version which is much more reliable than a single sensor alone can be. But before we think about such things, let us focus on your final project in the camera course. 

## Dependencies for Running Locally
* cmake >= 2.8
  * All OSes: [click here for installation instructions](https://cmake.org/install/)
* make >= 4.1 (Linux, Mac), 3.81 (Windows)
  * Linux: make is installed by default on most Linux distros
  * Mac: [install Xcode command line tools to get make](https://developer.apple.com/xcode/features/)
  * Windows: [Click here for installation instructions](http://gnuwin32.sourceforge.net/packages/make.htm)
* Git LFS
  * Weight files are handled using [LFS](https://git-lfs.github.com/)
* OpenCV >= 4.1
  * This must be compiled from source using the `-D OPENCV_ENABLE_NONFREE=ON` cmake flag for testing the SIFT and SURF detectors.
  * The OpenCV 4.1.0 source code can be found [here](https://github.com/opencv/opencv/tree/4.1.0)
* gcc/g++ >= 5.4
  * Linux: gcc / g++ is installed by default on most Linux distros
  * Mac: same deal as make - [install Xcode command line tools](https://developer.apple.com/xcode/features/)
  * Windows: recommend using [MinGW](http://www.mingw.org/)

## Basic Build Instructions

1. Clone this repo.
2. Make a build directory in the top level project directory: `mkdir build && cd build`
3. Compile: `cmake .. && make`
4. Run it: `./3D_object_tracking`.

## Project Rubrics

* FP.1
In the `matchBoundingBox()` function we make a table `matchCountMap` whose size is (num bounding boxes in previous frame x num bounding boxes in current frame). We iterate over the bounding boxes of previous and current frames and keep a track of keypoints as to which bounding box they belong in each frame. The number of keypoints in every bounding box betwwen frames is updated in the table. This is done in line `272-287` of `camFusion_Student.cpp`. Next we find the best match for the bounding box in the previous frame in the current frame by choosing the bounding box id which has the highest number of matches i.e. max element of each row of `matchCountMap` would give us the best match between frames for the bounding boxes. This is implememnted in lines `297-301` and the best bounding boxes matches are returned from the function.

* FP.2
The formula used for TTc lidar is same as discussed in the classroom. The main task here is to deal with the noisy measurements/outliers. To do this, I use the boxplot technique to find out the Inter-Quartile Range (IQR) of the 3D LIDAR points. The lower and upper threshhold limit was chosen as Q1 - 1.2 * IQR and Q3 + 1.2 * IQR respectively. After performing the outlier removal I found it better to use the mean x value of the LIDAR points in the TTC calculations instead of the x minimum of the points as it gives better TTC calculations. Using x minimum sometimes causes erroe when the measurement is faulty near the object edges. This can be foundin the `computeTTCLidar()` function. I have created a `utils.cpp` file which contains the outlier removal function as well as a fucntion for `IQR` calculations separately for LIDAR and Camera.












