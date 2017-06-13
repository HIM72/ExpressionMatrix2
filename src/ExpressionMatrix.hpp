// Class to describe an RNA expression matrix.

#ifndef CZI_EXPRESSION_MATRIX2_EXPRESSION_MATRIX_H
#define CZI_EXPRESSION_MATRIX2_EXPRESSION_MATRIX_H

// CZI.
#include "Cell.hpp"
#include "CellSets.hpp"
#include "HttpServer.hpp"
#include "Ids.hpp"
#include "MemoryMappedVector.hpp"
#include "MemoryMappedVectorOfLists.hpp"
#include "MemoryMappedVectorOfVectors.hpp"
#include "MemoryMappedStringTable.hpp"

// Boost libraries.
#include <boost/shared_ptr.hpp>

// Standard library, partially injected in the ExpressionMatrix2 namespace.
#include "map.hpp"
#include "string.hpp"
#include "utility.hpp"
#include <limits>

namespace ChanZuckerberg {
    namespace ExpressionMatrix2 {

        class CellSimilarityGraph;
        class ExpressionMatrix;
        class ExpressionMatrixCreationParameters;
        class GraphCreationParameters;

    }
}



// Class used to store various parameters that control the initial creation of
// the ExpressionMatrix.
class ChanZuckerberg::ExpressionMatrix2::ExpressionMatrixCreationParameters {
public:

    // The following parameters control the capacity of various hash tables
    // use to store strings.
    // These capacities are hard limits: after the capacity is reached,
    // inserting a new element triggers an endless loop
    // (because we use open addressing hash tables without rehashing and without checks).
    // For good performance of these hash tables, these capacities
    // should equal at least twice the actual expected number of strings
    // of each type that will be stored.
    uint64_t geneCapacity = 1<<18;              // Controls the maximum number of genes.
    uint64_t cellCapacity = 1<<24;              // Controls the maximum number of cells.
    uint64_t cellMetaDataNameCapacity = 1<<16;  // Controls the maximum number of distinct cell meta data name strings.
    uint64_t cellMetaDataValueCapacity = 1<<28; // Controls the maximum number of distinct cell meta data value strings.
};



// Class used to store various parameters that control the creation of
// a cell similarity graph.
class ChanZuckerberg::ExpressionMatrix2::GraphCreationParameters {
public:
    string cellSetName;
    string similarPairsName;
    double similarityThreshold;
    size_t maxConnectivity;
    GraphCreationParameters() {}
    GraphCreationParameters(
        string cellSetName,
        string similarPairsName,
        double similarityThreshold,
        size_t maxConnectivity
        ) :
        cellSetName(cellSetName),
        similarPairsName(similarPairsName),
        similarityThreshold(similarityThreshold),
        maxConnectivity(maxConnectivity)
    {
    }
};




class ChanZuckerberg::ExpressionMatrix2::ExpressionMatrix : public HttpServer {
public:


    // Construct a new expression matrix. All binary data for the new expression matrix
    // will be stored in the specified directory. If the directory does not exist,
    // it will be created. If the directory already exists, any previous
    // expression matrix stored in the directory will be overwritten by the new one.
    ExpressionMatrix(const string& directoryName, const ExpressionMatrixCreationParameters&);

    // Access a previously created expression matrix stored in the specified directory.
    ExpressionMatrix(const string& directoryName);

    // Add a gene.
    // This does nothing if the gene already exists.
    // Genes are are also automatically added by addCell
    // as they are encountered, but calling this makes sure even genes
    // with zero counts on all cells are added.
    void addGene(const string& geneName);

    // Add a cell to the expression matrix.
    // The meta data is passed as a vector of names and values, which are all strings.
    // The cell name should be entered as meta data "CellName".
    // The expression counts for each gene are passed as a vector of pairs
    // (gene names, count).
    // Returns the id assigned to this cell.
    // This changes the metaData vector so the CellName entry is the first entry.
    // It also changes the expression counts - it sorts them by decreasing count.
    CellId addCell(
        vector< pair<string, string> >& metaData,
        vector< pair<string, float> >& expressionCounts,
        size_t maxTermCountForApproximateSimilarityComputation
        );

