/// @file root-utils.hpp
/// @brief Thin helpers for reading and writing `vector<float>` branches in ROOT TTrees.
///
/// The benchmark only ever sees flat `std::vector<float>` data, but the
/// underlying TTrees store one `std::vector<float>` per entry. These
/// helpers translate between the two representations and let the driver
/// write decompressed data back into a parallel ROOT file for downstream
/// inspection.

#pragma once

#include <string>
#include <vector>

/// @brief Read a single `std::vector<float>` branch from a TTree and
/// flatten all entries into one contiguous vector.
///
/// Entry boundaries are discarded; use @ref readBranchEntrySizes to
/// recover them when the original structure needs to be reconstructed.
std::vector<float> readVectorFloatBranchData(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname);

/// @brief Append a `std::vector<float>` branch to an existing TTree.
///
/// The tree's entry count is preserved; @p branchValues.size() must equal
/// the tree's existing number of entries. Each inner vector corresponds
/// to one entry's values.
void insertVectorFloatBranch(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname,
    const std::vector<std::vector<float>>& branchValues);

/// @brief Read the per-entry length of a `std::vector<float>` branch.
///
/// @return A vector whose `i`-th element is the number of floats in entry `i`.
/// Combined with @ref readVectorFloatBranchData, this is enough to
/// reconstruct the original nested structure after decompression.
std::vector<std::size_t> readBranchEntrySizes(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname);

/// @brief Create a new ROOT file containing a tree with a single
/// `std::vector<float>` branch.
///
/// This establishes the tree's entry count so subsequent
/// @ref insertVectorFloatBranch calls can append matching branches.
void createTreeWithVectorFloatBranch(
    const std::string& filepath,
    const std::string& treename,
    const std::string& branchname,
    const std::vector<std::vector<float>>& branchValues);
