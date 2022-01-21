#include "compile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int64_t IF_COUNTER = 0;
int64_t WHILE_COUNTER = 0;

int64_t power_helper(num_node_t *node) {
    if (node->value > 0) {
        if (ceil(log2(node->value)) == floor(log2(node->value))) {
            return log2(node->value);
        }
    }
    return -1;
}

bool constant_check(node_t *node) {
    if (node->type == NUM) {
        return true; // we know that num is a constant
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *b_node = (binary_node_t *) node;
        bool left = constant_check(
            b_node->left); // only return true if the ends of the tree have constants
        bool right = constant_check(b_node->right);
        return left && right;
    }
    return false;
}

int64_t constant_calculator(node_t *node) {
    if (node->type == NUM) {
        num_node_t *n_node = (num_node_t *) node;
        return n_node->value;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *b_node = (binary_node_t *) node;
        if (b_node->op == '+') {
            return constant_calculator(b_node->left) + constant_calculator(b_node->right);
        }
        else if (b_node->op == '-') {
            return constant_calculator(b_node->left) - constant_calculator(b_node->right);
        }
        else if (b_node->op == '*') {
            return constant_calculator(b_node->left) * constant_calculator(b_node->right);
        }
        else if (b_node->op == '/') {
            return constant_calculator(b_node->left) / constant_calculator(b_node->right);
        }
    }
    return 0;
}

bool compile_ast(node_t *node) {
    if (node->type == NUM) {
        printf("movq $%ld, %%rdi\n", ((num_node_t *) node)->value);
        return true;
    }
    else if (node->type == PRINT) {
        compile_ast(((print_node_t *) node)->expr);
        printf("callq print_int\n");
        return true;
    }
    else if (node->type == SEQUENCE) {
        sequence_node_t *sequence = (sequence_node_t *) node;
        for (size_t i = 0; i < sequence->statement_count; i++) {
            compile_ast(sequence->statements[i]);
        }
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *bin = (binary_node_t *) node;
        if (constant_check(node) && bin->op != ('=' & '<' & '>')) {
            printf("movq $%ld, %%rdi\n", constant_calculator(node));
            return true;
        }
        compile_ast(bin->left);
        if (bin->right->type == NUM && bin->op == '*') {
            int64_t num_pow = power_helper((num_node_t *) bin->right);
            if (num_pow != -1) {
                printf("shl $%ld, %%rdi\n", num_pow); // shifting
                return true;
            }
        }
        printf("push %%rdi\n");
        compile_ast(bin->right);
        printf("pop %%r8\n");
        if (bin->op == '+') {
            printf("addq %%r8, %%rdi\n");
        }
        else if (bin->op == '-') {
            printf("subq %%rdi, %%r8\n");
            printf("movq %%r8, %%rdi\n");
        }
        else if (bin->op == '*') {
            printf("imulq %%r8, %%rdi\n");
        }
        else if (bin->op == '/') {
            printf("movq %%r8, %%rax\n");
            printf("cqto\n");
            printf("idiv %%rdi\n");
            printf("movq %%rax, %%rdi\n");
        }
        else {
            printf("cmp %%rdi, %%r8\n");
        }
        return true;
    }
    else if (node->type == VAR) {
        var_node_t *variable = ((var_node_t *) node);
        int64_t offset = variable->name - 'A' + 1; // distance from A
        offset *= -8;
        printf("movq %ld(%%rbp), %%rdi\n", offset);
        return true;
    }
    else if (node->type == LET) {
        let_node_t *let = (let_node_t *) node;
        int64_t offset = let->var - 'A' + 1;
        offset *= -8;
        compile_ast(let->value);
        printf("movq %%rdi, %ld(%%rbp)\n", offset);
        return true;
    }
    else if (node->type == IF) {
        if_node_t *conditional = (if_node_t *) node;
        char binary_op = conditional->condition->op;
        int64_t if_counter =
            IF_COUNTER++; // so we don't update the global variable too often
        compile_ast((node_t *) conditional->condition);
        if (binary_op == '=') {
            printf("je true_block_%ld\n", if_counter);
        }
        else if (binary_op == '<') {
            printf("jl true_block_%ld\n", if_counter);
        }
        else if (binary_op == '>') {
            printf("jg true_block_%ld\n", if_counter);
        }
        if (conditional->else_branch != NULL) {
            compile_ast(conditional->else_branch);
        }
        printf("jmp end_if_%ld\n", if_counter);
        printf("true_block_%ld:\n", if_counter);
        compile_ast(conditional->if_branch);
        printf("end_if_%ld:\n", if_counter);
        return true;
    }
    else if (node->type == WHILE) {
        while_node_t *loop = (while_node_t *) node;
        char binary_op = loop->condition->op;
        int64_t while_counter = WHILE_COUNTER++;
        compile_ast((node_t *) loop->condition);
        if (binary_op == '=') {
            printf("jne end_while_%ld\n", while_counter); // only do loop while equal
        }
        else if (binary_op == '<') {
            printf("jge end_while_%ld\n", while_counter);
        }
        else if (binary_op == '>') {
            printf("jle end_while_%ld\n", while_counter);
        }
        printf("block_%ld:\n", while_counter);
        compile_ast(loop->body);
        compile_ast((node_t *) loop->condition);
        if (binary_op == '=') {
            printf("je block_%ld\n", while_counter);
        }
        else if (binary_op == '<') {
            printf("jl block_%ld\n", while_counter);
        }
        else if (binary_op == '>') {
            printf("jg block_%ld\n", while_counter);
        }
        printf("end_while_%ld:\n", while_counter);
        return true;
    }
    return false;
}
