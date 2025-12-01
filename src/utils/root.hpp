#pragma once

#include <string>
#include <vector>

// Read a single std::vector<float> branch from a TTree and return all values
// flattened into a single vector.
std::vector<float> readVectorFloatBranchData(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname);

// Write a std::vector<float> branch to an existing TTree without changing the
// entry count. Each inner vector corresponds to one entry; the size must match
// the tree's existing number of entries.
void insertVectorFloatBranch(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname,
    const std::vector<std::vector<float>>& branchValues);

// Create a new ROOT file with a tree that already contains one vector<float>
// branch. This sets the initial entry count so additional branches can be
// appended later via insertVectorFloatBranch.
void createTreeWithVectorFloatBranch(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname,
    const std::vector<std::vector<float>>& branchValues);


