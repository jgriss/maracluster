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
 
#include "MSFileMerger.h"

using pwiz::msdata::MSDataPtr;
using pwiz::msdata::MSData;
using pwiz::msdata::MSDataFile;
using pwiz::msdata::SpectrumListSimplePtr;
using pwiz::msdata::SpectrumListSimple;
using pwiz::msdata::SpectrumListPtr;
using pwiz::msdata::SpectrumPtr;
using pwiz::msdata::Spectrum;

int MSFileMerger::maxMSFilePtrs_ = 60; // memory constrained
int MSFileMerger::maxSpectraPerFile_ = 500000; // memory constrained
int MSFileMerger::maxConsensusSpectraPerFile_ = 200000; // search engine memory constrained

int MSFileMerger::mergeMethod_ = 10;
bool MSFileMerger::normalize_ = true;

void MSFileMerger::parseClusterFileForMerge(const std::string& clusterFile) {
  std::ifstream clusterStream(clusterFile.c_str());
  std::string line;
  if (clusterStream.is_open()) {
    bool newCluster = true;
    size_t mergeCount = 0;
    while (getline(clusterStream, line)) {
      std::istringstream iss(line);
      std::string peptide, filepath;
      unsigned int scannr = 0;
      double qvalue = 0.0;
      
      iss >> filepath >> scannr;
      
      if (iss.fail()) {
        newCluster = true;
        iss.clear();
      } else {
        ScanId globalIdx = fileList_.getScanId(filepath, scannr);
        if (newCluster) {
          ScanMergeInfoSet s;
          s.mergedScanId = fileList_.getScanId(spectrumOutFN_, ++mergeCount);
          s.peptide = peptide;
          combineSets_.push_back(s);
          newCluster = false;
        }
        
        bool isDecoy = false;
        int charge = 0;
        combineSets_.back().push_back(
            ScanMergeInfo(globalIdx, qvalue, isDecoy, charge, peptide));
      }
    }
  }
}

/* Reads in clusters in the format (can only handle single file merges):
     scannr <tab> peptide <tab> charge <tab> qvalue */
void MSFileMerger::parseClusterFileForSingleFileMerge(
    const std::string& clusterFN, const std::string& spectrumInFN,
    const std::string& scanWeightsFN) {
  std::ifstream clusterStream(clusterFN.c_str());
  std::string line, tmp, id, peptide;
  
  // decide if we write the merger list to a file or not at all
  std::ofstream scanWeightsStream;
  bool writeMergeInfo = false;
  if (scanWeightsFN.size() > 0) {
    scanWeightsStream.open(scanWeightsFN.c_str());
    writeMergeInfo = true;
  }
  
  double qvalue;
  if (clusterStream.is_open()) {
    getline(clusterStream, line); // remove header line
    size_t mergeCount = 0;
    bool newCluster = true;
    while (getline(clusterStream, line)) {
      std::istringstream iss(line);
      unsigned int scannr, charge;
      std::string peptide;
      iss >> scannr >> peptide >> charge >> qvalue;
      
      if (iss.fail()) {
        newCluster = true;
        if (writeMergeInfo) scanWeightsStream << combineSets_.back();
      } else {
        ScanId globalIdx = fileList_.getScanId(spectrumInFN, scannr);
        unsigned int mergedIdx = ++mergeCount;
        if (newCluster) {
          ScanMergeInfoSet s;
          s.mergedScanId = fileList_.getScanId(spectrumOutFN_, mergedIdx);
          s.peptide = peptide;
          combineSets_.push_back(s);
          newCluster = false;
        }
        bool isDecoy = false;
        combineSets_.back().push_back(
            ScanMergeInfo(globalIdx, qvalue, isDecoy, charge, peptide));
        combineSets_.back().sortByScore();
      }
    }
  }
}

