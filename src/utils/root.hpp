#pragma once

#include <string>
#include <vector>

// Read a single std::vector<float> branch from a TTree and return all values
// flattened into a single vector.
std::vector<float> readBranchData(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname);

