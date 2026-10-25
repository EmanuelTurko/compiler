#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>
#include <vector>

#include "tokenization.hpp"
#include "parser.hpp"
#include "generation.hpp"



int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string contents{};
    {
        std::stringstream contentsStream{};
        std::fstream input(argv[1],std::ios::in);
        contentsStream << input.rdbuf();
        contents = contentsStream.str();
    }
    Tokenizer tokenizer(std::move(contents));
    auto tokens = tokenizer.tokenize();

    Parser parser(std::move(tokens));
    std::optional<node::Program> program = parser.parseProgram();

    if (!program.has_value()) {
        std::cerr << "Invalid program" << std::endl;
        exit(EXIT_FAILURE);
    }

    Generator generator(program.value());
    {
        std::fstream file("out.asm", std::ios::out);
        file << generator.generateProgram();
    }

    system("nasm -felf64 out.asm");
    system("ld -o out out.o");
    return EXIT_SUCCESS;
}