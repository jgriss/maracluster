/******************************************************************************  
  Copyright 2015 Matthew The <matthew.the@scilifelab.se>
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
  
 ******************************************************************************/
 
#ifndef PVALUECALCULATOR_H
#define PVALUECALCULATOR_H

/**
This file provides functions for calculating pvalues using dynamic programming
Adapted from https://code.google.com/p/in-silico-mass-fingerprinting/
*/
#include <vector>
#include <map>
#include <string>

#include <cmath>
#include <algorithm>
#include <numeric>

#include <iostream>
#include <sstream>

#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/lu.hpp>

class PvalueCalculator {
 public:
  static unsigned int probDiscretizationLevels_;
  static const unsigned int kPolyfitDegree = 5u;
  static const double kMinProb, kMaxProb;
  static const unsigned int kMaxScoringPeaks = 40u, kMinScoringPeaks;
  static const bool kVariableScoringPeaks;
  
  inline unsigned int getNumScoringPeaks() const { return peakBins_.size(); }
  static unsigned int getMaxScoringPeaks(double mass) { 
    if (kVariableScoringPeaks) {
      return std::min(kMaxScoringPeaks, static_cast<unsigned int>(mass / 50.0));
    } else {
      return kMaxScoringPeaks;
    }
  }
  
  static unsigned int getMinScoringPeaks(double mass) { 
    if (kVariableScoringPeaks) {
      return std::min(kMinScoringPeaks, static_cast<unsigned int>(mass / 50.0));
    } else {
      return kMinScoringPeaks;
    }
  }
  
  void init(const std::vector<unsigned int>& _peakBins, const std::vector<double>& _peakProbs);
  void initFromPeakBins(const std::vector<unsigned int>& originalPeakBins, const std::vector<double>& peakDist);
  void initPolyfit(const std::vector<unsigned int>& peakBins, const std::vector<unsigned int>& peakScores, const std::vector<double>& polyfit);
  
  void computePvalVector();
  double computePval(const std::vector<unsigned int>& queryPeakBins, bool smoothing = false);
  
  void computePvalVectorPolyfit();
  double computePvalPolyfit(const std::vector<unsigned int>& queryPeakBins);
  
  void copyPolyfit(short* peakBins, short* peakScores, double* polyfit);
  void serialize(std::string& polyfitString, std::string& peakScorePairsString);
  void deserialize(std::string& polyfitString, std::string& peakScorePairsString);
  
  static bool pvalUnitTest();
  static bool pvalPolyfitUnitTest();
  static bool pvalUniformUnitTest();
  static bool binaryPeakMatchUnitTest();
  
  // needed for smoothing and unit tests
  inline static void setSeed(unsigned long s) { seed_ = s; }
  static unsigned long lcg_rand();
  static double lcg_rand_unif();
  
 private:
  
  std::vector<double> polyfit_;
  std::vector<double> sumProb_;
  std::vector<double> peakProbs_;
  
  std::vector<unsigned int> peakBins_;
  std::vector<unsigned int> peakScores_;
  
  unsigned int maxScore_;
  
  void binaryMatchPeakBins(const std::vector<unsigned int>& queryPeakBins, std::vector<bool>& d);
  double polyval(double x);
  
  // used for unit tests
  static inline bool isEqual(double a, double b) { return (std::abs(a - b) < 1e-5); }
  static unsigned long seed_;
};

#endif // PVALUECALCULATOR_H
