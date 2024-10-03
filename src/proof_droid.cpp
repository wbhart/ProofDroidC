#include "grammar.h"
#include "node.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    manager_t mgr;
    std::string input;

    {
        parser_context_t *ctx = parser_create(&mgr);
        node* ast = nullptr;

        std::cout << "> ";

        while (getline(std::cin, input)) {
            input.push_back('\n');  // Add a newline at the end of input

            // Set the input buffer and reset position
            mgr.input = input.c_str();
            mgr.pos = 0;

            parser_parse(ctx, &ast);

            if (ast) {
                // Use the enum to select the string format for printing
                ast->print(OutputFormat::UNICODE);
                delete ast;  // Manually free the AST
                ast = nullptr;
            } else {
                std::cout << "Syntax error" << std::endl;
            }

            std::cout << "> ";
        }

        std::cout << std::endl;

        parser_destroy(ctx);
    }

    return 0;
}