void MSFileMerger::mergeAllSpectra(const std::string& spectrumInFN) {
  MSDataFile msdOrig(spectrumInFN);
  SpectrumListPtr sl = msdOrig.run.spectrumListPtr;
  
  MSClusterMerge::init();
  
  std::vector<SpectrumPtr> spectra;
  for (unsigned int j = 0; j < sl->size(); ++j) {
    SpectrumPtr s = sl->spectrum(j, true);
    spectra.push_back(s);
  }
  
  SpectrumListSimplePtr mergedSpectra(new SpectrumListSimple);
  ScanId mergedScanId = fileList_.getScanId(spectrumOutFN_,1u);
  mergeSpectraSetMSCluster(spectra, mergedScanId, mergedSpectra);
  
  std::cerr << "Creating merged MSData file" << std::endl;
  MSData msdMerged;
  msdMerged.id = msdMerged.run.id = "merged_spectra";
  msdMerged.run.spectrumListPtr = mergedSpectra;

  writeMSData(msdMerged, spectrumOutFN_);
}

void MSFileMerger::mergeSpectraSetMSCluster(
    std::vector<SpectrumPtr>& spectra, 
    ScanId scanId, SpectrumListSimplePtr mergedSpectra) {
  std::vector< std::vector<BinnedMZIntensityPair> > cluster;
  std::vector<MassChargeCandidate> allMccs;
  bool first = true;
  SpectrumPtr consensusSpec;
  BOOST_FOREACH(SpectrumPtr s, spectra) {
    if (first) {
      consensusSpec = SpectrumPtr(new Spectrum(*s));
      first = false;
    }
    
    std::vector<MassChargeCandidate> mccs;
    SpectrumHandler::getMassChargeCandidates(s, mccs);
    allMccs.insert(allMccs.end(), mccs.begin(), mccs.end());
    
    std::vector<MZIntensityPair> mziPairs;
    SpectrumHandler::getMZIntensityPairs(s, mziPairs);
    if (normalize_) SpectrumHandler::normalizeIntensitiesMSCluster(mziPairs);
    
    std::vector<BinnedMZIntensityPair> binnedMziPairs;
    MSClusterMerge::binMZIntensityPairs(mziPairs, binnedMziPairs);
    cluster.push_back(binnedMziPairs);
  }
  std::vector<BinnedMZIntensityPair> mergedMziPairs;
  MSClusterMerge::merge(cluster, mergedMziPairs);
  
  std::vector<MZIntensityPair> mziPairs;
  MSClusterMerge::unbinMZIntensityPairs(mergedMziPairs, mziPairs);
  
  SpectrumHandler::setMZIntensityPairs(consensusSpec, mziPairs);
  
  std::vector<MassChargeCandidate> consensusMccs;
  MSClusterMerge::mergeMccs(allMccs, consensusMccs);
  /*consensusMccs = allMccs;*/
  
  addSpectrumWithMccs(consensusSpec, consensusMccs, scanId.scannr, mergedSpectra);
}

void MSFileMerger::mergeSpectra() {
  if (fileList_.getFilePaths().size() > 0 /*maxMSFilePtrs_*/) {
    mergeSpectraScalable();
  } else {
    mergeSpectraSmall();
  }
}

