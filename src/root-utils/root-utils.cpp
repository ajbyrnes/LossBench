/// @file root-utils.cpp
/// @brief ROOT TTree I/O helpers for `vector<float>` branches.

#include <TBranch.h>
#include <TFile.h>
#include <TLeaf.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Determine if a branch is a std::vector<float> (returns true) or a C-style array (returns false).
// C-style arrays have titles like "branchname[size]/F" while vectors have class names.
bool isVectorFloatBranch(TTree* tree, const std::string& branchname) {
    auto* branch = tree->GetBranch(branchname.c_str());
    if (!branch) return false;

    // If the branch has a class name containing "vector", it's an STL vector
    std::string className = branch->GetClassName();
    if (className.find("vector") != std::string::npos) {
        return true;
    }

    // Otherwise it's likely a C-style array (title format: "name[size]/type")
    return false;
}

} // anonymous namespace

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
            std::format("Failed to retrieve TTree '{}' from file.", treename)
        );
    }

    // Check branch type before setting up reader
    bool isVector = isVectorFloatBranch(tree, branchname);

    // For C-style arrays, we also need to enable the size branch if it exists
    // The title format is "branchname[sizebranch]/F"
    if (!isVector) {
        auto* branch = tree->GetBranch(branchname.c_str());
        if (branch) {
            std::string title = branch->GetTitle();
            // Extract size branch name from title like "lep_pt[lep_n]/F"
            auto openBracket = title.find('[');
            auto closeBracket = title.find(']');
            if (openBracket != std::string::npos && closeBracket != std::string::npos) {
                std::string sizeBranch = title.substr(openBracket + 1, closeBracket - openBracket - 1);
                tree->SetBranchStatus("*", 0);
                tree->SetBranchStatus(branchname.c_str(), 1);
                tree->SetBranchStatus(sizeBranch.c_str(), 1);
            }
        }
    } else {
        // Only enable the branch we need to avoid touching other types/dictionaries.
        tree->SetBranchStatus("*", 0);
        tree->SetBranchStatus(branchname.c_str(), 1);
    }

    std::vector<float> values;

    if (isVector) {
        // Handle std::vector<float> branches
        TTreeReader reader(tree);
        TTreeReaderValue<std::vector<float>> branch(reader, branchname.c_str());

        // Force setup and check branch status
        reader.SetEntry(0);
        if (branch.GetSetupStatus() < 0) {
            std::string actualType = "unknown";
            if (auto* tbranch = tree->GetBranch(branchname.c_str())) {
                actualType = tbranch->GetClassName();
                if (actualType.empty()) {
                    actualType = tbranch->GetTitle();
                }
            }
            throw std::runtime_error(std::format(
                "Failed to set up branch '{}' from TTree '{}': expected vector<float>, found '{}'.",
                branchname, treename, actualType)
            );
        }

        // Loop over all entries and flatten into output vector
        while (reader.Next()) {
            const auto& entryValues = *branch;
            values.insert(values.end(), entryValues.begin(), entryValues.end());
        }
    } else {
        // Handle C-style variable-length array branches (e.g., "lep_pt[lep_n]/F")
        TTreeReader reader(tree);
        TTreeReaderArray<float> branch(reader, branchname.c_str());

        // Force setup and check branch status
        reader.SetEntry(0);
        if (branch.GetSetupStatus() < 0) {
            std::string actualType = "unknown";
            if (auto* tbranch = tree->GetBranch(branchname.c_str())) {
                actualType = tbranch->GetClassName();
                if (actualType.empty()) {
                    actualType = tbranch->GetTitle();
                }
            }
            throw std::runtime_error(std::format(
                "Failed to set up branch '{}' from TTree '{}': found '{}'.",
                branchname, treename, actualType)
            );
        }

        // Loop over all entries and flatten into output vector
        while (reader.Next()) {
            for (std::size_t i = 0; i < branch.GetSize(); ++i) {
                values.push_back(branch[i]);
            }
        }
    }

    return values;
}

std::vector<std::size_t> readBranchEntrySizes(
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
            std::format("Failed to retrieve TTree '{}' from file.", treename)
        );
    }

    // Check branch type before setting up reader
    bool isVector = isVectorFloatBranch(tree, branchname);

    // For C-style arrays, we also need to enable the size branch if it exists
    if (!isVector) {
        auto* branch = tree->GetBranch(branchname.c_str());
        if (branch) {
            std::string title = branch->GetTitle();
            auto openBracket = title.find('[');
            auto closeBracket = title.find(']');
            if (openBracket != std::string::npos && closeBracket != std::string::npos) {
                std::string sizeBranch = title.substr(openBracket + 1, closeBracket - openBracket - 1);
                tree->SetBranchStatus("*", 0);
                tree->SetBranchStatus(branchname.c_str(), 1);
                tree->SetBranchStatus(sizeBranch.c_str(), 1);
            }
        }
    } else {
        tree->SetBranchStatus("*", 0);
        tree->SetBranchStatus(branchname.c_str(), 1);
    }

    std::vector<std::size_t> sizes;

    if (isVector) {
        // Handle std::vector<float> branches
        TTreeReader reader(tree);
        TTreeReaderValue<std::vector<float>> branch(reader, branchname.c_str());

        // Force setup and check branch status
        reader.SetEntry(0);
        if (branch.GetSetupStatus() < 0) {
            std::string actualType = "unknown";
            if (auto* tbranch = tree->GetBranch(branchname.c_str())) {
                actualType = tbranch->GetClassName();
                if (actualType.empty()) {
                    actualType = tbranch->GetTitle();
                }
            }
            throw std::runtime_error(std::format(
                "Failed to set up branch '{}' from TTree '{}': expected vector<float>, found '{}'.",
                branchname, treename, actualType)
            );
        }

        // Loop over all entries and record sizes
        while (reader.Next()) {
            sizes.push_back(branch->size());
        }
    } else {
        // Handle C-style variable-length array branches
        TTreeReader reader(tree);
        TTreeReaderArray<float> branch(reader, branchname.c_str());

        // Force setup and check branch status
        reader.SetEntry(0);
        if (branch.GetSetupStatus() < 0) {
            std::string actualType = "unknown";
            if (auto* tbranch = tree->GetBranch(branchname.c_str())) {
                actualType = tbranch->GetClassName();
                if (actualType.empty()) {
                    actualType = tbranch->GetTitle();
                }
            }
            throw std::runtime_error(std::format(
                "Failed to set up branch '{}' from TTree '{}': found '{}'.",
                branchname, treename, actualType)
            );
        }

        // Loop over all entries and record sizes
        while (reader.Next()) {
            sizes.push_back(branch.GetSize());
        }
    }

    return sizes;
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

    if (file.Write() == 0) {
        throw std::runtime_error(std::format("Failed to write to file: {}", filepath));
    }
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
    if (tree->Write("", TObject::kOverwrite) == 0) {
        throw std::runtime_error(std::format("Failed to write tree '{}' to file: {}", treename, filepath));
    }
}