    // Version of addCell that takes JSON as input.
    // The expected JSON can be constructed using Python code modeled from the following:
    // import json
    // cell = {'metaData': {'cellName': 'abc', 'key1': 'value1'}, 'expressionCounts': {'gene1': 10,'gene2': 20}}
    // jSonString = json.dumps(cell)
    // expressionMatrix.addCell(json.dumps(jSonString))
    // Note the cellName metaData entry is required.
    CellId addCell(
        const string&,
        size_t maxTermCountForApproximateSimilarityComputation);



    // Add cells from data in files with fields separated by commas or by other separators.
    // A fields can contain separators, as long as the entire field is quoted.
    // This requires two input files, one for expression counts and one for cell meta data.
    // The file for cell meta data is optional (if not available, specify an empty string as its name).
    // If the meta data file is missing, no cell meta data is created.
    // The separators for each file are specified as arguments to this function.
    // The expression counts file must have geneCount+1 rows and cellCount+1 columns,
    // with cell names in the first row and gene names in the first column,
    // and expression counts everywhere else.
    // The entry in the first column of the first row is ignored but must be present (can be empty).
    // The meta data file must contain cellCount+1 rows and m+1 columns,
    // where m is the number of meta data fields. Cell names are in the first column
    // and meta data field names are in the first row.
    // Again, the entry in the first column of the first row is ignored but must be present (can be empty).
    // An example of the two files follow:
    // Expression counts file:
    // Dontcare,Cell1,Cell2,Cell3
    // Gene1,10,20,30
    // Gene2,30,40,50
    // Meta data file:
    // Dontcare,Name1,Name2
    // Cell1,abc,def
    // Cell2,123,456
    // Cell3,xyz,uv
    void addCells(
        const string& expressionCountsFileName,
        const string& expressionCountsFileSeparators,
        const string& metaDataFileName,
        const string& metaDataFileSeparators,
        size_t maxTermCountForApproximateSimilarityComputation
        );


    // Return the number of genes.
    GeneId geneCount() const
    {
        return GeneId(geneNames.size());
    }

    // Return the number of cells.
    CellId cellCount() const
    {
        return CellId(cellMetaData.size());
    }

    // Return the value of a specified meta data field for a given cell.
    // Returns an empty string if the cell does not have the specified meta data field.
    string getMetaData(CellId, const string& name) const;
    string getMetaData(CellId, StringId) const;

    // Set a meta data (name, value) pair for a given cell.
    // If the name already exists for that cell, the value is replaced.
    void setMetaData(CellId, const string& name, const string& value);
    void setMetaData(CellId, StringId nameId, const string& value);
    void setMetaData(CellId, StringId nameId, StringId valueId);

    // Compute a sorted histogram of a given meta data field.
    void histogramMetaData(
        const CellSets::CellSet& cellSet,
        StringId metaDataNameId,
        vector< pair<string, size_t> >& sortedHistogram) const;

    // Compute the similarity between two cells given their CellId.
    // The similarity is the correlation coefficient of their
    // expression counts.
    double computeCellSimilarity(CellId, CellId) const;

    // Approximate but fast computation of the similarity between two cells.
    double computeApproximateCellSimilarity(CellId, CellId) const;

    // Compute a histogram of the difference between approximate and exact similarity,
    // looping over all pairs. This is O(N**2) slow.
    void analyzeAllPairs() const;

    // Find similar cell pairs by looping over all pairs. This is O(N**2) slow.
    void findSimilarPairs0(
        const string& name,         // The name of the SimilarPairs object to be created.
        size_t k,                   // The maximum number of similar pairs to be storeed for each cell.
        double similarityThreshold, // The minimum similarity for a pair to be stored.
        bool useExactSimilarity     // Use exact of approximate cell similarity computation.
        );


    // Dump cell to csv file a set of similar cell pairs.
    void writeSimilarPairs(const string& name) const;

