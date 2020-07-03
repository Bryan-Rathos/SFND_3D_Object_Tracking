
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <utility>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>


#include "camFusion.hpp"
#include "dataStructures.h"
#include "utils.cpp"

// Create groups of Lidar points whose projection into the camera falls into the same bounding box
void clusterLidarWithROI(std::vector<BoundingBox> &boundingBoxes, std::vector<LidarPoint> &lidarPoints, float shrinkFactor, cv::Mat &P_rect_xx, cv::Mat &R_rect_xx, cv::Mat &RT)
{
    // loop over all Lidar points and associate them to a 2D bounding box
    cv::Mat X(4, 1, cv::DataType<double>::type);
    cv::Mat Y(3, 1, cv::DataType<double>::type);

    for (auto it1 = lidarPoints.begin(); it1 != lidarPoints.end(); ++it1)
    {
        // assemble vector for matrix-vector-multiplication
        X.at<double>(0, 0) = it1->x;
        X.at<double>(1, 0) = it1->y;
        X.at<double>(2, 0) = it1->z;
        X.at<double>(3, 0) = 1;

        // project Lidar point into camera
        Y = P_rect_xx * R_rect_xx * RT * X;
        cv::Point pt;
        pt.x = Y.at<double>(0, 0) / Y.at<double>(0, 2); // pixel coordinates
        pt.y = Y.at<double>(1, 0) / Y.at<double>(0, 2);

        std::vector<std::vector<BoundingBox>::iterator> enclosingBoxes; // pointers to all bounding boxes which enclose the current Lidar point
        for (std::vector<BoundingBox>::iterator it2 = boundingBoxes.begin(); it2 != boundingBoxes.end(); ++it2)
        {
            // shrink current bounding box slightly to avoid having too many outlier points around the edges
            cv::Rect smallerBox;
            smallerBox.x = (*it2).roi.x + shrinkFactor * (*it2).roi.width / 2.0;
            smallerBox.y = (*it2).roi.y + shrinkFactor * (*it2).roi.height / 2.0;
            smallerBox.width = (*it2).roi.width * (1 - shrinkFactor);
            smallerBox.height = (*it2).roi.height * (1 - shrinkFactor);

            // check wether point is within current bounding box
            if (smallerBox.contains(pt))
            {
                enclosingBoxes.push_back(it2);
            }

        } // eof loop over all bounding boxes

        // check wether point has been enclosed by one or by multiple boxes
        if (enclosingBoxes.size() == 1)
        { 
            // add Lidar point to bounding box
            enclosingBoxes[0]->lidarPoints.push_back(*it1);
        }

    } // eof loop over all Lidar points
}


void show3DObjects(std::vector<BoundingBox> &boundingBoxes, cv::Size worldSize, cv::Size imageSize, bool bWait)
{
    // create topview image
    cv::Mat topviewImg(imageSize, CV_8UC3, cv::Scalar(255, 255, 255));

    for(auto it1=boundingBoxes.begin(); it1!=boundingBoxes.end(); ++it1)
    {
        // create randomized color for current 3D object
        cv::RNG rng(it1->boxID);
        cv::Scalar currColor = cv::Scalar(rng.uniform(0,150), rng.uniform(0, 150), rng.uniform(0, 150));

        // plot Lidar points into top view image
        int top=1e8, left=1e8, bottom=0.0, right=0.0; 
        float xwmin=1e8, ywmin=1e8, ywmax=-1e8;
        for (auto it2 = it1->lidarPoints.begin(); it2 != it1->lidarPoints.end(); ++it2)
        {
            // world coordinates
            float xw = (*it2).x; // world position in m with x facing forward from sensor
            float yw = (*it2).y; // world position in m with y facing left from sensor
            xwmin = xwmin<xw ? xwmin : xw;
            ywmin = ywmin<yw ? ywmin : yw;
            ywmax = ywmax>yw ? ywmax : yw;

            // top-view coordinates
            int y = (-xw * imageSize.height / worldSize.height) + imageSize.height;
            int x = (-yw * imageSize.width / worldSize.width) + imageSize.width / 2;

            // find enclosing rectangle
            top = top<y ? top : y;
            left = left<x ? left : x;
            bottom = bottom>y ? bottom : y;
            right = right>x ? right : x;

            // draw individual point
            cv::circle(topviewImg, cv::Point(x, y), 4, currColor, -1);
        }

        // draw enclosing rectangle
        cv::rectangle(topviewImg, cv::Point(left, top), cv::Point(right, bottom),cv::Scalar(0,0,0), 2);

        // augment object with some key data
        char str1[200], str2[200];
        sprintf(str1, "id=%d, #pts=%d", it1->boxID, (int)it1->lidarPoints.size());
        putText(topviewImg, str1, cv::Point2f(left-25, bottom+50), cv::FONT_ITALIC, 1, currColor, 2);
        sprintf(str2, "xmin=%2.2f m, yw=%2.2f m", xwmin, ywmax-ywmin);
        putText(topviewImg, str2, cv::Point2f(left-25, bottom+125), cv::FONT_ITALIC, 1, currColor, 2);  
    }

    // plot distance markers
    float lineSpacing = 2.0; // gap between distance markers
    int nMarkers = floor(worldSize.height / lineSpacing);
    for (size_t i = 0; i < nMarkers; ++i)
    {
        int y = (-(i * lineSpacing) * imageSize.height / worldSize.height) + imageSize.height;
        cv::line(topviewImg, cv::Point(0, y), cv::Point(imageSize.width, y), cv::Scalar(255, 0, 0));
    }

    // display image
    std::string windowName = "3D Objects";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);
    cv::imshow(windowName, topviewImg);

    if(bWait)
    {
        cv::waitKey(0); // wait for key to be pressed
    }
}


