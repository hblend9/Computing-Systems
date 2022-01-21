#include <stdio.h>
#include <stdlib.h>

#include "compile.h"
#include "parser.h"

void usage(char *program) {
    fprintf(stderr, "USAGE: %s <program file>\n", program);
    exit(1);
}

/**
 * Prints the start of the the x86-64 assembly output.
 * The assembly code implementing the TeenyBASIC statements
 * goes between the header and the footer.
 */
void header(void) {
    printf(
        ".text\n"
        ".globl basic_main\n"
        "basic_main:\n"
        "   push %%rbp\n"         // store location of the old bp
        "   movq %%rsp, %%rbp\n"  // move bp to bottom
        "   subq $208, %%rsp\n"); // make space at the bottom for all the letters
}

/**
 * Prints the end of the x86-64 assembly output.
 * The assembly code implementing the TeenyBASIC statements
 * goes between the header and the footer.
 */
void footer(void) {
    printf(
        "   addq $208, %%rsp\n" // move stack pointer back up
        "   pop %%rbp\n"        // put rbp back to where it started
        "   ret\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
    }

    FILE *program = fopen(argv[1], "r");
    if (program == NULL) {
        usage(argv[0]);
    }

    header();

    node_t *ast = parse(program);
    fclose(program);
    if (ast == NULL) {
        fprintf(stderr, "Parse error\n");
        return 2;
    }

    // Display the AST for debugging purposes
    print_ast(ast);

    // Compile the AST into assembly instructions
    if (!compile_ast(ast)) {
        free_ast(ast);
        fprintf(stderr, "Compilation error\n");
        return 3;
    }

    free_ast(ast);

    footer();
}
