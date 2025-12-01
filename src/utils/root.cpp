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

std::vector<float> readBranchData(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname
)
{
    // Open file
    auto file = std::unique_ptr<TFile>(TFile::Open(filepath.c_str(), "READ"));
    if (!file || file->IsZombie()) {
        throw std::runtime_error(std::format("Failed to open file: {}", filepath));
    }

    // Open TTree
    auto tree = file->Get<TTree>(treename.c_str());
    if (!tree) {
        throw std::runtime_error(
            std::format("Failed to retrieve TTree '{}' from file.", treename));
    }

    // Only enable the branch we need to avoid touching other types/dictionaries.
    tree->SetBranchStatus("*", 0);
    tree->SetBranchStatus(branchname.c_str(), 1);

    TTreeReader reader(tree);
    TTreeReaderValue<std::vector<float>> branch(reader, branchname.c_str());

    // Force setup and check branch status
    reader.SetEntry(0);
    if (branch.GetSetupStatus() < 0) {
        throw std::runtime_error(std::format(
            "Failed to set up branch '{}' from TTree '{}' (missing or wrong type).",
            branchname, treename)); 
    }

    std::vector<float> values;

    // Loop over all entries in the tree
    // Load branch data into flattened vector
    while (reader.Next()) {
        const auto& entryValues = *branch;
        values.insert(values.end(), entryValues.begin(), entryValues.end());
    }

    return values;
}