/* Merges spectra in-memory */
void MSFileMerger::mergeSpectraSmall() {  
  /* create the MSData object in memory */  
  std::cerr << "Creating container for merged spectra" << std::endl;
  MSData msdMerged;
  msdMerged.id = msdMerged.run.id = "merged_spectra";
  
  SpectrumListSimplePtr mergedSpectra(new SpectrumListSimple);
  
  std::cerr << "Merging spectra" << std::endl;
  int count = 0;
  int partIdx = 0;
  
  std::map<std::string, MSDataPtr> msDataPtrs;
  MSClusterMerge::init();
  
  std::sort(combineSets_.begin(), combineSets_.end(), ScanMergeInfoSet::lowerScannr);
  
  BOOST_FOREACH (ScanMergeInfoSet& mergeSet, combineSets_) {    
    std::vector<SpectrumPtr> spectra;
    BOOST_FOREACH (ScanMergeInfo scanMergeInfo, mergeSet.scans) {
      std::string filepath = fileList_.getFilePath(scanMergeInfo.scannr);
      unsigned int scannr = fileList_.getScannr(scanMergeInfo.scannr);
      
      if (msDataPtrs.find(filepath) == msDataPtrs.end()) {
        MSDataPtr msd(new MSDataFile(filepath));
        msDataPtrs[filepath] = msd;
        std::cerr << "Extracting spectra from " << filepath 
                  << ", numFilePointers:" << msDataPtrs.size() << std::endl;
      }
      
      SpectrumListPtr sl = msDataPtrs[filepath]->run.spectrumListPtr;
      
      size_t result = getSpectrumIdxFromScannr(sl, scannr);
      SpectrumPtr s = sl->spectrum(result, true);
      spectra.push_back(s);
    }
    mergeSpectraSetMSCluster(spectra, mergeSet.mergedScanId, mergedSpectra);
    
    if (++count % 5000 == 0)
      std::cerr << "Merged " << count << "/" << combineSets_.size() << std::endl;
      
    if (mergedSpectra->spectra.size() >= maxConsensusSpectraPerFile_ 
          || count == combineSets_.size()) {
      std::cerr << "Creating merged MSData file" << std::endl;
      msdMerged.run.spectrumListPtr = mergedSpectra;
      
      std::cerr << "#spectra: " << msdMerged.run.spectrumListPtr->size() << std::endl;
      ++partIdx;
      
      typedef std::pair<const std::string, MSDataPtr> FnMsDatPtrPair;
      BOOST_FOREACH (FnMsDatPtrPair& fnMsDatPtrPair, msDataPtrs) {
        msdMerged.fileDescription.sourceFilePtrs.push_back(
            fnMsDatPtrPair.second->fileDescription.sourceFilePtrs.front());
      }
      
      std::string partSpecOutFN = getPartFN(
          spectrumOutFN_, "part" + boost::lexical_cast<std::string>(partIdx));
      writeMSData(msdMerged, partSpecOutFN);
      
      mergedSpectra = SpectrumListSimplePtr(new SpectrumListSimple);
    }
  }
}


/**
 * Terminology:
 *   Batch: determines how many input files are processed before writing intermediate files 
 *          (depends on maxMSFilePtrs_)
 *   Bin: determines in how many files the intermediate files are split 
 *        (depends on maxSpectraPerFile_ and maxConsensusSpectraPerFile_)
 **/
void MSFileMerger::mergeSpectraScalable() {  
  std::map<ScanId, ScanId> scannrToMergedScannr;
  createScannrToMergedScannrMap(scannrToMergedScannr);
  
  numBatches_ = (fileList_.size()-1) / maxMSFilePtrs_ + 1;
  numMSFilePtrsPerBatch_ = fileList_.size() / numBatches_ + 1;
  
  unsigned int numSpectra = scannrToMergedScannr.size();
  size_t numClusterBinsIn = (numSpectra-1) / maxSpectraPerFile_ + 1;
  size_t numClusterBinsOut = 
      (combineSets_.size()-1) / maxConsensusSpectraPerFile_ + 1;
  numClusterBins_ = std::max(numClusterBinsIn, numClusterBinsOut);
  
  splitSpecFilesByConsensusSpec(scannrToMergedScannr);
  mergeSplitSpecFiles();
}

void MSFileMerger::createScannrToMergedScannrMap(
    std::map<ScanId, ScanId>& scannrToMergedScannr) {
  BOOST_FOREACH (ScanMergeInfoSet& mergeSet, combineSets_) {
    BOOST_FOREACH (ScanMergeInfo scanMergeInfo, mergeSet.scans) {
      scannrToMergedScannr[scanMergeInfo.scannr] = mergeSet.mergedScanId;
    }
  }
}

