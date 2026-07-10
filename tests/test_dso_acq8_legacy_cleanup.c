#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path, long *out_size)
{
    FILE *fp;
    long size;
    char *buf;

    if ((path == NULL) || (out_size == NULL)) {
        return NULL;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)size + 1u);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }

    if (fread(buf, 1u, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }

    buf[size] = '\0';
    fclose(fp);
    *out_size = size;
    return buf;
}

static int assert_not_contains(const char *path, const char *text, const char *needle)
{
    if ((path == NULL) || (text == NULL) || (needle == NULL)) {
        return 1;
    }

    if (strstr(text, needle) != NULL) {
        fprintf(stderr, "legacy 8-bit symbol still present in %s: %s\n", path, needle);
        return 1;
    }

    return 0;
}

static int assert_file_not_contains(const char *path,
                                    const char *const *forbidden,
                                    size_t forbidden_count)
{
    long size = 0;
    char *text = read_text_file(path, &size);
    size_t i;

    (void)size;

    if (text == NULL) {
        fprintf(stderr, "failed to read %s\n", path);
        return 1;
    }

    for (i = 0u; i < forbidden_count; ++i) {
        if (assert_not_contains(path, text, forbidden[i]) != 0) {
            free(text);
            return 1;
        }
    }

    free(text);
    return 0;
}

int main(void)
{
    static const char *const acq8_forbidden[] = {
        "configure_dual_hw_backend",
        "start_dual_hw_streaming",
        "stop_dual_hw_streaming",
        "snapshot_dual_hw_position",
        "static uint16_t s_dma_dual[DSO_DMA_BUF_SIZE]",
        "configure_three_phase_backend",
        "start_triphasic_streaming",
        "stop_triphasic_streaming",
        "snapshot_three_phase_position",
        "8bit_dual_hw",
        "topology_is_dual_hw",
        "topology_is_three_phase",
        "8bit_three_phase"
    };
    static const char *const engine_forbidden[] = {
        "fill_dma_lane_sequence_dual",
        "fill_unpack_sequence_dual",
        "DUAL_FAST_PRODUCTION",
        "TRIPLE_EXPERIMENTAL",
        "TOPOLOGY_THREE_PHASE"
    };
    static const char *const contract_forbidden[] = {
        "DUAL_FAST_PRODUCTION",
        "TRIPLE_EXPERIMENTAL",
        "8bit_dual_hw",
        "8bit_triple_exp",
        "TOPOLOGY_THREE_PHASE"
    };

    if (assert_file_not_contains("User/dso/dso_acq_8bit.c",
                                 acq8_forbidden,
                                 sizeof(acq8_forbidden) / sizeof(acq8_forbidden[0])) != 0) {
        return 1;
    }

    if (assert_file_not_contains("User/dso/dso_engine.c",
                                 engine_forbidden,
                                 sizeof(engine_forbidden) / sizeof(engine_forbidden[0])) != 0) {
        return 1;
    }

    if (assert_file_not_contains("User/dso/dso_serial_debug.c",
                                 contract_forbidden,
                                 sizeof(contract_forbidden) / sizeof(contract_forbidden[0])) != 0) {
        return 1;
    }

    if (assert_file_not_contains("User/dso/dso_serial_debug_format.c",
                                 contract_forbidden,
                                 sizeof(contract_forbidden) / sizeof(contract_forbidden[0])) != 0) {
        return 1;
    }

    if (assert_file_not_contains("User/dso/dso_serial_debug_format.h",
                                 contract_forbidden,
                                 sizeof(contract_forbidden) / sizeof(contract_forbidden[0])) != 0) {
        return 1;
    }

    puts("all acq8 legacy cleanup tests passed");
    return 0;
}
