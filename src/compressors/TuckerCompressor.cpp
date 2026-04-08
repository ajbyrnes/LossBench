#include "TuckerCompressor.hpp"
#include <Tucker.hpp>
#include <cmath>
#include <cstring>
#include <stdexcept>

// Choose 2D shape for N elements: roughly square, rows*cols >= N
static void choose2DShape(int N, int& rows, int& cols) {
    cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(N))));
    rows = (N + cols - 1) / cols;
}

CompressedData TuckerCompressor::compress(const std::vector<float>& data) {
    int N = static_cast<int>(data.size());

    // Reshape flat data into a 2D tensor (rows x cols), zero-padding the tail
    int rows, cols;
    choose2DShape(N, rows, cols);
    int padded = rows * cols;

    Tucker::SizeArray dims(2);
    dims[0] = rows;
    dims[1] = cols;
    Tucker::Tensor<double> X(dims);
    X.initialize();  // zero-fill
    for (int i = 0; i < N; ++i) {
        X.data()[i] = static_cast<double>(data[i]);
    }

    // Compute ST-HOSVD with automatic rank determination
    const Tucker::TuckerTensor<double>* fact =
        Tucker::STHOSVD(&X, _epsilon);

    // Extract core tensor G (rank0 x rank1) and factor matrices U[0], U[1]
    int rank0 = fact->G->size(0);
    int rank1 = fact->G->size(1);
    int gElems = rank0 * rank1;

    int u0rows = fact->U[0]->nrows();  // == rows
    int u0cols = fact->U[0]->ncols();  // == rank0
    int u1rows = fact->U[1]->nrows();  // == cols
    int u1cols = fact->U[1]->ncols();  // == rank1

    // Serialize:
    //   [N:int32] [rows:int32] [cols:int32]
    //   [rank0:int32] [rank1:int32]
    //   [G as float[gElems]]
    //   [U0 as float[u0rows*u0cols]]
    //   [U1 as float[u1rows*u1cols]]
    size_t headerBytes = 5 * sizeof(int32_t);
    size_t gBytes = gElems * sizeof(float);
    size_t u0Bytes = static_cast<size_t>(u0rows) * u0cols * sizeof(float);
    size_t u1Bytes = static_cast<size_t>(u1rows) * u1cols * sizeof(float);
    size_t totalSize = headerBytes + gBytes + u0Bytes + u1Bytes;

    std::vector<uint8_t> compressed(totalSize);
    uint8_t* p = compressed.data();

    auto writeInt = [&](int32_t v) { std::memcpy(p, &v, 4); p += 4; };
    writeInt(N);
    writeInt(rows);
    writeInt(cols);
    writeInt(rank0);
    writeInt(rank1);

    auto writeDoubleAsFloat = [&](const double* src, int count) {
        for (int i = 0; i < count; ++i) {
            float v = static_cast<float>(src[i]);
            std::memcpy(p, &v, sizeof(float)); p += sizeof(float);
        }
    };

    writeDoubleAsFloat(fact->G->data(), gElems);
    writeDoubleAsFloat(fact->U[0]->data(), u0rows * u0cols);
    writeDoubleAsFloat(fact->U[1]->data(), u1rows * u1cols);

    delete fact;

    return {
        .data = std::move(compressed),
        .numFloats = data.size()
    };
}

std::vector<float> TuckerCompressor::decompress(const CompressedData& compressedData) {
    const uint8_t* p = compressedData.data.data();

    auto readInt = [&]() -> int32_t {
        int32_t v; std::memcpy(&v, p, 4); p += 4; return v;
    };

    int32_t N     = readInt();
    int32_t rows  = readInt();
    int32_t cols  = readInt();
    int32_t rank0 = readInt();
    int32_t rank1 = readInt();

    int gElems = rank0 * rank1;

    auto readFloatAsDouble = [&](double* dst, int count) {
        for (int i = 0; i < count; ++i) {
            float v; std::memcpy(&v, p, sizeof(float)); p += sizeof(float);
            dst[i] = static_cast<double>(v);
        }
    };

    // Read core tensor G (rank0 x rank1)
    Tucker::SizeArray gDims(2);
    gDims[0] = rank0;
    gDims[1] = rank1;
    Tucker::Tensor<double> G(gDims);
    readFloatAsDouble(G.data(), gElems);

    // Read U0 (rows x rank0)
    Tucker::Matrix<double> U0(rows, rank0);
    readFloatAsDouble(U0.data(), rows * rank0);

    // Read U1 (cols x rank1)
    Tucker::Matrix<double> U1(cols, rank1);
    readFloatAsDouble(U1.data(), cols * rank1);

    // Reconstruct: X = G x_0 U0 x_1 U1
    Tucker::Tensor<double>* temp = Tucker::ttm(&G, 0, &U0);
    Tucker::Tensor<double>* result = Tucker::ttm(temp, 1, &U1);
    delete temp;

    // Extract the first N elements (discard zero-padding)
    std::vector<float> output(N);
    for (int i = 0; i < N; ++i) {
        output[i] = static_cast<float>(result->data()[i]);
    }

    delete result;
    return output;
}

void TuckerCompressor::configure(const std::map<std::string, std::string>& options) {
    for (const auto& [key, value] : options) {
        if (key == "epsilon") {
            _epsilon = std::stod(value);
            if (_epsilon <= 0) {
                throw std::invalid_argument(
                    "Invalid epsilon value: " + value + ". Must be positive.");
            }
        } else {
            throw std::invalid_argument("Unknown Tucker option: " + key);
        }
    }
}

std::map<std::string, std::string> TuckerCompressor::getConfig() const {
    return {{"epsilon", std::to_string(_epsilon)}};
}

std::string TuckerCompressor::name() const {
    return "Tucker";
}

std::string TuckerCompressor::description() const {
    return "Tucker decomposition (ST-HOSVD) via TuckerMPI";
}

std::string TuckerCompressor::version() const {
    return "0.7.0";
}

std::string TuckerCompressor::usage() const {
    return "Tucker compressor options:\n"
           "  - epsilon: double, SV threshold for automatic rank determination "
           "(default 0.1, must be positive)\n"
           "\n"
           "Examples:\n"
           "  tucker:epsilon=0.1\n"
           "  tucker:epsilon=0.01";
}
