#include <scai/lama.hpp>

#include <scai/lama/matrix/all.hpp>
#include <scai/lama/matutils/MatrixCreator.hpp>

#include <scai/dmemo/BlockDistribution.hpp>
#include <scai/dmemo/Distribution.hpp>

#include <scai/hmemo/Context.hpp>
#include <scai/hmemo/HArray.hpp>

#include <scai/utilskernel/LArray.hpp>
#include <scai/lama/Vector.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <memory>
#include <cstdlib>
#include <chrono>

#include "MeshGenerator.h"
#include "FileIO.h"
#include "ParcoRepart.h"
#include "Settings.h"
#include "MultiLevel.h"
#include "LocalRefinement.h"
#include "SpectralPartition.h"

typedef double ValueType;
typedef int IndexType;


/**
 *  Examples of use:
 * 
 *  for reading from file "fileName" 
 * ./a.out --graphFile fileName --epsilon 0.05 --sfcRecursionSteps=10 --dimensions=2 --borderDepth=10  --stopAfterNoGainRounds=3 --minGainForNextGlobalRound=10
 * 
 * for generating a 10x20x30 mesh
 * ./a.out --generate --numX=10 --numY=20 --numZ=30 --epsilon 0.05 --sfcRecursionSteps=10 --dimensions=3 --borderDepth=10  --stopAfterNoGainRounds=3 --minGainForNextGlobalRound=10
 * 
 * !! for now, when reading a file --dimensions must be 2
 */

//----------------------------------------------------------------------------

