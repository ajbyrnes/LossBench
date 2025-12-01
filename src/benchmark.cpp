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


   // TFile input(inputFile.c_str(), "READ");
    // if (input.IsZombie()) {
    //     std::cerr << "Failed to open input file." << std::endl;
    //     return 1;
    // }

    // TTree* inputTree = nullptr;
    // input.GetObject(treeName.c_str(), inputTree);
    // if (!inputTree) {
    //     std::cerr << "Failed to retrieve tree." << std::endl;
    //     return 1;
    // }

    // // Enable only the branches we want to copy
    // inputTree->SetBranchStatus("*", 0);
    // for (const auto& branchName : branchNames) {
    //     if (inputTree->GetBranch(branchName.c_str())) {
    //         inputTree->SetBranchStatus(branchName.c_str(), 1);
    //     } else {
    //         std::cerr << "Warning: branch not found: " << branchName << '\n';
    //     }
    // }

    // // Create output file/tree from scratch to avoid touching other branch dictionaries
    // TFile output(outputFile.c_str(), "RECREATE");
    // if (output.IsZombie()) {
    //     std::cerr << "Failed to open output file." << std::endl;
    //     return 1;
    // }

    // TTree outputTree(treeName.c_str(), treeName.c_str());
    // outputTree.SetDirectory(&output);

    // // Set up readers and buffers for selected branches
    // TTreeReader reader(inputTree);
    // std::vector<std::unique_ptr<TTreeReaderValue<std::vector<float>>>> readers;
    // std::vector<std::vector<float>> buffers(branchNames.size());

    // readers.reserve(branchNames.size());
    // for (size_t i = 0; i < branchNames.size(); ++i) {
    //     readers.emplace_back(
    //         std::make_unique<TTreeReaderValue<std::vector<float>>>(reader, branchNames[i].c_str()));
    //     const auto setupStatus = readers.back()->GetSetupStatus();
    //     // if (setupStatus < TTreeReaderValueBase::kSetupMatch) {
    //     //     std::cerr << "Failed to set up branch '" << branchNames[i]
    //     //               << "' (missing or wrong type)." << std::endl;
    //     //     return 1;
    //     // }
    //     outputTree.Branch(branchNames[i].c_str(), &buffers[i]);
    // }

    // // Manual copy loop: read selected branches and fill output tree
    // while (reader.Next()) {
    //     for (size_t i = 0; i < readers.size(); ++i) {
    //         buffers[i] = *(*readers[i]);
    //     }
    //     outputTree.Fill();
    // }

    // output.cd();
    // outputTree.Write();
    // output.Close();
    // input.Close();

    // return 0;


int main() {
    const std::string inputFile{"jets.root"};
    const std::string treeName{"CollectionTree"};
    const std::string branchName{"AnalysisJetsAuxDyn.pt"};

    try {
        auto values = readBranchData(inputFile, treeName, branchName);
        std::cout << "Read " << values.size() << " values from branch '" << branchName << "'." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}