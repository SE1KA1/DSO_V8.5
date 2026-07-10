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

static void reject_text(const char *buf, const char *needle)
{
    if (strstr(buf, needle) != NULL) {
        fprintf(stderr, "forbidden text still present: %s\n", needle);
        exit(1);
    }
}

static char *extract_function_body(const char *buf, const char *signature)
{
    const char *start = strstr(buf, signature);
    const char *next;
    size_t len;
    char *body;

    if (start == NULL) {
        fprintf(stderr, "missing function: %s\n", signature);
        exit(1);
    }

    next = strstr(start + strlen(signature), "\nvoid ");
    if (next == NULL) {
        fprintf(stderr, "failed to find end of function: %s\n", signature);
        exit(1);
    }

    len = (size_t)(next - start);
    body = (char *)malloc(len + 1u);
    if (body == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    memcpy(body, start, len);
    body[len] = '\0';
    return body;
}

int main(void)
{
    char *meter_c = read_file("User/dso/dso_freq_meter.c");
    char *start_body = extract_function_body(meter_c, "void DSO_FreqMeter_Start(void)");

    require_text(meter_c, "typedef struct {");
    require_text(meter_c, "DSO_FreqMeterState state;");
    require_text(meter_c, "DSO_FreqMeterMode mode;");
    require_text(meter_c, "static void enter_low_capture_mode");
    require_text(meter_c, "static void enter_gated_mode");
    require_text(meter_c, "static void service_low_capture");
    require_text(meter_c, "static void service_gated");
    require_text(meter_c, "DSO_FreqMeterMath_SelectMode");
    require_text(meter_c, "DSO_FREQ_METER_STATE_NO_SIGNAL");
    require_text(meter_c, "DSO_FREQ_METER_STATE_WAIT_EDGE");
    require_text(meter_c, "DSO_FREQ_METER_STATE_VALID");
    require_text(meter_c, "if (s_meter.state != DSO_FREQ_METER_STATE_VALID)");
    require_text(start_body, "enter_gated_mode(now_ms);");
    reject_text(meter_c, "(s_meter.state == DSO_FREQ_METER_STATE_NO_SIGNAL) &&");
    reject_text(meter_c, "enter_period_mode");
    reject_text(meter_c, "enter_reciprocal_mode");
    reject_text(meter_c, "service_period_like");
    reject_text(start_body, "enter_period_mode(now_ms);");
    reject_text(meter_c, "DSO_FREQ_METER_SMOOTH_GAIN");
    reject_text(meter_c, "DSO_FREQ_METER_JUMP_REL");
    reject_text(meter_c, "s_freq_hz");

    free(start_body);
    free(meter_c);
    return 0;
}
