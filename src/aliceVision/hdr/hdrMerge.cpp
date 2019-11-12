// This file is part of the AliceVision project.
// Copyright (c) 2019 AliceVision contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "hdrMerge.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <aliceVision/alicevision_omp.hpp>


namespace aliceVision {
namespace hdr {

/**
 * f(x)=min + (max-min) * \frac{1}{1 + e^{10 * (x - mid) / width}}
 * https://www.desmos.com/calculator/xamvguu8zw
 *              ____
 * sigmoid:         \________
 *                sigMid
 */
inline float sigmoid(float zeroVal, float endVal, float sigwidth, float sigMid, float xval)
{
    return zeroVal + (endVal - zeroVal) * (1.0f / (1.0f + expf(10.0f * ((xval - sigMid) / sigwidth))));
}

/**
 * https://www.desmos.com/calculator/cvu8s3rlvy
 *
 *                       ____
 * sigmoid inv:  _______/
 *                    sigMid
 */
inline float sigmoidInv(float zeroVal, float endVal, float sigwidth, float sigMid, float xval)
{
    return zeroVal + (endVal - zeroVal) * (1.0f / (1.0f + expf(10.0f * ((sigMid - xval) / sigwidth))));
}

void hdrMerge::process(const std::vector< image::Image<image::RGBfColor> > &images,
                              const std::vector<float> &times,
                              const rgbCurve &weight,
                              const rgbCurve &response,
                              image::Image<image::RGBfColor> &radiance,
                              float targetTime,
                              bool robCalibrate,
                              float clampedValueCorrection)
{
  //checks
  assert(!response.isEmpty());
  assert(!images.empty());
  assert(images.size() == times.size());

  //reset radiance image
  radiance.fill(image::RGBfColor(0.f, 0.f, 0.f));

  //get images width, height
  const std::size_t width = images.front().Width();
  const std::size_t height = images.front().Height();

  const float maxLum = 1000.0f;
  const float minLum = 0.0001f;

  rgbCurve weightShortestExposure = weight;
  weightShortestExposure.invertAndScaleSecondPart(1.0f + clampedValueCorrection * maxLum);

  #pragma omp parallel for
  for(int y = 0; y < height; ++y)
  {
    for(int x = 0; x < width; ++x)
    {
      //for each pixels
      image::RGBfColor &radianceColor = radiance(y, x);

      for(std::size_t channel = 0; channel < 3; ++channel)
      {
        double wsum = 0.0;
        double wdiv = 0.0;

        {
            int exposureIndex = 0;
            // float highValue = images[exposureIndex](y, x)(channel);
            // // https://www.desmos.com/calculator/xamvguu8zw
            // //                       ____
            // // sigmoid inv:  _______/
            // //                  0    1
            // float clampedHighValue = sigmoidInv(0.0f, 1.0f, /*sigWidth=*/0.2f,  /*sigMid=*/0.9f, highValue);
            //////////

            // for each images
            const double value = images[exposureIndex](y, x)(channel);
            const double time = times[exposureIndex];
            //
            //                                       /
            // weightShortestExposure:          ____/
            //                          _______/
            //                                0      1
            double w = std::max(0.f, weightShortestExposure(value, channel)); //  - weight(0.05, 0)

            const double r = response(value, channel);

            wsum += w * r / time;
            wdiv += w;

        }
        for(std::size_t i = 1; i < images.size(); ++i)
        {
          // for each images
          const double value = images[i](y, x)(channel);
          const double time = times[i];
          //
          // weight:          ____
          //          _______/    \________
          //                0      1
          double w = std::max(0.f, weight(value, channel)); //  - weight(0.05, 0)

          const double r = response(value, channel);
          wsum += w * r / time;
          wdiv += w;
        }
        //{
        //    int exposureIndex = images.size() - 1;
        //    double lowValue = images[exposureIndex](y, x)(channel);
        //    // https://www.desmos.com/calculator/cvu8s3rlvy
        //    //              ____
        //    // sigmoid:         \________
        //    //                  0    1
        //    double clampedLowValue = sigmoid(0.0f, 1.0f, /*sigWidth=*/0.01f, /*sigMid=*/0.005, lowValue);            
        //}

        radianceColor(channel) = wsum / std::max(0.001, wdiv) * targetTime;
      }
    }
  }
}


} // namespace hdr
} // namespace aliceVision
