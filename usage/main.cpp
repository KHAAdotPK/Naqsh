/*
    src/main.cpp
    Q@hackers.pk
 */

#include "main.hh"

int main(int argc, char* argv[]) {
    
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <INPUT filename> <OUTPUT filename>" << std::endl;
        return 1;
    }

    std::string inputFilename = argv[1];
    std::string outputFilename = argv[2];

    std::ifstream inputFile(inputFilename);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file " << inputFilename << std::endl;
        return 1;
    }

    std::ofstream outputFile(outputFilename);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open file " << outputFilename << std::endl;
        return 1;
    }

    Parser parser;
    std::string line;
    while (std::getline(inputFile, line)) {
        outputFile << parser.cleanLine(line) << std::endl;
    }

    return 0;
}

