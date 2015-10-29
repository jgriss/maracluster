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
 
#ifndef BINARY_INTERFACE_H
#define BINARY_INTERFACE_H

#include <vector>
#include <cerrno>
#include <fstream>

#include <boost/iostreams/device/mapped_file.hpp>

// TODO: add exception handling
class BinaryInterface {
 public:
  template <typename Type>
  static void write(const std::vector<Type>& vec, const std::string& outputFN, 
                    bool append) {
    if (vec.size() > 0) {
      std::ofstream outfile;
      if (append) {
        outfile.open(outputFN.c_str(), std::ios_base::app | std::ios_base::binary);
      } else {
        outfile.open(outputFN.c_str(), std::ios_base::out | std::ios_base::binary);
      }
      if (outfile.is_open()) {
        const char* pointer = reinterpret_cast<const char*>(&vec[0]);
        size_t bytes = vec.size() * sizeof(vec[0]);
        outfile.write(pointer, bytes);
      }
    }
  }
  
  template <typename Type>
  static void read(const std::string& inputFN, std::vector<Type>& vec) {
    boost::iostreams::mapped_file mmap(inputFN,
              boost::iostreams::mapped_file::readonly);
    
    const char* f = mmap.const_data();
    const char* l = f + mmap.size();
    
    errno = 0;
    Type tmp;
    while (errno == 0 && f && f<=(l-sizeof(tmp)) ) {
      memcpy(&tmp, f, sizeof(tmp));
      f += sizeof(tmp);
      vec.push_back(tmp);
    }
  }
};

#endif