    // Create a new graph.
    // Graphs are not persistent (they are stored in memory only).
    void createCellSimilarityGraph(
        const string& graphName,            // The name of the graph to be created. This is used as a key in the graph map.
        const string& cellSetName,          // The cell set to be used.
        const string& similarPairsName,     // The name of the SimilarPairs object to be used to create the graph.
        double similarityThreshold,         // The minimum similarity to create an edge.
        size_t k                           // The maximum number of neighbors (k of the k-NN graph).
     );


private:

    // The directory that contains the binary data for this Expression matrix.
    string directoryName;

    // A StringTable containing the gene names.
    // Given a GeneId (an integer), it can find the gene name.
    // Given the gene name, it can find the corresponding GeneId.
    MemoryMapped::StringTable<GeneId> geneNames;

    // Vector containing fixed size information for each cell.
    // Variable size information (meta data and expression counts)
    // are stored separately - see below.
    MemoryMapped::Vector<Cell> cells;

    // A StringTable containing the cell names.
    // Given a CellId (an integer), it can find the cell name.
    // Given the cell name, it can find the corresponding CellId.
    // The name of reach cell is also stored as the first entry
    // in the meta data for the cell, called "cellName".
    MemoryMapped::StringTable<GeneId> cellNames;

    // The meta data for each cell.
    // For each cell we store pairs of string ids for each meta data (name, value) pair.
    // The corresponding strings are stored in cellMetaDataNames and cellMetaDataValues.
    // The first (name, value) pair for each cell contains name = "CellName"
    // and value = the name of the cell.
    MemoryMapped::VectorOfLists< pair<StringId, StringId> > cellMetaData;
    MemoryMapped::StringTable<StringId> cellMetaDataNames;
    MemoryMapped::StringTable<StringId> cellMetaDataValues;

    // The number of cells that use each of the cell meta data names.
    // This is maintained to always have the same size as cellMetaDataNames,
    // and it is indexed by the StringId.
    MemoryMapped::Vector<CellId> cellMetaDataNamesUsageCount;
    void incrementCellMetaDataNameUsageCount(StringId);
    void decrementCellMetaDataNameUsageCount(StringId);

    // The expression counts for each cell. Stored in sparse format,
    // each with the GeneId it corresponds to.
    // For each cell, they are stored sorted by increasing GeneId.
    // This is indexed by the CellId.
    MemoryMapped::VectorOfVectors<pair<GeneId, float>, uint64_t> cellExpressionCounts;

    // Return the raw cell count for a given cell and gene.
    // This does a binary search in the cellExpressionCounts for the given cell.
    float getExpressionCount(CellId, GeneId) const;

    // We also separately store the largest expression counts for each cell.
    // This is organized in the same way as cellExpressionCounts above.
    // This is used for fast, approximate computations of cell similarities.
    // The threshold for storing an expression count is different for each cell.
    // For each cell, we store in the Cell object the number of expression
    // counts neglected and the value of the largest expression count neglected.
    // With these we compute error bounds for approximate similarity
    // computations.
    MemoryMapped::VectorOfVectors<pair<GeneId, float>, uint64_t> largeCellExpressionCounts;

