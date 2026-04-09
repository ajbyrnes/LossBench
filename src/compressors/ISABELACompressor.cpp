#include "ISABELACompressor.hpp"
#include <cstring>
#include <stdexcept>

#include <isabela.h>

// Small header prepended to compressed data so decompress knows the config.
// Layout: windowSize(4) | ncoefficients(4) | errorRate(4) | transform(1) = 13 bytes
static constexpr size_t kHeaderSize = 13;

static void writeHeader(std::vector<uint8_t>& buf, uint32_t ws, uint32_t nc, float er, uint8_t tf) {
    buf.resize(kHeaderSize);
    std::memcpy(buf.data() + 0, &ws, 4);
    std::memcpy(buf.data() + 4, &nc, 4);
    std::memcpy(buf.data() + 8, &er, 4);
    buf[12] = tf;
}

static void readHeader(const uint8_t* buf, uint32_t& ws, uint32_t& nc, float& er, uint8_t& tf) {
    std::memcpy(&ws, buf + 0, 4);
    std::memcpy(&nc, buf + 4, 4);
    std::memcpy(&er, buf + 8, 4);
    tf = buf[12];
}

CompressedData ISABELACompressor::compress(const std::vector<float>& data) {
    size_t inputBytes = data.size() * sizeof(float);

    // ISABELA stores per-window sort indices (same size as data), coefficients,
    // and compressed error residuals. Use 8x as ISABELA's own example does.
    std::vector<char> outBuf(inputBytes * 8);

    isabela_config config{};
    config.window_size = _windowSize;
    config.ncoefficients = _ncoefficients;
    config.error_rate = _errorRate;
    config.element_byte_size = sizeof(float);
    config.transform = static_cast<uchar>(_transform);

    isabela_stream strm{};
    ISABELA_status status = isabelaDeflateInit(&strm, sizeof(float), &config);
    if (status != ISABELA_SUCCESS) {
        throw std::runtime_error("ISABELA DeflateInit failed");
    }

    strm.next_in = const_cast<void*>(static_cast<const void*>(data.data()));
    strm.avail_in = static_cast<uint32_t>(inputBytes);
    strm.next_out = outBuf.data();

    status = isabelaDeflate(&strm, ISABELA_FINISH);
    if (status != ISABELA_SUCCESS) {
        isabelaDeflateEnd(&strm);
        throw std::runtime_error("ISABELA Deflate failed");
    }

    size_t compressedSize = strm.avail_out;
    isabelaDeflateEnd(&strm);

    // Build output: header + compressed payload
    std::vector<uint8_t> result;
    writeHeader(result, _windowSize, _ncoefficients, _errorRate,
                static_cast<uint8_t>(_transform));
    result.insert(result.end(),
                  reinterpret_cast<uint8_t*>(outBuf.data()),
                  reinterpret_cast<uint8_t*>(outBuf.data()) + compressedSize);

    return {
        .data = std::move(result),
        .numFloats = data.size()
    };
}

std::vector<float> ISABELACompressor::decompress(const CompressedData& compressedData) {
    if (compressedData.data.size() < kHeaderSize) {
        throw std::runtime_error("ISABELA compressed data too small for header");
    }

    uint32_t ws, nc;
    float er;
    uint8_t tf;
    readHeader(compressedData.data.data(), ws, nc, er, tf);

    const uint8_t* payload = compressedData.data.data() + kHeaderSize;
    size_t payloadSize = compressedData.data.size() - kHeaderSize;

    // ISABELA's example app allocates 8x for decompression output.
    // Allocate extra padding to prevent heap corruption from internal writes.
    size_t outputFloats = compressedData.numFloats;
    std::vector<float> output(outputFloats * 2);

    isabela_config config{};
    config.window_size = ws;
    config.ncoefficients = nc;
    config.error_rate = er;
    config.element_byte_size = sizeof(float);
    config.transform = static_cast<uchar>(tf);

    isabela_stream strm{};
    ISABELA_status status = isabelaInflateInit(&strm, sizeof(float), &config);
    if (status != ISABELA_SUCCESS) {
        throw std::runtime_error("ISABELA InflateInit failed");
    }

    strm.next_in = const_cast<void*>(static_cast<const void*>(payload));
    strm.avail_in = static_cast<uint32_t>(payloadSize);
    strm.next_out = output.data();

    status = isabelaInflate(&strm, ISABELA_FINISH);
    if (status != ISABELA_SUCCESS) {
        isabelaInflateEnd(&strm);
        throw std::runtime_error("ISABELA Inflate failed");
    }

    isabelaInflateEnd(&strm);

    output.resize(outputFloats);
    return output;
}

void ISABELACompressor::configure(const std::map<std::string, std::string>& options) {
    // Reset to defaults
    _windowSize = 1024;
    _ncoefficients = 30;
    _errorRate = 0.01f;
    _transform = 0;

    for (const auto& [key, value] : options) {
        if (key == "windowSize") {
            _windowSize = static_cast<uint32_t>(std::stoul(value));
            if (_windowSize == 0) {
                throw std::invalid_argument("windowSize must be positive");
            }
        } else if (key == "ncoefficients") {
            _ncoefficients = static_cast<uint32_t>(std::stoul(value));
            if (_ncoefficients == 0) {
                throw std::invalid_argument("ncoefficients must be positive");
            }
        } else if (key == "errorRate") {
            _errorRate = std::stof(value);
            if (_errorRate < 0) {
                throw std::invalid_argument("errorRate must be non-negative");
            }
        } else if (key == "transform") {
            if (value == "bsplines" || value == "bspline") {
                _transform = ISABELA_BSPLINES;
            } else if (value == "wavelets" || value == "wavelet") {
                _transform = ISABELA_WAVELETS;
            } else {
                int t = std::stoi(value);
                if (t < 0 || t > 1) {
                    throw std::invalid_argument("transform must be bsplines/0 or wavelets/1");
                }
                _transform = t;
            }
        } else {
            throw std::invalid_argument("Unknown ISABELA option: " + key);
        }
    }
}

std::map<std::string, std::string> ISABELACompressor::getConfig() const {
    return {
        {"windowSize", std::to_string(_windowSize)},
        {"ncoefficients", std::to_string(_ncoefficients)},
        {"errorRate", std::to_string(_errorRate)},
        {"transform", _transform == ISABELA_WAVELETS ? "wavelets" : "bsplines"}
    };
}

std::string ISABELACompressor::name() const {
    return "ISABELA";
}

std::string ISABELACompressor::description() const {
    return "Sort-based lossy compression using ISABELA";
}

std::string ISABELACompressor::version() const {
    return "0.2.1";
}

std::string ISABELACompressor::usage() const {
    return "ISABELA compressor options:\n"
           "  - windowSize:    uint, Elements per window (default: 1024)\n"
           "  - ncoefficients: uint, Coefficients per window (default: 30)\n"
           "  - errorRate:     float, Max relative error, 0 = no error encoding (default: 0.01)\n"
           "  - transform:     string, bsplines or wavelets (default: bsplines)\n"
           "\n"
           "Examples:\n"
           "  isabela\n"
           "  isabela:errorRate=0.05\n"
           "  isabela:transform=wavelets,ncoefficients=20\n"
           "  isabela:windowSize=512,errorRate=0.001";
}
