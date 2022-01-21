#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "directory_tree.h"
#include "fat16.h"

const size_t MASTER_BOOT_RECORD_SIZE = 0x20B;

// must pass pre-condition --> while (true) loop
// read first directory entry into variable of size directory entry
// check if the entry's name does not start with null terminator (\0)
// if not, store where i am on the disk and then get the info from the first cluster from
// the directory entry the offset shows us where this is pointing to on the disk fseek to
// that offset to get us to that information check if my directory entry is hidden or not
// if file: make file, put info in it, close it
// if directory: make directory, follow using directory, add to the original node
// after computations, fseek back to the location that i stored to earlier

void follow(FILE *disk, directory_node_t *node, bios_parameter_block_t bpb) {
    while (true) {
        directory_entry_t entry;
        assert(fread(&entry, sizeof(directory_entry_t), 1, disk));
        if (entry.filename[0] ==
            '\0') { // check if the entry's name starts with a null terminator
            break;
        }
        long curr_pos = ftell(disk);            // storing where I am on the disk
        char *file_name = get_file_name(entry); // calls malloc; free this
        size_t offset = get_offset_from_cluster(entry.first_cluster,
                                                bpb); // getting the current offset
        if (is_hidden(entry)) {
            free(file_name);
            continue;
        }
        fseek(disk, offset,
              SEEK_SET); // fseek to that offset to get us to that information
        if (is_directory(entry)) {
            directory_node_t *dnode = init_directory_node(file_name);
            add_child_directory_tree(node, (node_t *) dnode);
            follow(disk, dnode, bpb); // recursion to go to the next node again
            // no information in the directory itself
        }
        else { // case if it is a file, not a directory
            uint8_t *f =
                malloc(entry.file_size * sizeof(uint8_t)); // just malloc for file
            size_t read = fread(f, entry.file_size, 1, disk);
            assert(read == 1);
            file_node_t *fnode = init_file_node(file_name, entry.file_size, f);
            add_child_directory_tree(node, (node_t *) fnode);
        }
        fseek(disk, curr_pos, SEEK_SET);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <image filename>\n", argv[0]);
        return 1;
    }

    FILE *disk = fopen(argv[1], "r");
    if (disk == NULL) {
        fprintf(stderr, "No such image file: %s\n", argv[1]);
        return 1;
    }

    bios_parameter_block_t bpb;

    fseek(disk, MASTER_BOOT_RECORD_SIZE,
          SEEK_SET); // seeking from the start, use seek_set
    size_t read = fread(&bpb, sizeof(bios_parameter_block_t), 1, disk);
    assert(read == 1);
    fseek(disk, get_root_directory_location(bpb), SEEK_SET);

    directory_node_t *root = init_directory_node(NULL);
    follow(disk, root, bpb);
    print_directory_tree((node_t *) root);
    create_directory_tree((node_t *) root);
    free_directory_tree((node_t *) root);

    int result = fclose(disk);
    assert(result == 0);
}
