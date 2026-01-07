#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

typedef int64_t time64_t;

typedef struct {
    float amplitude;
    float omega;
    float phase_rad;
} TideConstituent;

#define DEG_TO_RAD 0.01745329252f
#define MODEL_EPOCH_UNIX 1546301340L

static TideConstituent constituents[] = {
    {1.6236f, 28.9841042f * DEG_TO_RAD, 209.601f * DEG_TO_RAD},
    {0.4758f, 30.0000000f * DEG_TO_RAD, 30.617f * DEG_TO_RAD},
    {0.2846f, 28.4397295f * DEG_TO_RAD, 263.418f * DEG_TO_RAD},
    {0.1624f, 30.0821373f * DEG_TO_RAD, 196.250f * DEG_TO_RAD},
    {0.1031f, 15.0410686f * DEG_TO_RAD, 357.212f * DEG_TO_RAD},
    {0.1272f, 13.9430356f * DEG_TO_RAD, 59.267f * DEG_TO_RAD},
    {0.0361f, 14.9589314f * DEG_TO_RAD, 354.719f * DEG_TO_RAD},
    {0.0415f, 13.3986609f * DEG_TO_RAD, 79.892f * DEG_TO_RAD},
    {0.0913f, 27.9682084f * DEG_TO_RAD, 187.004f * DEG_TO_RAD},
    {0.0824f, 28.5125831f * DEG_TO_RAD, 330.693f * DEG_TO_RAD},
    {0.0306f, 27.8953548f * DEG_TO_RAD, 313.497f * DEG_TO_RAD},
    {0.1087f, 29.5284789f * DEG_TO_RAD, 324.568f * DEG_TO_RAD},
    {0.0224f, 29.9589333f * DEG_TO_RAD, 15.603f * DEG_TO_RAD},
    {0.0301f, 31.0158958f * DEG_TO_RAD, 14.171f * DEG_TO_RAD},
    {0.0475f, 29.4556253f * DEG_TO_RAD, 245.613f * DEG_TO_RAD},
    {0.0063f, 15.5854433f * DEG_TO_RAD, 22.363f * DEG_TO_RAD},
    {0.0049f, 16.1391017f * DEG_TO_RAD, 77.357f * DEG_TO_RAD},
    {0.0065f, 12.8542862f * DEG_TO_RAD, 127.521f * DEG_TO_RAD},
    {0.0083f, 13.4715145f * DEG_TO_RAD, 152.544f * DEG_TO_RAD},
    {0.0525f, 57.9682084f * DEG_TO_RAD, 38.983f * DEG_TO_RAD},
    {0.0225f, 58.9841042f * DEG_TO_RAD, 232.210f * DEG_TO_RAD},
    {0.0184f, 57.4238337f * DEG_TO_RAD, 87.083f * DEG_TO_RAD},
    {0.0063f, 59.0662415f * DEG_TO_RAD, 39.766f * DEG_TO_RAD},
    {0.0009f, 60.0000000f * DEG_TO_RAD, 20.681f * DEG_TO_RAD},
    {0.0092f, 86.9523127f * DEG_TO_RAD, 346.092f * DEG_TO_RAD},
    {0.0048f, 86.4079380f * DEG_TO_RAD, 33.689f * DEG_TO_RAD},
    {0.0070f, 87.9682084f * DEG_TO_RAD, 165.004f * DEG_TO_RAD},
    {0.0198f, 44.0251729f * DEG_TO_RAD, 128.877f * DEG_TO_RAD},
    {0.0266f, 42.9271398f * DEG_TO_RAD, 201.290f * DEG_TO_RAD},
    {0.0087f, 1.0980331f * DEG_TO_RAD, 104.409f * DEG_TO_RAD},
    {0.0075f, 0.5443747f * DEG_TO_RAD, 48.050f * DEG_TO_RAD},
};

float calculate_tide_derivative(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float deriv = 0.0f;
    for (int i = 0; i < 31; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        deriv -= constituents[i].amplitude * constituents[i].omega * sinf(angle);
    }
    return deriv;
}

float calculate_tide_second_derivative(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float deriv2 = 0.0f;
    for (int i = 0; i < 31; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        deriv2 -= constituents[i].amplitude * constituents[i].omega * constituents[i].omega * cosf(angle);
    }
    return deriv2;
}

time64_t find_extremum_newton(time64_t start_time, int find_max, time64_t min_time, int depth) {
    printf("  [depth=%d] Starting from ", depth);
    struct tm *t = gmtime((time_t*)&start_time);
    printf("%02d:%02d, looking for %s, min_time=%02d:%02d\n", 
           t->tm_hour, t->tm_min, find_max ? "MAX" : "MIN",
           (int)((min_time % 86400) / 3600), (int)((min_time % 3600) / 60));
    
    float t_hours = (float)(start_time - MODEL_EPOCH_UNIX) / 3600.0f;

    for (int iter = 0; iter < 20; iter++) {
        time64_t t_unix = MODEL_EPOCH_UNIX + (time64_t)(t_hours * 3600.0f);
        float h_prime = calculate_tide_derivative(t_unix);
        float h_double_prime = calculate_tide_second_derivative(t_unix);

        if (fabsf(h_double_prime) < 1e-10f) break;

        float delta = h_prime / h_double_prime;
        t_hours -= delta;

        if (fabsf(delta) < 1.0f / 3600.0f) break;
    }

    time64_t result = MODEL_EPOCH_UNIX + (time64_t)(t_hours * 3600.0f);
    
    struct tm *r = gmtime((time_t*)&result);
    float h2 = calculate_tide_second_derivative(result);
    int is_max = (h2 < 0);
    printf("  [depth=%d] Converged to %02d:%02d, is_max=%d, h''=%.4f\n", 
           depth, r->tm_hour, r->tm_min, is_max, h2);

    if (is_max != find_max || result < min_time) {
        printf("  [depth=%d] Wrong type or before min, recursing +6h\n", depth);
        return find_extremum_newton(result + 6 * 3600, find_max, min_time, depth+1);
    }

    return result;
}

int main() {
    // Jan 3 2025 12:00 UTC
    struct tm tm = {0};
    tm.tm_year = 2025 - 1900;
    tm.tm_mon = 0;
    tm.tm_mday = 3;
    tm.tm_hour = 12;
    time64_t dt = timegm(&tm);
    
    printf("Query time: 2025-01-03 12:00 UTC\n");
    printf("Looking for next HW:\n");
    time64_t hw = find_extremum_newton(dt, 1, dt, 0);
    struct tm *hw_tm = gmtime((time_t*)&hw);
    printf("Result: %04d-%02d-%02d %02d:%02d UTC\n\n",
           hw_tm->tm_year+1900, hw_tm->tm_mon+1, hw_tm->tm_mday,
           hw_tm->tm_hour, hw_tm->tm_min);
    
    return 0;
}
