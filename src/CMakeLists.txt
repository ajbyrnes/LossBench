# Minimum version required by ROOT as of November 28, 2025
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Declare the project name and version
project(LossBench VERSION 1.0.0 LANGUAGES CXX)

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)