#include <TBranch.h>
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

std::vector<float> readVectorFloatBranchData(
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

void createTreeWithVectorFloatBranch(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname,
    const std::vector<std::vector<float>>& branchValues
)
{
    TFile file(filepath.c_str(), "RECREATE");
    if (file.IsZombie()) {
        throw std::runtime_error(std::format("Failed to create file: {}", filepath));
    }

    TTree tree(treename.c_str(), treename.c_str());

    std::vector<float> buffer;
    auto* branch = tree.Branch(branchname.c_str(), &buffer);
    if (!branch) {
        throw std::runtime_error(std::format(
            "Failed to create initial branch '{}' on new tree '{}'.", branchname, treename));
    }

    for (const auto& entryValues : branchValues) {
        buffer = entryValues;
        tree.Fill();
    }

    file.Write();
    file.Close();
}

void insertVectorFloatBranch(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname,
    const std::vector<std::vector<float>>& branchValues
)
{
    // Open the file in UPDATE mode so we can append the new branch alongside the existing entries.
    auto file = std::unique_ptr<TFile>(TFile::Open(filepath.c_str(), "UPDATE"));
    if (!file || file->IsZombie()) {
        throw std::runtime_error(std::format("Failed to open file for update: {}", filepath));
    }

    auto tree = file->Get<TTree>(treename.c_str());
    if (!tree) {
        throw std::runtime_error(
            std::format("Failed to retrieve TTree '{}' from file.", treename));
    }

    const Long64_t nEntries = tree->GetEntries();
    if (static_cast<Long64_t>(branchValues.size()) != nEntries) {
        throw std::runtime_error(std::format(
            "Branch value count ({}) does not match TTree entries ({}).",
            branchValues.size(), nEntries));
    }

    if (tree->GetBranch(branchname.c_str()) != nullptr) {
        throw std::runtime_error(std::format(
            "Branch '{}' already exists on tree '{}'.", branchname, treename));
    }

    std::vector<float> buffer;
    auto* branch = tree->Branch(branchname.c_str(), &buffer);
    if (!branch) {
        throw std::runtime_error(std::format(
            "Failed to create branch '{}' on tree '{}'.", branchname, treename));
    }

    for (Long64_t entry = 0; entry < nEntries; ++entry) {
        // LoadTree sets the internal entry number without touching other branch data.
        tree->LoadTree(entry);
        buffer = branchValues[entry];
        branch->Fill(); // Fill the branch for the current entry without calling TTree::Fill().
    }

    // Ensure the tree entry count stays aligned and write the updated tree back.
    tree->SetEntries(nEntries);
    tree->Write("", TObject::kOverwrite);
}