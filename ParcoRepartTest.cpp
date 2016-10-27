#include <scai/lama.hpp>

#include <scai/lama/matrix/all.hpp>
#include <scai/lama/matutils/MatrixCreator.hpp>

#include <scai/dmemo/BlockDistribution.hpp>

#include <scai/hmemo/Context.hpp>
#include <scai/hmemo/HArray.hpp>

#include <scai/utilskernel/LArray.hpp>
#include <scai/lama/Vector.hpp>

#include <memory>
#include <cstdlib>

#include "ParcoRepart.h"
#include "gtest/gtest.h"

typedef double ValueType;
typedef int IndexType;

using namespace scai;

namespace ITI {

class ParcoRepartTest : public ::testing::Test {

};

TEST_F(ParcoRepartTest, testHilbertIndexUnitSquare) {
  const IndexType dimensions = 2;
  const IndexType n = 4;
  const IndexType recursionDepth = 5;
  ValueType tempArray[8] = {0.1,0.1, 0.1, 0.6, 0.7, 0.7, 0.8, 0.1};
  DenseVector<ValueType> coordinates(n*dimensions, 0);
  coordinates.setValues(scai::hmemo::HArray<ValueType>(8, tempArray));
  const std::vector<ValueType> minCoords({0,0});
  const std::vector<ValueType> maxCoords({1,1});

  std::vector<ValueType> indices(n);
  for (IndexType i = 0; i < n; i++) {
    indices[i] = ParcoRepart<IndexType, ValueType>::getHilbertIndex(coordinates, dimensions, i, recursionDepth ,minCoords, maxCoords);
    EXPECT_LE(indices[i], 1);
    EXPECT_GE(indices[i], 0);
  }
  EXPECT_LT(indices[0], indices[1]);
  EXPECT_LT(indices[1], indices[2]);
  EXPECT_LT(indices[2], indices[3]);
}

TEST_F(ParcoRepartTest, testPartitionerInterface) {
	IndexType nroot = 100;
	IndexType n = nroot * nroot;
  IndexType k = 10;
  scai::lama::CSRSparseMatrix<ValueType>a(n,n);
  scai::lama::MatrixCreator::fillRandom(a, 0.01);
  IndexType dim = 2;

	scai::lama::DenseVector<ValueType> coordinates(dim*n, 0);
	for (IndexType i = 0; i < nroot; i++) {
		for (IndexType j = 0; j < nroot; j++) {
 			coordinates.setValue(2*(i*nroot + j), i);
 			coordinates.setValue(2*(i*nroot + j)+1, j);
 		}
 	}

 	scai::lama::DenseVector<ValueType> partition = ParcoRepart<ValueType, ValueType>::partitionGraph(a, coordinates, dim,	k);

  EXPECT_EQ(partition.size(), n);
}

TEST_F(ParcoRepartTest, testPartitionBalance) {
  IndexType nroot = 100;
  IndexType n = nroot * nroot;
  IndexType k = 10;
  scai::lama::CSRSparseMatrix<ValueType>a(n,n);
  scai::lama::MatrixCreator::fillRandom(a, 0.01);
  IndexType dim = 2;
  ValueType epsilon = 0.05;

  scai::lama::DenseVector<ValueType> coordinates(dim*n, 0);
  for (IndexType i = 0; i < nroot; i++) {
    for (IndexType j = 0; j < nroot; j++) {
      coordinates.setValue(2*(i*nroot + j), i);
      coordinates.setValue(2*(i*nroot + j)+1, j);
    }
  }

  scai::lama::DenseVector<ValueType> partition = ParcoRepart<ValueType, ValueType>::partitionGraph(a, coordinates, dim,  k, epsilon);

  EXPECT_EQ(n, partition.size());
  EXPECT_EQ(0, partition.min().getValue<ValueType>());
  EXPECT_EQ(k-1, partition.max().getValue<ValueType>());
  EXPECT_TRUE(partition.getDistribution().isReplicated());//for now

  std::vector<IndexType> subsetSizes(k, 0);//probably replace with some Lama data structure later
  scai::utilskernel::LArray<ValueType> localPartition = partition.getLocalValues();
  for (IndexType i = 0; i < localPartition.size(); i++) {
    ValueType partID = localPartition[i];
    EXPECT_LE(partID, k);
    EXPECT_GE(partID, 0);
    subsetSizes[partID] += 1;
  }
  IndexType optSize = std::ceil(n / k);

  //in a distributed setting, this would need to be communicated and summed
  EXPECT_LE(*std::max_element(subsetSizes.begin(), subsetSizes.end()), (1+epsilon)*optSize);

}

} //namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}