void MSFileMerger::splitSpecFilesByConsensusSpec(
    std::map<ScanId, ScanId>& scannrToMergedScannr) {
  for (size_t batchNr = 0; batchNr < numBatches_; ++batchNr) {
    std::vector<SpectrumListSimplePtr> spectrumListsAcc(numClusterBins_);
    for (size_t k = 0; k < numClusterBins_; ++k) {
      spectrumListsAcc[k] = SpectrumListSimplePtr(new SpectrumListSimple);
    }
    size_t startIdx = batchNr*numMSFilePtrsPerBatch_;
    size_t endIdx = (std::min)((batchNr+1)*numMSFilePtrsPerBatch_, fileList_.size());
  #pragma omp parallel for schedule(dynamic, 1)
    for (size_t i = startIdx; i < endIdx; ++i) {
      std::string filePath = fileList_.getFilePath(i);
      if (filePath == spectrumOutFN_) continue;
      std::cerr << "Splitting " << filePath
                << " (" << (i+1)*100/fileList_.size() << "%)" << std::endl;
      
      SpectrumListPtr sl;    
    #pragma omp critical (create_msdata)
      {  
        MSDataFile msd(filePath);
        sl = msd.run.spectrumListPtr;
      }
      
      std::vector<SpectrumListSimplePtr> spectrumLists(numClusterBins_);
      for (size_t k = 0; k < numClusterBins_; ++k) {
        spectrumLists[k] = SpectrumListSimplePtr(new SpectrumListSimple);
      }
      for (unsigned int j = 0; j < sl->size(); ++j) {
        SpectrumPtr s = sl->spectrum(j, true);
        unsigned int scannr = SpectrumHandler::getScannr(s);
        ScanId globalScannr = fileList_.getScanId(filePath, scannr);
        ScanId mergedScannr = scannrToMergedScannr[globalScannr];
        
        // since vectors are zero based and scannrs start at 1, we subtract 1
        unsigned int clusterBin = (mergedScannr.scannr-1) % numClusterBins_; 
        s->id = "scan=" + boost::lexical_cast<std::string>(hash_value(globalScannr));
        spectrumLists[clusterBin]->spectra.push_back(s);
      }
    #pragma omp critical (merge_speclists)
      {
        for (size_t k = 0; k < numClusterBins_; ++k) {
          spectrumListsAcc[k]->spectra.reserve(
              spectrumListsAcc[k]->spectra.size() + spectrumLists[k]->spectra.size() ); // preallocate memory
          spectrumListsAcc[k]->spectra.insert( 
              spectrumListsAcc[k]->spectra.end(), 
              spectrumLists[k]->spectra.begin(), 
              spectrumLists[k]->spectra.end() );
        }
      }
    }
    
    writeClusterBins(batchNr, spectrumListsAcc);
  }
  std::cerr << "Finished splitting ms2 files" << std::endl;
}

void MSFileMerger::mergeSplitSpecFiles() {
  MSClusterMerge::init();
  
  SpectrumListSimplePtr mergedSpectra(new SpectrumListSimple);
  unsigned int partIdx = 0u;
  
  for (size_t clusterBin = 0; clusterBin < numClusterBins_; ++clusterBin) {
    std::cerr << "Merging spectra in bin " << clusterBin+1 << "/" << numClusterBins_ << std::endl;
    
    mergeSpectraBin(clusterBin, mergedSpectra);
    
    while (mergedSpectra->size() > maxConsensusSpectraPerFile_ 
        || (mergedSpectra->size() > 0 && clusterBin == numClusterBins_ - 1)) {
      std::cerr << "Creating merged MSData file" << std::endl;
      MSData msdMerged;
      msdMerged.id = msdMerged.run.id = "merged_spectra";
      
      SpectrumListSimplePtr writeSpectra(new SpectrumListSimple);
      size_t idx = 0;
      BOOST_FOREACH (SpectrumPtr& s, mergedSpectra->spectra) {
        s->index = idx;
        writeSpectra->spectra.push_back(s);
        if (++idx >= maxConsensusSpectraPerFile_) break;
      }
      
      msdMerged.run.spectrumListPtr = writeSpectra;
      
      std::string partSpecOutFN = getPartFN(spectrumOutFN_, "part" + 
          boost::lexical_cast<std::string>(++partIdx));
      writeMSData(msdMerged, partSpecOutFN);
      
      mergedSpectra->spectra.erase(mergedSpectra->spectra.begin(), mergedSpectra->spectra.begin() + idx);
    }
  }
}

