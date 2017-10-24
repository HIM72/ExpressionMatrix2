#include "Lsh.hpp"
#include "ExpressionMatrixSubset.hpp"
#include "SimilarPairs.hpp"
#include "timestamp.hpp"
using namespace ChanZuckerberg;
using namespace ExpressionMatrix2;

#include <boost/chrono.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include "fstream.hpp"


Lsh::Lsh(
    const ExpressionMatrixSubset& expressionMatrixSubset,
    size_t lshCount,                // Number of LSH hyperplanes
    uint32_t seed                   // Seed to generate LSH hyperplanes.
    )
{
    cout << timestamp << "Generating LSH vectors." << endl;
    generateLshVectors(expressionMatrixSubset.geneCount(), lshCount, seed);

    cout << timestamp << "Computing cell LSH signatures." << endl;
    computeCellLshSignatures(expressionMatrixSubset);
}



// Generate the LSH vectors.
void Lsh::generateLshVectors(
    size_t geneCount,
    size_t lshCount,                // Number of LSH hyperplanes
    uint32_t seed                   // Seed to generate LSH hyperplanes.
)
{
    // Prepare to generate normally vector distributed components.
    using RandomSource = boost::mt19937;
    using NormalDistribution = boost::normal_distribution<>;
    RandomSource randomSource(seed);
    NormalDistribution normalDistribution;
    boost::variate_generator<RandomSource, NormalDistribution> normalGenerator(randomSource, normalDistribution);

    // Allocate space for the LSH vectors.
    lshVectors.resize(geneCount, vector<double>(lshCount, 0.));

    // Sum of the squares of the components of each of the LSH vectors.
    vector<double> normalizationFactor(lshCount, 0.);



    // Loop over genes.
    for(size_t geneId = 0; geneId<lshCount; geneId++) {

        // For this gene, generate the components of all the LSH vectors.
        for(size_t lshVectorId = 0; lshVectorId<lshCount; lshVectorId++) {

            const double x = normalGenerator();
            lshVectors[geneId][lshVectorId] = x;

            // Update the sum of squares for this LSH vector.
            normalizationFactor[lshVectorId] += x*x;
        }
    }

    // Normalize each of the LSH vectors.
    for(auto& f: normalizationFactor) {
        f = 1. / sqrt(f);
    }
    for(size_t geneId = 0; geneId<lshCount; geneId++) {
        for(size_t lshVectorId = 0; lshVectorId<lshCount; lshVectorId++) {
            lshVectors[geneId][lshVectorId] *= normalizationFactor[lshVectorId];
        }
    }

}



// Compute the LSH signatures of all cells in the cell set we are using.
void Lsh::computeCellLshSignatures(const ExpressionMatrixSubset& expressionMatrixSubset)
{
    // Get the number of LSH vectors.
    CZI_ASSERT(!lshVectors.empty());
    const size_t lshCount = lshVectors.front().size();

    // Get the number of genes and cells in the gene set and cell set we are using.
    const auto geneCount = expressionMatrixSubset.geneCount();
    const auto cellCount = expressionMatrixSubset.cellCount();
    CZI_ASSERT(lshVectors.size() == geneCount);

    // Compute the sum of the components of each lsh vector.
    // It is needed below to compute the contribution of the
    // expression counts that are zero.
    vector<double> lshVectorsSums(lshCount, 0.);
    for(GeneId localGeneId=0; localGeneId!=geneCount; localGeneId++) {
        const auto& v = lshVectors[localGeneId]; // Components of all LSH vectors for this gene.
        CZI_ASSERT(v.size() == lshCount);
        for(size_t i=0; i<lshCount; i++) {
            lshVectorsSums[i] += v[i];
        }
    }

    // Initialize the cell signatures.
    cout << timestamp << "Initializing cell LSH signatures." << endl;
    signatures.resize(cellCount, BitSet(lshCount));

    // Vector to contain, for a single cell, the scalar products of the shifted
    // expression vector for the cell with all of the LSH vectors.
    vector<double> scalarProducts(lshCount);



    // Loop over all the cells in the cell set we are using.
    // The CellId is local to the cell set we are using.
    cout << timestamp << "Computation of cell LSH signatures begins." << endl;
    size_t nonZeroExpressionCount = 0;
    const auto t0 = boost::chrono::steady_clock::now();
    for(CellId localCellId=0; localCellId<cellCount; localCellId++) {
        if((localCellId % 1000) == 0) {
            cout << timestamp << "Working on cell " << localCellId << " of " << cellCount << endl;
        }

        // Compute the mean of the expression vector for this cell.
        const ExpressionMatrixSubset::Sum& sum = expressionMatrixSubset.sums[localCellId];
        const double mean = sum.sum1 / double(geneCount);

        // If U is one of the LSH vectors, we need to compute the scalar product
        // s = X*U, where X is the cell expression vector, shifted to zero mean:
        // X = x - mean,
        // mean = sum(x)/geneCount (computed above).
        // We get:
        // s = (x-mean)*U = x*U - mean*U = x*U - mean*sum(U)
        // We computed sum(U) above and stored it in lshVectorSums for
        // each of the LSH vectors.
        // Initialize the scalar products for this cell
        // with all of the LSH vectors to -mean*sum(U).
        for(size_t i=0; i<lshCount; i++) {
            scalarProducts[i] = -mean * lshVectorsSums[i];
        }

        // Now add to each scalar product the x*U portion.
        // For performance, the loop over genes is outside,
        // which gives better memory locality.
        // Add the contributions of the non-zero expression counts for this cell.
        for(const auto& p : expressionMatrixSubset.cellExpressionCounts[localCellId]) {
            const GeneId localGeneId = p.first;
            const double count = double(p.second);
            ++nonZeroExpressionCount;

            // Add the contribution of this gene to the scalar products.
            const auto& v = lshVectors[localGeneId];
            CZI_ASSERT(v.size() == lshCount);
            for(size_t i=0; i<lshCount; i++) {
                scalarProducts[i] += count * v[i];
            }
        }

        // Set to 1 the signature bits corresponding to positive scalar products.
        auto& cellSignature = signatures[localCellId];
        for(size_t i=0; i<lshCount; i++) {
            if(scalarProducts[i]>0.) {
                cellSignature.set(i);
            }
        }

    }
    const auto t1 = boost::chrono::steady_clock::now();
    cout << timestamp << "Computation of cell LSH signatures ends." << endl;
    cout << "Processed " << nonZeroExpressionCount << " non-zero expression counts for ";
    cout << geneCount << " genes and " << cellCount << " cells." << endl;
    cout << "Average expression matrix sparsity is " <<
        double(nonZeroExpressionCount) / (double(geneCount) * double(cellCount)) << endl;
    const double t01 = 1.e-9 * double((boost::chrono::duration_cast<boost::chrono::nanoseconds>(t1 - t0)).count());
    cout << "Computation of LSH cell signatures took " << t01 << "s." << endl;
    cout << "    Seconds per cell " << t01 / cellCount << endl;
    cout << "    Seconds per non-zero expression matrix entry " << t01/double(nonZeroExpressionCount) << endl;
    cout << "    Seconds per inner loop iteration " << t01 / (double(nonZeroExpressionCount) * double(lshCount)) << endl;
    cout << "    Gflop/s " << 2. * 1e-9 * double(nonZeroExpressionCount) * double(lshCount) / t01 << endl;

}
