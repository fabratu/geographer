#pragma once

enum class InitialPartitioningMethods {SFC = 0, Pixel = 1, Spectral = 2, Multisection = 3};

struct Settings{
    IndexType dimensions= 3;
    IndexType numX = 32;
    IndexType numY = 32;
    IndexType numZ = 32;
    IndexType numBlocks = 2;
    IndexType minBorderNodes = 1;
    IndexType stopAfterNoGainRounds = 0;
    IndexType minGainForNextRound = 1;
    IndexType sfcResolution = 17;
    IndexType numberOfRestarts = 0;
    IndexType diffusionRounds = 20;
    IndexType multiLevelRounds = 0;
    IndexType coarseningStepsBetweenRefinement = 3;
    IndexType pixeledSideLen = 10;
    IndexType fileFormat = 0;   // 0 for METSI, 1 for MatrixMarket
    InitialPartitioningMethods initialPartition = InitialPartitioningMethods::SFC;
    bool useDiffusionTieBreaking = false;
    bool useGeometricTieBreaking = false;
    bool gainOverBalance = false;
    bool skipNoGainColors = false;
    bool writeDebugCoordinates = false;
    bool bisect = false;
    bool useExtent = false;
    double epsilon = 0.05;
    std::string fileName = "-";
    
    void print(std::ostream& out){
        IndexType numPoints = numX* numY* numZ;
        
        out<< "Setting: number of points= " << numPoints<< ", dimensions= "<< dimensions << ", minBorderNodes= "\
        << minBorderNodes << ", stopAfterNoGainRounds= "<< stopAfterNoGainRounds <<\
        ", minGainForNextRound= " << minGainForNextRound << ", sfcResolution= "<<\
        sfcResolution << ", epsilon= "<< epsilon << ", numBlocks= " << numBlocks << std::endl;
        out<< "multiLevelRounds: " << multiLevelRounds << std::endl;
        out<< "coarseningStepsBetweenRefinement: "<< coarseningStepsBetweenRefinement << std::endl;
        out<< "useDiffusionTieBreaking: " << useDiffusionTieBreaking <<std::endl;
        out<< "useGeometricTieBreaking: " << useGeometricTieBreaking <<std::endl;
        out<< "gainOverBalance: " << gainOverBalance << std::endl;
        out<< "skipNoGainColors: "<< skipNoGainColors << std::endl;
        out<< "pixeledSideLen: "<< pixeledSideLen << std::endl;
        out<< "fileFormat: "<< fileFormat << std::endl;
        if (initialPartition==InitialPartitioningMethods::SFC) {
            out<< "initial partition: hilbert curve" << std::endl;
        } else if (initialPartition==InitialPartitioningMethods::Pixel) {
            out<< "initial partition: pixels" << std::endl;
        } else if (initialPartition==InitialPartitioningMethods::Spectral) {
            out<< "initial partition: spectral" << std::endl;
        } else if (initialPartition==InitialPartitioningMethods::Multisection) {
            if (!bisect){
                out<< "initial partition: multisection" << std::endl;
            }else{
                out<< "initial partition: bisection" << std::endl;
            }
        } else {
            out<< "initial partition undefined" << std::endl;
        }
    }
};