int main(int argc, char** argv) {
	using namespace boost::program_options;
	options_description desc("Supported options");

	struct Settings settings;

	desc.add_options()
				("help", "display options")
				("version", "show version")
				("graphFile", value<std::string>(), "read graph from file")
				("coordFile", value<std::string>(), "coordinate file. If none given, assume that coordinates for graph arg are in file arg.xyz")
				("generate", "generate random graph. Currently, only uniform meshes are supported.")
				("dimensions", value<int>(&settings.dimensions)->default_value(settings.dimensions), "Number of dimensions of generated graph")
				("numX", value<int>(&settings.numX)->default_value(settings.numX), "Number of points in x dimension of generated graph")
				("numY", value<int>(&settings.numY)->default_value(settings.numY), "Number of points in y dimension of generated graph")
				("numZ", value<int>(&settings.numZ)->default_value(settings.numZ), "Number of points in z dimension of generated graph")
				("epsilon", value<double>(&settings.epsilon)->default_value(settings.epsilon), "Maximum imbalance. Each block has at most 1+epsilon as many nodes as the average.")
				("minBorderNodes", value<int>(&settings.minBorderNodes)->default_value(settings.minBorderNodes), "Tuning parameter: Minimum number of border nodes used in each refinement step")
				("stopAfterNoGainRounds", value<int>(&settings.stopAfterNoGainRounds)->default_value(settings.stopAfterNoGainRounds), "Tuning parameter: Number of rounds without gain after which to abort localFM. A value of 0 means no stopping.")
				//("sfcRecursionSteps", value<int>(&settings.sfcResolution)->default_value(settings.sfcResolution), "Tuning parameter: Recursion Level of space filling curve. A value of 0 causes the recursion level to be derived from the graph size.")
                                ("initialPartition", value<int>(&settings.initialPartition)->default_value(settings.initialPartition), "Parameter for different initial partition: 0 for the hilbert space filling curve, 1 for the pixeled method, 2 for spectral parition")
                                ("pixeledDetailLevel", value<int>(&settings.pixeledDetailLevel)->default_value(settings.pixeledDetailLevel), "The resolution for the pixeled partition or the spectral")
				("minGainForNextGlobalRound", value<int>(&settings.minGainForNextRound)->default_value(settings.minGainForNextRound), "Tuning parameter: Minimum Gain above which the next global FM round is started")
				("gainOverBalance", value<bool>(&settings.gainOverBalance)->default_value(settings.gainOverBalance), "Tuning parameter: In local FM step, choose queue with best gain over queue with best balance")
				("useDiffusionTieBreaking", value<bool>(&settings.useDiffusionTieBreaking)->default_value(settings.useDiffusionTieBreaking), "Tuning Parameter: Use diffusion to break ties in Fiduccia-Mattheyes algorithm")
				("useGeometricTieBreaking", value<bool>(&settings.useGeometricTieBreaking)->default_value(settings.useGeometricTieBreaking), "Tuning Parameter: Use distances to block center for tie breaking")
				("skipNoGainColors", value<bool>(&settings.skipNoGainColors)->default_value(settings.skipNoGainColors), "Tuning Parameter: Skip Colors that didn't result in a gain in the last global round")
				("multiLevelRounds", value<int>(&settings.multiLevelRounds)->default_value(settings.multiLevelRounds), "Tuning Parameter: How many multi-level rounds with coarsening to perform")
				;

	variables_map vm;
	store(command_line_parser(argc, argv).
			  options(desc).run(), vm);
	notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 0;
	}

	if (vm.count("version")) {
		std::cout << "Git commit " << version << std::endl;
		return 0;
	}

	if (vm.count("generate") && vm.count("file")) {
		std::cout << "Pick one of --file or --generate" << std::endl;
		return 0;
	}

	if (vm.count("generate") && (vm["dimensions"].as<int>() != 3)) {
		std::cout << "Mesh generation currently only supported for three dimensions" << std::endl;
		return 0;
	}

    IndexType N = -1; 		// total number of points
    IndexType edges= -1;        // number of edges

    scai::lama::CSRSparseMatrix<ValueType> graph; 	// the adjacency matrix of the graph
    std::vector<DenseVector<ValueType>> coordinates(settings.dimensions); // the coordinates of the graph

    std::vector<ValueType> maxCoord(settings.dimensions); // the max coordinate in every dimensions, used only for 3D

    scai::dmemo::CommunicatorPtr comm = scai::dmemo::Communicator::getCommunicatorPtr();

    /* timing information
     */
    std::chrono::time_point<std::chrono::system_clock> startTime;
     
    startTime = std::chrono::system_clock::now();
    
    if (comm->getRank() == 0)
	{
        std::cout<< "commit:"<< version<< " input:"<< ( vm.count("graphFile") ? vm["graphFile"].as<std::string>() :" generate") << std::endl;
	}

    std::string graphFile;
	
    if (vm.count("graphFile")) {
    	graphFile = vm["graphFile"].as<std::string>();
    	std::string coordFile;
    	if (vm.count("coordFile")) {
    		coordFile = vm["coordFile"].as<std::string>();
    	} else {
    		coordFile = graphFile + ".xyz";
    	}
    
        std::fstream f(graphFile);

        if(f.fail()){
            throw std::runtime_error("File "+ graphFile + " failed.");
        }
        
        f >> N >> edges;			// first line must have total number of nodes and edges
        
        // for 2D we do not know the size of every dimension
        settings.numX = N;
        settings.numY = 1;
        settings.numZ = 1;
        
        if (comm->getRank() == 0)
        {
            std::cout<< "Reading from file \""<< graphFile << "\" for the graph and \"" << coordFile << "\" for coordinates"<< std::endl;
        }

        // read the adjacency matrix and the coordinates from a file
        graph = ITI::FileIO<IndexType, ValueType>::readGraph( graphFile );
        scai::dmemo::DistributionPtr rowDistPtr = graph.getRowDistributionPtr();
        scai::dmemo::DistributionPtr noDistPtr( new scai::dmemo::NoDistribution( N ));
        assert(graph.getColDistribution().isEqual(*noDistPtr));

        if (comm->getRank() == 0) {
        	std::cout<< "Read " << N << " points." << std::endl;
        }
        
        coordinates = ITI::FileIO<IndexType, ValueType>::readCoords(coordFile, N, settings.dimensions );

        if (comm->getRank() == 0) {
        	std::cout << "Read coordinates." << std::endl;
        }

    }
    /*
     else if(vm.count("generate")){
    	if (settings.dimensions == 2) {
    		settings.numZ = 1;
    	}

        N = settings.numX * settings.numY * settings.numZ;
            
        maxCoord[0] = settings.numX;
        maxCoord[1] = settings.numY;
        maxCoord[2] = settings.numZ;

        std::vector<IndexType> numPoints(3); // number of points in each dimension, used only for 3D

        for (IndexType i = 0; i < 3; i++) {
        	numPoints[i] = maxCoord[i];
        }

        if( comm->getRank()== 0){
            std::cout<< "Generating for dim= "<< settings.dimensions << " and numPoints= "<< settings.numX << ", " << settings.numY << ", "<< settings.numZ << ", in total "<< N << " number of points" << std::endl;
            std::cout<< "\t\t and maxCoord= "<< maxCoord[0] << ", "<< maxCoord[1] << ", " << maxCoord[2]<< std::endl;
        }
        
        scai::dmemo::DistributionPtr rowDistPtr ( scai::dmemo::Distribution::getDistributionPtr( "BLOCK", comm, N) );
        scai::dmemo::DistributionPtr noDistPtr(new scai::dmemo::NoDistribution(N));
        graph = scai::lama::CSRSparseMatrix<ValueType>( rowDistPtr , noDistPtr );
        
        scai::dmemo::DistributionPtr coordDist ( scai::dmemo::Distribution::getDistributionPtr( "BLOCK", comm, N) );
        for(IndexType i=0; i<settings.dimensions; i++){
            coordinates[i].allocate(coordDist);
            coordinates[i] = static_cast<ValueType>( 0 );
        }

        // create the adjacency matrix and the coordinates
        ITI::MeshGenerator<IndexType, ValueType>::createStructured3DMesh_dist( graph, coordinates, maxCoord, numPoints);
        
        if(comm->getRank()==0){
            IndexType nodes= graph.getNumRows();
            IndexType edges= graph.getNumValues()/2;	
            std::cout<< "Generated random 3D graph with "<< nodes<< " and "<< edges << " edges."<< std::endl;
	}


    }
    */
    else{
    	std::cout << "Only input file as input. Call again with --graphFile" << std::endl;
    	return 0;
    }
    
    // time needed to get the input
    std::chrono::duration<double> inputTime = std::chrono::system_clock::now() - startTime;

    assert(N > 0);

    if (comm->getSize() > 0) {
    	settings.numBlocks = comm->getSize();
    }

    
    //----------
    
    scai::dmemo::DistributionPtr rowDistPtr = graph.getRowDistributionPtr();
    scai::dmemo::DistributionPtr noDistPtr( new scai::dmemo::NoDistribution( N ));
    
    DenseVector<IndexType> uniformWeights;
    
    ValueType cut;
    ValueType imbalance;
    
    settings.pixeledDetailLevel = 4;
    settings.minGainForNextRound = 10;
    settings.minBorderNodes = 10;
    settings.useGeometricTieBreaking = 1;
    
    std::string destPath = "./partResults/testInitial/blocks_"+std::to_string(settings.numBlocks)+"/";
    boost::filesystem::create_directories( destPath );   
    
    std::size_t found= graphFile.std::string::find_last_of("/");
    std::string logFile = destPath + "results_" + graphFile.substr(found+1)+ ".log";
    std::ofstream logF(logFile);
    std::ifstream f(graphFile);
    
    logF<< "Results for file " << graphFile << std::endl;
    logF<< "node= "<< N << " , edges= "<< edges << std::endl<< std::endl;
    settings.print( logF );
    if( comm->getRank()==0)    settings.print( std::cout );
    logF<< std::endl<< std::endl << "Only initial partition, no MultiLevel or LocalRefinement"<< std::endl << std::endl;

    IndexType dimensions = settings.dimensions;
    IndexType k = settings.numBlocks;
    
    std::chrono::time_point<std::chrono::system_clock> beforeInitialTime;
    std::chrono::duration<double> partitionTime;
    std::chrono::duration<double> finalPartitionTime;
    
    using namespace ITI;
    
    //------------------------------------------- hilbert/sfc
    
    // the partitioning may redistribute the input graph
    graph.redistribute( rowDistPtr, noDistPtr);
    for(int d=0; d<dimensions; d++){
        coordinates[d].redistribute( rowDistPtr );
    }    
    if(comm->getRank()==0) std::cout <<std::endl<<std::endl;
    
    beforeInitialTime =  std::chrono::system_clock::now();
    PRINT0( "Get a hilbert/sfc partition");
    // get a hilbertPartition
    scai::lama::DenseVector<IndexType> hilbertPartition = ParcoRepart<IndexType, ValueType>::hilbertPartition( graph, coordinates, settings);
    
    partitionTime =  std::chrono::system_clock::now() - beforeInitialTime;
    
    assert( hilbertPartition.size() == N);
    assert( coordinates[0].size() == N);
    
    //aux::print2DGrid( graph, hilbertPartition );
    //if(dimensions==2){
    //    ITI::FileIO<IndexType, ValueType>::writeCoordsDistributed_2D( coordinates, N, destPath+"hilbertPart");
    //}
    cut = ParcoRepart<IndexType, ValueType>::computeCut( graph, hilbertPartition);
    imbalance = ParcoRepart<IndexType, ValueType>::computeImbalance( hilbertPartition, k);
    if(comm->getRank()==0){
        logF<< "-- Initial Hilbert/sfc partition time: " << partitionTime.count() <<  std::endl;
        logF<< "\tcut: " << cut << " , imbalance= "<< imbalance<< std::endl;
    }
    
    uniformWeights = DenseVector<IndexType>(graph.getRowDistributionPtr(), 1);
    ITI::MultiLevel<IndexType, ValueType>::multiLevelStep(graph, hilbertPartition, uniformWeights, coordinates, settings);
    
    finalPartitionTime =  std::chrono::system_clock::now() - beforeInitialTime;
    
    //if(dimensions==2){
    //   ITI::FileIO<IndexType, ValueType>::writeCoordsDistributed_2D( coordinates, N, destPath+"finalWithHilbert");
    //}
    cut = ParcoRepart<IndexType, ValueType>::computeCut( graph, hilbertPartition);
    imbalance = ParcoRepart<IndexType, ValueType>::computeImbalance( hilbertPartition, k);
    if(comm->getRank()==0){
        logF<< "   After multilevel, total time: " << finalPartitionTime.count() << std::endl;
        logF<< "\tfinal cut= "<< cut << ", final imbalance= "<< imbalance;
        logF  << std::endl  << std::endl; 
    }
    
    //------------------------------------------- pixeled
  
    // the partitioning may redistribute the input graph
    graph.redistribute(rowDistPtr, noDistPtr);
    for(int d=0; d<dimensions; d++){
        coordinates[d].redistribute( rowDistPtr );
    }    
    if(comm->getRank()==0) std::cout <<std::endl<<std::endl;
    
    beforeInitialTime =  std::chrono::system_clock::now();
    PRINT0( "Get a pixeled partition");
    // get a hilbertPartition
    scai::lama::DenseVector<IndexType> pixeledPartition = ParcoRepart<IndexType, ValueType>::pixelPartition( graph, coordinates, settings);
    
    partitionTime =  std::chrono::system_clock::now() - beforeInitialTime;
    
    assert( pixeledPartition.size() == N);
    assert( coordinates[0].size() == N);
    
    //aux::print2DGrid( graph, hilbertPartition );
    //if(dimensions==2){
    //    ITI::FileIO<IndexType, ValueType>::writeCoordsDistributed_2D( coordinates, N, destPath+"hilbertPart");
    //}
    cut = ParcoRepart<IndexType, ValueType>::computeCut( graph, pixeledPartition);
    imbalance = ParcoRepart<IndexType, ValueType>::computeImbalance( pixeledPartition, k);
    if(comm->getRank()==0){
        logF<< "-- Initial pixel partition time: " << partitionTime.count() <<  std::endl;
        logF<< "\tcut: " << cut << " , imbalance= "<< imbalance<< std::endl;
    }
    
    uniformWeights = DenseVector<IndexType>(graph.getRowDistributionPtr(), 1);
    ITI::MultiLevel<IndexType, ValueType>::multiLevelStep(graph, pixeledPartition, uniformWeights, coordinates, settings);
    
    finalPartitionTime =  std::chrono::system_clock::now() - beforeInitialTime;
    
    //if(dimensions==2){
    //   ITI::FileIO<IndexType, ValueType>::writeCoordsDistributed_2D( coordinates, N, destPath+"finalWithHilbert");
    //}
    cut = ParcoRepart<IndexType, ValueType>::computeCut( graph, pixeledPartition);
    imbalance = ParcoRepart<IndexType, ValueType>::computeImbalance( pixeledPartition, k);
    if(comm->getRank()==0){
        logF<< "   After multilevel, total time: " << finalPartitionTime.count() << std::endl;
        logF<< "\tfinal cut= "<< cut << ", final imbalance= "<< imbalance;
        logF  << std::endl  << std::endl; 
    }
 
    //------------------------------------------- spectral
    
    // the partitioning may redistribute the input graph
    graph.redistribute(rowDistPtr, noDistPtr);
    for(int d=0; d<dimensions; d++){
        coordinates[d].redistribute( rowDistPtr );
    }
    if(comm->getRank()==0) std::cout <<std::endl<<std::endl;
    PRINT0("Get a spectral partition");

    beforeInitialTime =  std::chrono::system_clock::now();
    // get initial spectral partition
    scai::lama::DenseVector<IndexType> spectralPartition = SpectralPartition<IndexType, ValueType>::getPartition( graph, coordinates, settings);
      
    partitionTime =  std::chrono::system_clock::now() - beforeInitialTime;
    
    assert( spectralPartition.size() == N);
    assert( coordinates[0].size() == N);
    //aux::print2DGrid( graph, spectralPartition );
    //if(dimensions==2){
    //    ITI::FileIO<IndexType, ValueType>::writeCoordsDistributed_2D( coordinates, N, destPath+"spectralPart");
    //}
    cut = ParcoRepart<IndexType, ValueType>::computeCut( graph, spectralPartition);
    imbalance = ParcoRepart<IndexType, ValueType>::computeImbalance( spectralPartition, k);
    if(comm->getRank()==0){
        logF<< "-- Initial Spectral partition " << std::endl;
        logF<< "\tcut: " << cut << " , imbalance= "<< imbalance;
    }
    
    uniformWeights = DenseVector<IndexType>(graph.getRowDistributionPtr(), 1);
    ITI::MultiLevel<IndexType, ValueType>::multiLevelStep(graph, spectralPartition, uniformWeights, coordinates, settings);
    
    finalPartitionTime =  std::chrono::system_clock::now() - beforeInitialTime;
    
    //if(dimensions==2){
    //    ITI::FileIO<IndexType, ValueType>::writeCoordsDistributed_2D( coordinates, N, destPath+"finalWithSpectral");
    //}
    cut = ParcoRepart<IndexType, ValueType>::computeCut( graph, spectralPartition);
    imbalance = ParcoRepart<IndexType, ValueType>::computeImbalance( spectralPartition, k);
    if(comm->getRank()==0){
        logF<< "   After multilevel, total time: " << finalPartitionTime.count() << std::endl;
        logF<< "\tfinal cut= "<< cut << ", final imbalance= "<< imbalance;
        logF  << std::endl  << std::endl; 
    }
    
    
    logF.close();
    if(comm->getRank()==0){
        std::cout<< "Output files written in " << destPath << " in file "<< logFile <<std::endl;
    }
    /*
    if (comm->getRank() == 0) {
        std::cout<< "commit:"<< version<< " input:"<< ( vm.count("graphFile") ? vm["graphFile"].as<std::string>() :"generate");
        std::cout<< " nodes:"<< N<< " dimensions:"<< settings.dimensions <<" k:" << settings.numBlocks;
        std::cout<< " epsilon:" << settings.epsilon << " minBorderNodes:"<< settings.minBorderNodes;
        std::cout<< " minGainForNextRound:" << settings.minGainForNextRound;
        std::cout<< " stopAfterNoGainRounds:"<< settings.stopAfterNoGainRounds << std::endl;
        
        std::cout<< "Cut is: "<< cut<< " and imbalance: "<< imbalance << std::endl;
        std::cout<<"inputTime:" << inputT << " partitionTime:" << partT <<" reportTime:"<< repT << std::endl;
    }
    */
}