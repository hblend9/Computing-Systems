#include "cache_timing.h"

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

const size_t REPEATS = 100000;

int main() {
    uint64_t sum_miss = 0;
    uint64_t sum_hit = 0;

    for (size_t i = 0; i < REPEATS; i++) {
        page_t *page = malloc(sizeof(page_t));
        flush_cache_line(page);            // ensure that the page is not in cache
        uint64_t first = time_read(page);  // miss
        uint64_t second = time_read(page); // hit
        if (second > first) {              // check it hit > miss
            i--;
            continue;
        }
        sum_miss += first;
        sum_hit += second;
    }

    printf("average miss = %" PRIu64 "\n", sum_miss / REPEATS);
    printf("average hit  = %" PRIu64 "\n", sum_hit / REPEATS);
}