void MSFileMerger::mergeSpectraBin(size_t clusterBin,
    SpectrumListSimplePtr mergedSpectra) {
  std::vector<MSDataPtr> msdVector;
  for (unsigned int i = 0; i < numBatches_; ++i) {
    std::string partSpecOutFN = getPartFN(spectrumOutFN_, "part" +
          boost::lexical_cast<std::string>(clusterBin) + "_" + 
          boost::lexical_cast<std::string>(i));
    MSDataPtr msd(new MSDataFile(partSpecOutFN));
    msdVector.push_back(msd);
  }
  
  std::vector< std::vector<MergeScanIndex> > scanIndices(numBatches_);
  std::vector< std::pair< std::vector<SpectrumPtr>, ScanId> > spectra;
  
  for (unsigned int i = clusterBin; i < combineSets_.size(); i += numClusterBins_) {
    unsigned int posInCluster = 0;
    BOOST_FOREACH (ScanMergeInfo scanMergeInfo, combineSets_[i].scans) {
      ScanId scannr = scanMergeInfo.scannr;
      unsigned int fileIdx = fileList_.getFileIdx(scannr);
      //std::cerr << scannr << " " << i << " " << fileIdx << std::endl;
      SpectrumListPtr sl = msdVector[fileIdx / numMSFilePtrsPerBatch_]->run.spectrumListPtr;
      size_t result = getSpectrumIdxFromScannr(sl, hash_value(scannr));
      if (result >= sl->size()) {
        std::cerr << "  Warning: index " << result << " out of bounds: " 
              << fileList_.getFilePath(scannr) << ": " << scannr << std::endl;
      } else {
        scanIndices[fileIdx / numMSFilePtrsPerBatch_].push_back( MergeScanIndex(result, posInCluster, spectra.size()) );
        ++posInCluster;
      }
    }
    // initialize container for spectra to be merged for mergedScanId
    spectra.push_back(std::make_pair(std::vector<SpectrumPtr>(posInCluster), combineSets_[i].mergedScanId));
  }
  
  std::cerr << "  Processing consensus spectra." << std::endl;
  for (unsigned int j = 0; j < numBatches_; ++j) {
    std::cerr << "  Batch " << j+1 << "/" << numBatches_ << std::endl;
    SpectrumListPtr sl = msdVector[j]->run.spectrumListPtr;
    std::sort(scanIndices[j].begin(), scanIndices[j].end(), lessIndex);
    BOOST_FOREACH (MergeScanIndex msi, scanIndices[j]) {
      SpectrumPtr s = sl->spectrum(msi.spectrumIndex, true);
      spectra[msi.mergeIdx].first[msi.posInCluster] = s;
    }
  }
  std::cerr << "  Merging spectra" << std::endl;
#pragma omp parallel for schedule(dynamic, 100)
  for (size_t k = 0; k < spectra.size(); ++k) {
    if (spectra[k].first.size() > 0) {
      mergeSpectraSetMSCluster(spectra[k].first, spectra[k].second, mergedSpectra);
    }
  }
  
  for (unsigned int i = 0; i < numBatches_; ++i) {
    std::string partSpecOutFN = getPartFN(spectrumOutFN_, "part" + 
          boost::lexical_cast<std::string>(clusterBin) + "_" + 
          boost::lexical_cast<std::string>(i));
    remove(partSpecOutFN.c_str());
  }
}

void MSFileMerger::writeClusterBins(unsigned int batchIdx,
    std::vector<SpectrumListSimplePtr>& spectrumLists) {
#pragma omp parallel for schedule(dynamic, 1)
  for (unsigned int i = 0; i < numClusterBins_; ++i) {
    if (spectrumLists[i]->spectra.size() == 0) {
      continue; // in case there are so few spectra that not all bins are filled
    }
    
    int idx = -1;
    BOOST_FOREACH (SpectrumPtr& s, spectrumLists[i]->spectra) {
      s->index = ++idx;
    }
    
    MSData msdMerged;
    msdMerged.id = msdMerged.run.id = "merged_spectra";
    msdMerged.run.spectrumListPtr = spectrumLists[i];
    
    std::string partSpecOutFN = getPartFN(spectrumOutFN_, "part" + 
          boost::lexical_cast<std::string>(i) + "_" + 
          boost::lexical_cast<std::string>(batchIdx));
    
    writeMSData(msdMerged, partSpecOutFN);
  }
}

std::string MSFileMerger::getPartFN(const std::string& outputFN, 
                                    const std::string& partString) {
  size_t found = outputFN.find_last_of(".");
  std::string baseFN = outputFN.substr(0, found + 1);
  std::string outputFormat = outputFN.substr(found);
  return baseFN + partString + outputFormat;
}

/**************************************
 ** These functions are obsolete now **
 **************************************/