// associate a given bounding box with the keypoints it contains
void clusterKptMatchesWithROI(BoundingBox &boundingBox, std::vector<cv::KeyPoint> &kptsPrev, std::vector<cv::KeyPoint> &kptsCurr, std::vector<cv::DMatch> &kptMatches)
{
    // std::cout << kptsPrev.size() << " " << kptsCurr.size() << std::endl;
    // std::cout << "Total matches: " << kptMatches.size() << std::endl;

    // std::vector<double> distances;
    std::vector<std::pair<int, double>> distMap;

    for(auto& match: kptMatches)
    {
        cv::KeyPoint prevKpt = kptsPrev[match.queryIdx];
    	cv::KeyPoint currKpt = kptsCurr[match.trainIdx];

        if(boundingBox.roi.contains(currKpt.pt))
        {
            double euDist = sqrt(pow(prevKpt.pt.x - currKpt.pt.x, 2) + pow(prevKpt.pt.y - currKpt.pt.y, 2));
            distMap.push_back(std::make_pair(match.trainIdx, euDist));
            // distances.push_back(euDist);
        }
    }

    // std::cout << "distMap size before outlier removal:" << distMap.size() << std::endl;

    removeOutliersCam(distMap);

    // std::cout << "distMap size after outlier removal: "<< distMap.size() << std::endl; 
    
    // int uni_count = std::distance(distMap.begin(), std::unique(distMap.begin(), distMap.end()));
    // int uni_count = std::distance(kptMatches.begin(), std::unique(kptMatches.begin(), kptMatches.end(), [](const cv::DMatch &a, const cv::DMatch &b) { return (a.trainIdx == b.trainIdx); } ));
    // std::cout << "Unique matches: " << uni_count << std::endl;

    for(auto ele : distMap)
    {
        // std::cout << ele.first << " " << ele.second << std::endl;
        for(auto match : kptMatches)
        {
            if(ele.first == match.trainIdx)
            {
                // std::cout << ele.first << " " << match.trainIdx << std::endl; 
                boundingBox.kptMatches.push_back(match);
            }
        }
    }
    // std::cout << "bbox match roi:" << boundingBox.kptMatches.size() << std::endl;
    // std::cout << "Bbox contains: " << count << " keypoints" << std::endl;
}


// Compute time-to-collision (TTC) based on keypoint correspondences in successive images
void computeTTCCamera(std::vector<cv::KeyPoint> &kptsPrev, std::vector<cv::KeyPoint> &kptsCurr, 
                      std::vector<cv::DMatch> kptMatches, double frameRate, double &TTC, cv::Mat *visImg)
{
    // compute distance ratios between all matched keypoints
    std::vector<double> distRatios; // stores the distance ratios for all keypoints between curr. and prev. frame
    for (auto it1 = kptMatches.begin(); it1 != kptMatches.end() - 1; ++it1)
    { // outer kpt. loop

        // get current keypoint and its matched partner in the prev. frame
        cv::KeyPoint kpOuterCurr = kptsCurr.at(it1->trainIdx);
        cv::KeyPoint kpOuterPrev = kptsPrev.at(it1->queryIdx);

        for (auto it2 = kptMatches.begin() + 1; it2 != kptMatches.end(); ++it2)
        { // inner kpt.-loop

            double minDist = 100.0; // min. required distance

            // get next keypoint and its matched partner in the prev. frame
            cv::KeyPoint kpInnerCurr = kptsCurr.at(it2->trainIdx);
            cv::KeyPoint kpInnerPrev = kptsPrev.at(it2->queryIdx);

            // compute distances and distance ratios
            double distCurr = cv::norm(kpOuterCurr.pt - kpInnerCurr.pt);
            double distPrev = cv::norm(kpOuterPrev.pt - kpInnerPrev.pt);

            if (distPrev > std::numeric_limits<double>::epsilon() && distCurr >= minDist)
            { // avoid division by zero

                double distRatio = distCurr / distPrev;
                distRatios.push_back(distRatio);
            }
        } // eof inner loop over all matched kpts
    }     // eof outer loop over all matched kpts

    // only continue if list of distance ratios is not empty
    if (distRatios.size() == 0)
    {
        TTC = NAN;
        return;
    }

    // STUDENT TASK (replacement for meanDistRatio)
    std::sort(distRatios.begin(), distRatios.end());
    long medIndex = floor(distRatios.size() / 2.0);
    double medDistRatio = distRatios.size() % 2 == 0 ? (distRatios[medIndex - 1] + distRatios[medIndex]) / 2.0 : distRatios[medIndex]; // compute median dist. ratio to remove outlier influence

    double dT = 1 / frameRate;
    TTC = -dT / (1 - medDistRatio);
}


