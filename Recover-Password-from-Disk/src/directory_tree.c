#include "directory_tree.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

const mode_t MODE = 0777;

void init_node(node_t *node, char *name, node_type_t type) {
    if (name == NULL) {
        name = strdup("ROOT");
        assert(name != NULL);
    }
    node->name = name;
    node->type = type;
}

file_node_t *init_file_node(char *name, size_t size, uint8_t *contents) {
    file_node_t *node = malloc(sizeof(file_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, FILE_TYPE);
    node->size = size;
    node->contents = contents;
    return node;
}

directory_node_t *init_directory_node(char *name) {
    directory_node_t *node = malloc(sizeof(directory_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, DIRECTORY_TYPE);
    node->num_children = 0;
    node->children = NULL;
    return node;
}

void add_child_directory_tree(directory_node_t *dnode, node_t *child) {
    size_t num = dnode->num_children;
    dnode->children = (node_t **) realloc(dnode->children, sizeof(node_t *) * (num + 1));
    dnode->children[num] = child;
    dnode->num_children += 1;

    // Alphabetical order; need to find which index to add the child
    for (size_t i = num; i > 0; i--) {
        // start from the back because there is an empty space at the end
        if (strcmp(child->name, dnode->children[i - 1]->name) <
            0) { // str1 less than str2
            dnode->children[i] = dnode->children[i - 1];
            dnode->children[i - 1] = child;
        }
        else {
            break;
        }
    }
}

void print_tree_helper(node_t *node, size_t num) {
    printf("%*s", 4 * (int) num,
           ""
           ""); // 4n spaces
    printf("%s", node->name);
    printf("\n");

    // alphabetical order from add_child_directory_tree
    if (node->type == DIRECTORY_TYPE) { // check if the node is directory or file type
        directory_node_t *dnode = (directory_node_t *) node; // use a directory node now
        for (size_t i = 0; i < dnode->num_children; i++) {   // recursion
            print_tree_helper(dnode->children[i],
                              num + 1); // looking into more children, so need more spaces
        }
    }
}

void print_directory_tree(node_t *node) {
    size_t num = 0;
    print_tree_helper(node, num);
}

void create_tree_helper(node_t *node, char *name) {
    char *path =
        malloc(sizeof(char) * (strlen(name) + strlen(node->name) + 2)); // new path name
    strcpy(path, name);
    strcat(path, "/");
    strcat(path, node->name);

    if (node->type == FILE_TYPE) {
        file_node_t *fnode = (file_node_t *) node;
        FILE *file = fopen(path, "w"); // want to open for writing
        size_t written = fwrite(fnode->contents, sizeof(uint8_t), fnode->size, file);
        assert(written == fnode->size);
        assert(fclose(file) == 0); // the file should be closed successfully
    }
    else {
        assert(node->type == DIRECTORY_TYPE);
        directory_node_t *dnode = (directory_node_t *) node;
        assert(mkdir(path, MODE) == 0);
        for (size_t i = 0; i < dnode->num_children; i++) {
            create_tree_helper(dnode->children[i], path);
        }
    }
    free(path);
}

void create_directory_tree(node_t *node) {
    create_tree_helper(node, ".");
}

void free_directory_tree(node_t *node) {
    if (node->type == FILE_TYPE) {
        file_node_t *fnode = (file_node_t *) node;
        free(fnode->contents);
    }
    else {
        assert(node->type == DIRECTORY_TYPE);
        directory_node_t *dnode = (directory_node_t *) node;
        for (size_t i = 0; i < dnode->num_children; i++) {
            free_directory_tree(dnode->children[i]);
        }
        free(dnode->children);
    }
    free(node->name);
    free(node);
}
