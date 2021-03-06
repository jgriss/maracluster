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
 
#ifndef MATRIX_LOADER_H
#define MATRIX_LOADER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>

#include "SpectrumFileList.h"
#include "PvalueTriplet.h"

#include <cerrno>
#include <boost/iostreams/device/mapped_file.hpp>

class MatrixLoader {
 public:
  MatrixLoader() : edgesAvailable_(false) {}
  
  long long numPvals_;
  
  bool initStream(const std::string& matrixFN);
  bool nextEdge(ScanId& row, ScanId& col, double& value);
  bool nextNEdges(unsigned int n, std::vector<PvalueTriplet>& pvec);
  
  bool hasEdgesAvailable() { return edgesAvailable_; }
 protected:
  SpectrumFileList fileList_;
  std::ifstream matrixStream_;
  
  static long long estimateNumPvals(const std::string& pvalFN);
  static long long getFileSize(const std::string& pvalFN);
  
  boost::iostreams::mapped_file mmap_;
  const char* f_;
  const char* l_;
  bool edgesAvailable_;
};

#endif // MATRIX_LOADER_H
