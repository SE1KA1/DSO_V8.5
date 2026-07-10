#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long len;
    char *buf;

    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (char *)malloc((size_t)len + 1u);
    if (buf == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (fread(buf, 1u, (size_t)len, fp) != (size_t)len) {
        fprintf(stderr, "failed to read %s\n", path);
        exit(1);
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

static void require_text(const char *buf, const char *needle)
{
    if (strstr(buf, needle) == NULL) {
        fprintf(stderr, "missing text: %s\n", needle);
        exit(1);
    }
}

int main(void)
{
    char *freq_h = read_file("User/dso/dso_freq_meter.h");
    char *math_h = read_file("User/dso/dso_freq_meter_math.h");
    char *proc_h = read_file("User/dso/dso_process.h");

    require_text(freq_h, "DSO_FREQ_METER_STATE_WAIT_EDGE");
    require_text(freq_h, "DSO_FREQ_METER_STATE_MEASURING");
    require_text(freq_h, "DSO_FREQ_METER_STATE_NO_SIGNAL");
    require_text(freq_h, "DSO_FREQ_METER_STATE_OVERRANGE");
    require_text(freq_h, "uint64_t freq_millihz;");
    require_text(freq_h, "uint32_t gate_us;");
    require_text(freq_h, "uint32_t last_edge_age_ms;");
    require_text(math_h, "DSO_FREQ_METER_MODE_LOW_CAPTURE");
    require_text(math_h, "DSO_FREQ_METER_MODE_GATED_COUNT");

    /* DSO_Frame keeps only the UI-facing ext_freq fields; gate_us is canonical in dso_freq_meter.h. */
    require_text(proc_h, "bool     ext_freq_valid;");
    require_text(proc_h, "uint8_t  ext_freq_state;");
    require_text(proc_h, "uint8_t  ext_freq_mode;");
    require_text(proc_h, "uint64_t ext_freq_millihz;");

    free(freq_h);
    free(math_h);
    free(proc_h);
    return 0;
}
