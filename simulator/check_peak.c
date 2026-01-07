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
#define MODEL_EPOCH_UNIX 1546301340L

float calculate_tide_height(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float height = 2.64f;
    for (int i = 0; i < 31; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        height += constituents[i].amplitude * cosf(angle);
    }
    return height;
}

int main() {
    // Jan 29 2025, find peak around 12:00 UTC
    struct tm tm_base = {0};
    tm_base.tm_year = 2025 - 1900;
    tm_base.tm_mon = 0;
    tm_base.tm_mday = 29;
    tm_base.tm_hour = 11;
    tm_base.tm_min = 0;
    time64_t base = timegm(&tm_base);
    
    printf("Jan 29 2025, minute-by-minute around noon UTC:\n");
    float max_h = 0;
    int max_min = 0;
    for (int m = 0; m <= 120; m++) {
        time64_t t = base + m * 60;
        float h = calculate_tide_height(t);
        if (h > max_h) { max_h = h; max_min = m; }
        if (m >= 50 && m <= 70) {
            printf("  11:%02d UTC: %.4f m%s\n", m, h, (m == max_min) ? " <-- MAX" : "");
        }
    }
    printf("\nPeak at 11:%02d UTC (%.4f m)\n", max_min, max_h);
    printf("PLA says HW at 12:03 UTC\n");
    
    return 0;
}