void computeTTCLidar(std::vector<LidarPoint> &lidarPointsPrev,
                     std::vector<LidarPoint> &lidarPointsCurr, double frameRate, double &TTC)
{
    double dT = 1.0/frameRate;
    double meanXPrev, meanXCurr;

    // std::cout << "lidarPointsPrev size = " << lidarPointsPrev.size() << endl;
    removeOutliersLidar(lidarPointsPrev);
    // std::cout << "lidarPointsPrev size after outlier removal = " << lidarPointsPrev.size() << endl;
    
    // std::cout << "lidarPointsCurr size = " << lidarPointsCurr.size() << endl;
    removeOutliersLidar(lidarPointsCurr);
    // std::cout << "lidarPointsCurr size after outlier removal = " << lidarPointsCurr.size() << endl;

    double prevTotal = 0, currTotal = 0;

    prevTotal = std::accumulate(lidarPointsPrev.begin(), lidarPointsPrev.end(), 0.0, [](double sum, const LidarPoint &pt){ return (sum + pt.x); } );
    meanXPrev = prevTotal / lidarPointsPrev.size();

    // std::cout << "meanXPrev: " << meanXPrev << std::endl;

    currTotal = std::accumulate(lidarPointsCurr.begin(), lidarPointsCurr.end(), 0.0, [](double sum, const LidarPoint &pt){ return (sum + pt.x); } );    
    meanXCurr = currTotal / lidarPointsCurr.size();

    // compute TTC from both measurements
    TTC = (meanXCurr * dT) / (meanXPrev - meanXCurr);
}


void matchBoundingBoxes(std::vector<cv::DMatch> &matches, std::map<int, int> &bbBestMatches, DataFrame &prevFrame, DataFrame &currFrame)
{
    // cout << "prevBoxes: " << prevFrame.boundingBoxes.size() << ", currBoxes: " << currFrame.boundingBoxes.size() << endl;

    std::vector<std::vector<int>> matchCountMap;
    matchCountMap.resize(prevFrame.boundingBoxes.size(), std::vector<int>(currFrame.boundingBoxes.size(),0));
    
    for(auto itMatch = matches.begin(); itMatch != matches.end(); ++itMatch)
    {
        auto prevFramePt = prevFrame.keypoints[itMatch->queryIdx].pt;
        auto currFramePt = currFrame.keypoints[itMatch->trainIdx].pt;

        for(auto prevBox = prevFrame.boundingBoxes.begin(); prevBox != prevFrame.boundingBoxes.end(); ++prevBox)
        {
            for(auto currBox = currFrame.boundingBoxes.begin(); currBox != currFrame.boundingBoxes.end(); ++currBox)
            {
                if(prevBox->roi.contains(prevFramePt) && currBox->roi.contains(currFramePt))
                {
                    matchCountMap[prevBox->boxID][currBox->boxID] += 1;
                }
            }
        }
    }

    // PRINT MATCH COUNT TABLE
    // for(auto& row : matchCountMap){
    //     for(auto& ele : row){
    //         cout << std::setw(5) << ele << " | ";
    //     }
    //     cout << endl;
    // }

    for(int row = 0; row < matchCountMap.size(); row++)
    {
        int bestMatch = std::distance(matchCountMap[row].begin(),std::max_element(matchCountMap[row].begin(),matchCountMap[row].end())); 
        bbBestMatches[row] = bestMatch;
    }

    // PRINT BEST BOUNDING BOX MATCHES MAP
    // for (auto const& pair: bbBestMatches) {
	// 	std::cout << "{" << pair.first << ": " << pair.second << "}\n";
	// }
}