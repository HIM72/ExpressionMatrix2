#include "ExpressionMatrix.hpp"
#include "Lsh.hpp"
#include "timestamp.hpp"
using namespace ChanZuckerberg;
using namespace ExpressionMatrix2;

#include "SignatureGraph.hpp"

#include "fstream.hpp"



// Check that a signature graph does not exist,
// and throw an exception if it does.
void ExpressionMatrix::checkSignatureGraphDoesNotExist(
    const string& signatureGraphName) const
{
    if(signatureGraphs.find(signatureGraphName) != signatureGraphs.end()) {
        throw runtime_error("Signature graph " + signatureGraphName + " already exists.");
    }

}

// Return a reference to a signature graph with a given name,
// and throw and exception if not found.
SignatureGraph& ExpressionMatrix::getSignatureGraph(const string& signatureGraphName)
{
    const auto it = signatureGraphs.find(signatureGraphName);
    if(it == signatureGraphs.end()) {
        throw runtime_error("Signature graph " + signatureGraphName + " does not exists.");
    }
    return *(it->second);
}


// All cells with the same signature are aggregated
// into a single vertex of the signature graph.
void ExpressionMatrix::createSignatureGraph(
    const string& signatureGraphName,
    const string& cellSetName,
    const string& lshName,
    size_t minCellCount)
{
    checkSignatureGraphDoesNotExist(signatureGraphName);

    // Locate the cell set and verify that it is not empty.
    const auto& it = cellSets.cellSets.find(cellSetName);
    if(it == cellSets.cellSets.end()) {
        throw runtime_error("Cell set " + cellSetName + " does not exist.");
    }
    const MemoryMapped::Vector<CellId>& cellSet = *(it->second);
    const CellId cellCount = CellId(cellSet.size());
    if(cellCount == 0) {
        throw runtime_error("Cell set " + cellSetName + " is empty.");
    }

    // Access the Lsh object that will do the computation.
    Lsh lsh(directoryName + "/Lsh-" + lshName);
    if(lsh.cellCount() != cellCount) {
        throw runtime_error("LSH object " + lshName +
            " has a number of cells inconsistent with cell set " + cellSetName + ".");
    }
    const size_t lshBitCount = lsh.lshCount();
    cout << "Number of LSH signature bits is " << lshBitCount << "." << endl;

    // Gather cells with the same signature.
    map<BitSetPointer, vector<CellId> > signatureMap;
    for(CellId cellId=0; cellId<cellCount; cellId++) {
        signatureMap[lsh.getSignature(cellId)].push_back(cellId);
    }
    cout << "Found " << signatureMap.size();
    cout << " populated signatures out of " << (1ULL<<lshBitCount);
    cout << " total possible signatures." << endl;

#if 0
    // Create a histogram containing how many signatures have a given number of cells.
    {
        cout << timestamp << "Creating signature histogram." << endl;
        vector<size_t> histogram;
        for(const auto& p: signatureMap) {
            const size_t size = p.second.size();
            if(size >= histogram.size()) {
                histogram.resize(size+1, 0);
            }
            ++(histogram[size]);
        }
        ofstream csvOut("SignatureHistogram.csv");
        csvOut << "Number of cells,Number of signatures,"
            "Total number of cells,Cumulative total number of cells\n";
        size_t sum = 0;
        for(size_t i=0; i<histogram.size(); i++) {
            const size_t frequency = histogram[i];
            if(frequency) {
                sum += frequency*i;
                csvOut << i << "," << frequency << "," << frequency*i  << "," << sum << "\n";
            }
        }
    }
#endif

    // Create the signature graph.
    const std::shared_ptr<SignatureGraph> signatureGraphPointer =
        std::make_shared<SignatureGraph>();
    signatureGraphs.insert(make_pair(signatureGraphName, signatureGraphPointer));
    SignatureGraph& signatureGraph = *signatureGraphPointer;

    // Create the vertices of the signature graph.
    cout << timestamp << "Creating vertices of the signature graph." << endl;
    typedef SignatureGraph::vertex_descriptor vertex_descriptor;
    for(const auto& p: signatureMap) {
        const BitSetPointer signature = p.first;
        const vector<CellId> cells = p.second;
        if(cells.size() < minCellCount) {
            continue;
        }
        const vertex_descriptor v = add_vertex(signatureGraph);
        signatureGraph.vertexMap.insert(make_pair(signature, v));
        SignatureGraphVertex& vertex = signatureGraph[v];
        vertex.signature = signature;
        vertex.cellCount = CellId(cells.size());
    }
    cout << "The signature graph has " << num_vertices(signatureGraph) << " vertices." << endl;

    // Create the edges of the signature graph.
    cout << timestamp << "Creating edges of the signature graph." << endl;
    signatureGraph.createEdges(lshBitCount);
    cout << "The signature graph has " << num_edges(signatureGraph) << " edges." << endl;
    cout << "Average connectivity is " <<
        (2.*double(num_edges(signatureGraph)))/double(num_vertices(signatureGraph)) << endl;

    // Write out the signature graph in Graphviz format.
    // signatureGraph.writeGraphviz("SignatureGraph.dot");

    // Write out the signature graph in svg format.
    // This gives us more flexibility than using svg to create svg output.
    SignatureGraph::SvgParameters svgParameters = signatureGraph.getDefaultSvgParameters();
    // svgParameters.hideEdges = true;
    signatureGraph.writeSvg("SignatureGraph.svg", svgParameters);

    cout << timestamp << "createSignatureGraph ends." << endl;
}