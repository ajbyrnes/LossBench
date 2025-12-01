#include <TFile.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/root.hpp"



int main() {
    const std::string outputFile{"test.root"};

    // Create a brand-new tree with an initial branch so that later inserts have
    // a defined entry count to match.
    std::vector<std::vector<float>> baseBranchData = {
        {1.f, 2.f, 3.f},
        {4.f, 5.f},
        {6.f}
    };
    createTreeWithVectorFloatBranch(outputFile, "TestTree", "TestBranch1", baseBranchData);

    // Example usage of insertVectorBranch to append another branch afterwards.
    std::vector<std::vector<float>> newBranchData = {
        {10.f, 20.f},
        {30.f, 40.f, 50.f},
        {60.f}
    };

    insertVectorFloatBranch(outputFile, "TestTree", "TestBranch2", newBranchData);
}
