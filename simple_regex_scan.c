#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_mmap.h>
#include <doca_regex.h>
#include <doca_error.h>

#include <common.h>
#include <utils.h>

int main(int argc, char **argv) {
    char *pci_addr = NULL;
    char *data_path = NULL;
    char *rules_path = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "p:d:r:")) != -1) {
        switch (opt) {
            case 'p':
                pci_addr = optarg;
                break;
            case 'd':
                data_path = optarg;
                break;
            case 'r':
                rules_path = optarg;
                break;
        }
    }

    char *data_buffer = NULL;
    char *rules_buffer = NULL;
    size_t data_len = 0;
    size_t rules_len = 0;
    
    read_file(data_path, &data_buffer, &data_len);
    read_file(rules_path, &rules_buffer, &rules_len);

    struct doca_dev *dev;
    open_doca_device_with_pci(pci_addr, NULL, &dev);

    struct doca_regex *doca_regex;
    doca_regex_create(&doca_regex);
    doca_ctx_dev_add(doca_regex_as_ctx(doca_regex), dev);
    doca_regex_set_workq_matches_memory_pool_size(doca_regex, 8);
    doca_regex_set_hardware_compiled_rules(doca_regex, rules_buffer, rules_len);

    struct doca_buf_inventory *buf_inv;
    doca_buf_inventory_create(NULL, 1, DOCA_BUF_EXTENSION_NONE, &buf_inv);
    doca_buf_inventory_start(buf_inv);

    struct doca_mmap *mmap;
    doca_mmap_create(NULL, &mmap);
    doca_mmap_dev_add(mmap, dev);
    doca_mmap_set_memrange(mmap, data_buffer, data_len);
    doca_mmap_start(mmap);

    doca_ctx_start(doca_regex_as_ctx(doca_regex));

    struct doca_workq *workq;
    doca_workq_create(1, &workq);
    doca_ctx_workq_add(doca_regex_as_ctx(doca_regex), workq);

    struct doca_buf *buf;
    doca_buf_inventory_buf_by_addr(buf_inv, mmap, data_buffer, data_len, &buf);
    
    void *mbuf_data;
    doca_buf_get_data(buf, &mbuf_data);
    doca_buf_set_data(buf, mbuf_data, data_len);

    struct doca_regex_search_result result = {0};
    struct doca_regex_job_search job_request = {0};
    job_request.base.type = DOCA_REGEX_JOB_SEARCH;
    job_request.base.ctx = doca_regex_as_ctx(doca_regex);
    job_request.rule_group_ids[0] = 1;
    job_request.buffer = buf;
    job_request.result = &result;

    doca_workq_submit(workq, (struct doca_job *)&job_request);

    struct doca_event event = {0};
    struct timespec ts = {0, 10000};
    
    while (doca_workq_progress_retrieve(workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE) == DOCA_ERROR_AGAIN) {
        nanosleep(&ts, &ts);
    }
    
    struct doca_regex_search_result *scan_res = (struct doca_regex_search_result *)event.result.ptr;
    struct doca_regex_match *match = scan_res->matches;
    
    while (match) {
        printf("Matched regex %d against data: %.*s\n", match->rule_id, match->length, data_buffer + match->match_start);
        match = match->next;
    }

    return 0;
}