    // Functions used to implement HttpServer functionality.
    void processRequest(const vector<string>& request, ostream& html);
    typedef void (ExpressionMatrix::*ServerFunction)(const vector<string>& request, ostream& html);
    map<string, ServerFunction> serverFunctionTable;
    void fillServerFunctionTable();
    void writeNavigation(ostream& html);
    void writeNavigation(ostream& html, const string& text, const string& url);
    void exploreSummary(const vector<string>& request, ostream& html);
    void writeHashTableAnalysis(ostream&) const;
    void exploreGene(const vector<string>& request, ostream& html);
    void exploreCell(const vector<string>& request, ostream& html);
    ostream& writeCellLink(ostream&, CellId, bool writeId=false);
    ostream& writeCellLink(ostream&, const string& cellName, bool writeId=false);
    ostream& writeGeneLink(ostream&, GeneId, bool writeId=false);
    ostream& writeGeneLink(ostream&, const string& geneName, bool writeId=false);
    ostream& writeMetaDataSelection(ostream&, const string& selectName, bool multiple) const;
    ostream& writeMetaDataSelection(ostream&, const string& selectName, const set<string>& selected, bool multiple) const;
    ostream& writeMetaDataSelection(ostream&, const string& selectName, const vector<string>& selected, bool multiple) const;
    void compareTwoCells(const vector<string>& request, ostream& html);
    void exploreCellSets(const vector<string>& request, ostream& html);
    void exploreCellSet(const vector<string>& request, ostream& html);
    void createCellSetUsingMetaData(const vector<string>& request, ostream& html);
    void createCellSetIntersectionOrUnion(const vector<string>& request, ostream& html);
    ostream& writeCellSetSelection(ostream& html, const string& selectName, bool multiple) const;
    ostream& writeCellSetSelection(ostream& html, const string& selectName, const set<string>& selected, bool multiple) const;
    ostream& writeGraphSelection(ostream& html, const string& selectName, bool multiple) const;
    void removeCellSet(const vector<string>& request, ostream& html);
    void exploreGraphs(const vector<string>& request, ostream& html);
    void compareGraphs(const vector<string>& request, ostream& html);
    void exploreGraph(const vector<string>& request, ostream& html);
    void clusterDialog(const vector<string>& request, ostream& html);
    void cluster(const vector<string>& request, ostream& html);
    void createNewGraph(const vector<string>& request, ostream& html);
    void removeGraph(const vector<string>& request, ostream& html);
    void getAvailableSimilarPairs(vector<string>&) const;
    void exploreMetaData(const vector<string>& request, ostream& html);
    void metaDataHistogram(const vector<string>& request, ostream& html);
    void metaDataContingencyTable(const vector<string>& request, ostream& html);
    void removeMetaData(const vector<string>& request, ostream& html);



    // Class used by exploreGene.
    class ExploreGeneData {
    public:
        CellId cellId;
        float rawCount;
        float count1;   // L1 normalized.
        float count2;   // L2 normalized.
        bool operator<(const ExploreGeneData& that) const
        {
            return count2 > that.count2;    // Greater counts comes first
        }
    };

    // Return a cell id given a string.
    // The string can be a cell name or a CellId (an integer).
    // Returns invalidCellId if the cell was not found.
    CellId cellIdFromString(const string& s);

    // Return a gene id given a string.
    // The string can be a gene name or GeneId (a string).
    // Returns invalidGeneId if the gene was not found.
    GeneId geneIdFromString(const string& s);

    // Functionality to define and maintain cell sets.
    CellSets cellSets;
public:

    // Create a new cell set that contains cells for which
    // the value of a specified meta data field matches
    // a given regular expression.
    // Return true if successful, false if a cell set with
    // the specified name already exists.
    bool createCellSetUsingMetaData(
        const string& cellSetName,          // The name of the cell set to be created.
        const string& metaDataFieldName,    // The name of the meta data field to be used.
        const string& regex                 // The regular expression that must be matched for a cell to be added to the set.
        );

    // Create a new cell set as the intersection or union of two or more existing cell sets.
    // The input cell sets are specified comma separated in the first argument.
    // Return true if successful, false if one of the input cell sets does not exist
    // or the output cell set already exists.
    bool createCellSetIntersection(const string& inputSets, const string& outputSet);
    bool createCellSetUnion(const string& inputSets, const string& outputSet);
    bool createCellSetIntersectionOrUnion(const string& inputSets, const string& outputSet, bool doUnion);


    // The cell similarity graphs.
    // This is not persistent (lives in memory only).
    map<string, pair<GraphCreationParameters, boost::shared_ptr<CellSimilarityGraph> > > graphs;

    // Store the cluster ids in a graph in a meta data field.
    void storeClusterId(const string& metaDataName, const CellSimilarityGraph&);


};



#endif