void MSFileMerger::mergeSpectraSet(std::vector<SpectrumPtr>& spectra, 
                                    SpectrumPtr& consensusSpec) {
  std::vector<MZIntensityPair> mziPairsIn;
  bool first = true;
  BOOST_FOREACH(SpectrumPtr s, spectra) {
    if (first) {
      consensusSpec = SpectrumPtr(new Spectrum(*s));
      SpectrumHandler::getMZIntensityPairs(consensusSpec, mziPairsIn);
      
      if (normalize_) SpectrumHandler::normalizeIntensitiesMSCluster(mziPairsIn);
      first = false;
    } else {        
      std::vector<MZIntensityPair> mziPairsFrom;
      SpectrumHandler::getMZIntensityPairs(s, mziPairsFrom);
      
      if (normalize_) SpectrumHandler::normalizeIntensitiesMSCluster(mziPairsFrom);
      double weight = 1.0;
      mergeTwoSpectra(mziPairsIn, mziPairsFrom, weight);
    }
  }

  if (mergeMethod_ == 11) {
    mziPairsIn.resize((std::min)(100u, 
        static_cast<unsigned int>(mziPairsIn.size())));
  }

  std::sort(mziPairsIn.begin(), mziPairsIn.end(), SpectrumHandler::lessMZ);
  SpectrumHandler::setMZIntensityPairs(consensusSpec, mziPairsIn);
}

void MSFileMerger::mergeTwoSpectra(std::vector<MZIntensityPair>& mziPairsIn, 
                                    std::vector<MZIntensityPair>& mziPairsFrom, 
                                    double weight) {
  bool useWeights = false;
  
  if (!useWeights) weight = 1.0;
  switch (mergeMethod_) {
    /* simply concatenate the data points of the second spectra into the
       first one */
    case 1: {
      SpectrumHandler::scaleIntensities(mziPairsFrom, weight);
      mziPairsIn.reserve( mziPairsIn.size() + mziPairsFrom.size() );
      mziPairsIn.insert( mziPairsIn.end(), mziPairsFrom.begin(),
                         mziPairsFrom.end() );
      break;
    }
    /* linearly interpolate the intensity of the second spectrum at the nodes 
       of the first spectrum and add them to the first spectrum, optionally  
       shift and scale the spectra to align them better (did not produce 
       better results in tests */
    case 2: {
      double winningShift = 0.0, winningScaling = 0.0;
      InterpolationMerge::findBestAffineTransform(mziPairsIn, mziPairsFrom,
          weight, winningShift, winningScaling);
      //std::cerr << "Winning shift = " << winningShift << 
      //             " winning scaling = " << winningScaling << std::endl;
      InterpolationMerge::merge(mziPairsIn, mziPairsFrom, weight,
          InterpolationMerge::AVGM, winningShift, winningScaling);
      break;
    }
    /* linearly interpolate both ways (s1->s2 and s2->s1) and concatenate */
    case 5: {
      InterpolationMerge::mergeViceVersa(mziPairsIn, mziPairsFrom, weight);
      break;
    }
    /* linearly interpolate (s2->s1) and take minimum intensity */
    case 6: {
      InterpolationMerge::merge(mziPairsIn, mziPairsFrom, weight,
                                InterpolationMerge::MINM);
      break;
    }
    /* linearly interpolate (s2->s1) and take maximum intensity */
    case 7: {
      InterpolationMerge::merge(mziPairsIn, mziPairsFrom, weight,  
                                InterpolationMerge::MAXM);
      break;
    }
    /* linearly interpolate (s2->s1) and merge peaks with similar peak rank */
    case 9: {
      bool useRegionRank = false;
      RankMerge::mergeMinMax(mziPairsIn, mziPairsFrom, weight, useRegionRank);
      break;
    }
    /* linearly interpolate (s2->s1) and merge peaks with similar local 
       (to 1 of the 10 XCorr regions) peak rank */
    case 10: {
      bool useRegionRank = true;
      RankMerge::mergeMinMax(mziPairsIn, mziPairsFrom, weight, useRegionRank);
      break;
    }
    /* cluster peaks and take center of gravity as new peak. N.B. this only
       works for peak picked spectra. */
    case 11: {
      ClusterMerge::merge(mziPairsIn, mziPairsFrom, weight);
      break;
    }
    default: {
      throw std::runtime_error("Method not found");
    }
  }
}
