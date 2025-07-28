#include "sh_utils.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
namespace diverse
{
    const float kSqrt03_02  = std::sqrt(3.0f /  2.0f);
    const float kSqrt01_03  = std::sqrt(1.0f /  3.0f);
    const float kSqrt02_03  = std::sqrt(2.0f /  3.0f);
    const float kSqrt04_03  = std::sqrt(4.0f /  3.0f);
    const float kSqrt01_04  = std::sqrt(1.0f /  4.0f);
    const float kSqrt03_04  = std::sqrt(3.0f /  4.0f);
    const float kSqrt01_05  = std::sqrt(1.0f /  5.0f);
    const float kSqrt03_05  = std::sqrt(3.0f /  5.0f);
    const float kSqrt06_05  = std::sqrt(6.0f /  5.0f);
    const float kSqrt08_05  = std::sqrt(8.0f /  5.0f);
    const float kSqrt09_05  = std::sqrt(9.0f /  5.0f);
    const float kSqrt01_06  = std::sqrt(1.0f /  6.0f);
    const float kSqrt05_06  = std::sqrt(5.0f /  6.0f);
    const float kSqrt03_08  = std::sqrt(3.0f /  8.0f);
    const float kSqrt05_08  = std::sqrt(5.0f /  8.0f);
    const float kSqrt09_08  = std::sqrt(9.0f /  8.0f);
    const float kSqrt05_09  = std::sqrt(5.0f /  9.0f);
    const float kSqrt08_09  = std::sqrt(8.0f /  9.0f);
    const float kSqrt01_10  = std::sqrt(1.0f / 10.0f);
    const float kSqrt03_10  = std::sqrt(3.0f / 10.0f);
    const float kSqrt01_12  = std::sqrt(1.0f / 12.0f);
    const float kSqrt04_15  = std::sqrt(4.0f / 15.0f);
    const float kSqrt01_16  = std::sqrt(1.0f / 16.0f);
    const float kSqrt15_16  = std::sqrt(15.0f / 16.0f);
    const float kSqrt01_18  = std::sqrt(1.0f / 18.0f);
    const float kSqrt01_60  = std::sqrt(1.0f / 60.0f);

    auto dp(int n, int start,const std::vector<float>& a,const std::vector<float>& b)->float
    {
        float sum = 0;
        for (int i = 0; i < n; i++)
        {
            sum += a[start + i] * b[i];
        }
        return sum;
    }
    static std::vector<float> coeffsIn(15);

    SHRotation::SHRotation(const glm::mat3& mat)
    {
        auto rot = glm::value_ptr(mat);
        const std::vector<std::vector<float>> sh1 = {
            std::vector<float>{rot[4], -rot[7], rot[1]},
            {-rot[5], rot[8], -rot[2]},
            {rot[3], -rot[6], rot[0]}
        };

        const std::vector<std::vector<float>> sh2 = {{
            kSqrt01_04 * ((sh1[2][2] * sh1[0][0] + sh1[2][0] * sh1[0][2]) + (sh1[0][2] * sh1[2][0] + sh1[0][0] * sh1[2][2])),
                (sh1[2][1] * sh1[0][0] + sh1[0][1] * sh1[2][0]),
            kSqrt03_04 * (sh1[2][1] * sh1[0][1] + sh1[0][1] * sh1[2][1]),
                (sh1[2][1] * sh1[0][2] + sh1[0][1] * sh1[2][2]),
            kSqrt01_04 * ((sh1[2][2] * sh1[0][2] - sh1[2][0] * sh1[0][0]) + (sh1[0][2] * sh1[2][2] - sh1[0][0] * sh1[2][0]))
        }, {
            kSqrt01_04 * ((sh1[1][2] * sh1[0][0] + sh1[1][0] * sh1[0][2]) + (sh1[0][2] * sh1[1][0] + sh1[0][0] * sh1[1][2])),
                sh1[1][1] * sh1[0][0] + sh1[0][1] * sh1[1][0],
            kSqrt03_04 * (sh1[1][1] * sh1[0][1] + sh1[0][1] * sh1[1][1]),
                sh1[1][1] * sh1[0][2] + sh1[0][1] * sh1[1][2],
            kSqrt01_04 * ((sh1[1][2] * sh1[0][2] - sh1[1][0] * sh1[0][0]) + (sh1[0][2] * sh1[1][2] - sh1[0][0] * sh1[1][0]))
        }, {
            kSqrt01_03 * (sh1[1][2] * sh1[1][0] + sh1[1][0] * sh1[1][2]) - kSqrt01_12 * ((sh1[2][2] * sh1[2][0] + sh1[2][0] * sh1[2][2]) + (sh1[0][2] * sh1[0][0] + sh1[0][0] * sh1[0][2])),
            kSqrt04_03 * sh1[1][1] * sh1[1][0] - kSqrt01_03 * (sh1[2][1] * sh1[2][0] + sh1[0][1] * sh1[0][0]),
                        sh1[1][1] * sh1[1][1] - kSqrt01_04 * (sh1[2][1] * sh1[2][1] + sh1[0][1] * sh1[0][1]),
            kSqrt04_03 * sh1[1][1] * sh1[1][2] - kSqrt01_03 * (sh1[2][1] * sh1[2][2] + sh1[0][1] * sh1[0][2]),
            kSqrt01_03 * (sh1[1][2] * sh1[1][2] - sh1[1][0] * sh1[1][0]) - kSqrt01_12 * ((sh1[2][2] * sh1[2][2] - sh1[2][0] * sh1[2][0]) + (sh1[0][2] * sh1[0][2] - sh1[0][0] * sh1[0][0]))
        }, {
            kSqrt01_04 * ((sh1[1][2] * sh1[2][0] + sh1[1][0] * sh1[2][2]) + (sh1[2][2] * sh1[1][0] + sh1[2][0] * sh1[1][2])),
                sh1[1][1] * sh1[2][0] + sh1[2][1] * sh1[1][0],
            kSqrt03_04 * (sh1[1][1] * sh1[2][1] + sh1[2][1] * sh1[1][1]),
                sh1[1][1] * sh1[2][2] + sh1[2][1] * sh1[1][2],
            kSqrt01_04 * ((sh1[1][2] * sh1[2][2] - sh1[1][0] * sh1[2][0]) + (sh1[2][2] * sh1[1][2] - sh1[2][0] * sh1[1][0]))
        }, {
            kSqrt01_04 * ((sh1[2][2] * sh1[2][0] + sh1[2][0] * sh1[2][2]) - (sh1[0][2] * sh1[0][0] + sh1[0][0] * sh1[0][2])),
                (sh1[2][1] * sh1[2][0] - sh1[0][1] * sh1[0][0]),
            kSqrt03_04 * (sh1[2][1] * sh1[2][1] - sh1[0][1] * sh1[0][1]),
                (sh1[2][1] * sh1[2][2] - sh1[0][1] * sh1[0][2]),
            kSqrt01_04 * ((sh1[2][2] * sh1[2][2] - sh1[2][0] * sh1[2][0]) - (sh1[0][2] * sh1[0][2] - sh1[0][0] * sh1[0][0]))
        }};

        // band 3
        const std::vector<std::vector<float>> sh3 = {{
            kSqrt01_04 * ((sh1[2][2] * sh2[0][0] + sh1[2][0] * sh2[0][4]) + (sh1[0][2] * sh2[4][0] + sh1[0][0] * sh2[4][4])),
                kSqrt03_02 * (sh1[2][1] * sh2[0][0] + sh1[0][1] * sh2[4][0]),
                kSqrt15_16 * (sh1[2][1] * sh2[0][1] + sh1[0][1] * sh2[4][1]),
                kSqrt05_06 * (sh1[2][1] * sh2[0][2] + sh1[0][1] * sh2[4][2]),
                kSqrt15_16 * (sh1[2][1] * sh2[0][3] + sh1[0][1] * sh2[4][3]),
                kSqrt03_02 * (sh1[2][1] * sh2[0][4] + sh1[0][1] * sh2[4][4]),
                kSqrt01_04 * ((sh1[2][2] * sh2[0][4] - sh1[2][0] * sh2[0][0]) + (sh1[0][2] * sh2[4][4] - sh1[0][0] * sh2[4][0]))
        }, {
            kSqrt01_06 * (sh1[1][2] * sh2[0][0] + sh1[1][0] * sh2[0][4]) + kSqrt01_06 * ((sh1[2][2] * sh2[1][0] + sh1[2][0] * sh2[1][4]) + (sh1[0][2] * sh2[3][0] + sh1[0][0] * sh2[3][4])),
                sh1[1][1] * sh2[0][0] + (sh1[2][1] * sh2[1][0] + sh1[0][1] * sh2[3][0]),
                kSqrt05_08 * sh1[1][1] * sh2[0][1] + kSqrt05_08 * (sh1[2][1] * sh2[1][1] + sh1[0][1] * sh2[3][1]),
                kSqrt05_09 * sh1[1][1] * sh2[0][2] + kSqrt05_09 * (sh1[2][1] * sh2[1][2] + sh1[0][1] * sh2[3][2]),
                kSqrt05_08 * sh1[1][1] * sh2[0][3] + kSqrt05_08 * (sh1[2][1] * sh2[1][3] + sh1[0][1] * sh2[3][3]),
                sh1[1][1] * sh2[0][4] + (sh1[2][1] * sh2[1][4] + sh1[0][1] * sh2[3][4]),
                kSqrt01_06 * (sh1[1][2] * sh2[0][4] - sh1[1][0] * sh2[0][0]) + kSqrt01_06 * ((sh1[2][2] * sh2[1][4] - sh1[2][0] * sh2[1][0]) + (sh1[0][2] * sh2[3][4] - sh1[0][0] * sh2[3][0]))
        }, {
            kSqrt04_15 * (sh1[1][2] * sh2[1][0] + sh1[1][0] * sh2[1][4]) + kSqrt01_05 * (sh1[0][2] * sh2[2][0] + sh1[0][0] * sh2[2][4]) - kSqrt01_60 * ((sh1[2][2] * sh2[0][0] + sh1[2][0] * sh2[0][4]) - (sh1[0][2] * sh2[4][0] + sh1[0][0] * sh2[4][4])),
                kSqrt08_05 * sh1[1][1] * sh2[1][0] + kSqrt06_05 * sh1[0][1] * sh2[2][0] - kSqrt01_10 * (sh1[2][1] * sh2[0][0] - sh1[0][1] * sh2[4][0]),
                sh1[1][1] * sh2[1][1] + kSqrt03_04 * sh1[0][1] * sh2[2][1] - kSqrt01_16 * (sh1[2][1] * sh2[0][1] - sh1[0][1] * sh2[4][1]),
                kSqrt08_09 * sh1[1][1] * sh2[1][2] + kSqrt02_03 * sh1[0][1] * sh2[2][2] - kSqrt01_18 * (sh1[2][1] * sh2[0][2] - sh1[0][1] * sh2[4][2]),
                sh1[1][1] * sh2[1][3] + kSqrt03_04 * sh1[0][1] * sh2[2][3] - kSqrt01_16 * (sh1[2][1] * sh2[0][3] - sh1[0][1] * sh2[4][3]),
                kSqrt08_05 * sh1[1][1] * sh2[1][4] + kSqrt06_05 * sh1[0][1] * sh2[2][4] - kSqrt01_10 * (sh1[2][1] * sh2[0][4] - sh1[0][1] * sh2[4][4]),
                kSqrt04_15 * (sh1[1][2] * sh2[1][4] - sh1[1][0] * sh2[1][0]) + kSqrt01_05 * (sh1[0][2] * sh2[2][4] - sh1[0][0] * sh2[2][0]) - kSqrt01_60 * ((sh1[2][2] * sh2[0][4] - sh1[2][0] * sh2[0][0]) - (sh1[0][2] * sh2[4][4] - sh1[0][0] * sh2[4][0]))
        }, {
            kSqrt03_10 * (sh1[1][2] * sh2[2][0] + sh1[1][0] * sh2[2][4]) - kSqrt01_10 * ((sh1[2][2] * sh2[3][0] + sh1[2][0] * sh2[3][4]) + (sh1[0][2] * sh2[1][0] + sh1[0][0] * sh2[1][4])),
                kSqrt09_05 * sh1[1][1] * sh2[2][0] - kSqrt03_05 * (sh1[2][1] * sh2[3][0] + sh1[0][1] * sh2[1][0]),
                kSqrt09_08 * sh1[1][1] * sh2[2][1] - kSqrt03_08 * (sh1[2][1] * sh2[3][1] + sh1[0][1] * sh2[1][1]),
                sh1[1][1] * sh2[2][2] - kSqrt01_03 * (sh1[2][1] * sh2[3][2] + sh1[0][1] * sh2[1][2]),
                kSqrt09_08 * sh1[1][1] * sh2[2][3] - kSqrt03_08 * (sh1[2][1] * sh2[3][3] + sh1[0][1] * sh2[1][3]),
                kSqrt09_05 * sh1[1][1] * sh2[2][4] - kSqrt03_05 * (sh1[2][1] * sh2[3][4] + sh1[0][1] * sh2[1][4]),
                kSqrt03_10 * (sh1[1][2] * sh2[2][4] - sh1[1][0] * sh2[2][0]) - kSqrt01_10 * ((sh1[2][2] * sh2[3][4] - sh1[2][0] * sh2[3][0]) + (sh1[0][2] * sh2[1][4] - sh1[0][0] * sh2[1][0]))
        }, {
            kSqrt04_15 * (sh1[1][2] * sh2[3][0] + sh1[1][0] * sh2[3][4]) + kSqrt01_05 * (sh1[2][2] * sh2[2][0] + sh1[2][0] * sh2[2][4]) - kSqrt01_60 * ((sh1[2][2] * sh2[4][0] + sh1[2][0] * sh2[4][4]) + (sh1[0][2] * sh2[0][0] + sh1[0][0] * sh2[0][4])),
                kSqrt08_05 * sh1[1][1] * sh2[3][0] + kSqrt06_05 * sh1[2][1] * sh2[2][0] - kSqrt01_10 * (sh1[2][1] * sh2[4][0] + sh1[0][1] * sh2[0][0]),
                sh1[1][1] * sh2[3][1] + kSqrt03_04 * sh1[2][1] * sh2[2][1] - kSqrt01_16 * (sh1[2][1] * sh2[4][1] + sh1[0][1] * sh2[0][1]),
                kSqrt08_09 * sh1[1][1] * sh2[3][2] + kSqrt02_03 * sh1[2][1] * sh2[2][2] - kSqrt01_18 * (sh1[2][1] * sh2[4][2] + sh1[0][1] * sh2[0][2]),
                sh1[1][1] * sh2[3][3] + kSqrt03_04 * sh1[2][1] * sh2[2][3] - kSqrt01_16 * (sh1[2][1] * sh2[4][3] + sh1[0][1] * sh2[0][3]),
                kSqrt08_05 * sh1[1][1] * sh2[3][4] + kSqrt06_05 * sh1[2][1] * sh2[2][4] - kSqrt01_10 * (sh1[2][1] * sh2[4][4] + sh1[0][1] * sh2[0][4]),
                kSqrt04_15 * (sh1[1][2] * sh2[3][4] - sh1[1][0] * sh2[3][0]) + kSqrt01_05 * (sh1[2][2] * sh2[2][4] - sh1[2][0] * sh2[2][0]) - kSqrt01_60 * ((sh1[2][2] * sh2[4][4] - sh1[2][0] * sh2[4][0]) + (sh1[0][2] * sh2[0][4] - sh1[0][0] * sh2[0][0]))
        }, {
            kSqrt01_06 * (sh1[1][2] * sh2[4][0] + sh1[1][0] * sh2[4][4]) + kSqrt01_06 * ((sh1[2][2] * sh2[3][0] + sh1[2][0] * sh2[3][4]) - (sh1[0][2] * sh2[1][0] + sh1[0][0] * sh2[1][4])),
                sh1[1][1] * sh2[4][0] + (sh1[2][1] * sh2[3][0] - sh1[0][1] * sh2[1][0]),
                kSqrt05_08 * sh1[1][1] * sh2[4][1] + kSqrt05_08 * (sh1[2][1] * sh2[3][1] - sh1[0][1] * sh2[1][1]),
                kSqrt05_09 * sh1[1][1] * sh2[4][2] + kSqrt05_09 * (sh1[2][1] * sh2[3][2] - sh1[0][1] * sh2[1][2]),
                kSqrt05_08 * sh1[1][1] * sh2[4][3] + kSqrt05_08 * (sh1[2][1] * sh2[3][3] - sh1[0][1] * sh2[1][3]),
                sh1[1][1] * sh2[4][4] + (sh1[2][1] * sh2[3][4] - sh1[0][1] * sh2[1][4]),
                kSqrt01_06 * (sh1[1][2] * sh2[4][4] - sh1[1][0] * sh2[4][0]) + kSqrt01_06 * ((sh1[2][2] * sh2[3][4] - sh1[2][0] * sh2[3][0]) - (sh1[0][2] * sh2[1][4] - sh1[0][0] * sh2[1][0]))
        }, {
            kSqrt01_04 * ((sh1[2][2] * sh2[4][0] + sh1[2][0] * sh2[4][4]) - (sh1[0][2] * sh2[0][0] + sh1[0][0] * sh2[0][4])),
                kSqrt03_02 * (sh1[2][1] * sh2[4][0] - sh1[0][1] * sh2[0][0]),
                kSqrt15_16 * (sh1[2][1] * sh2[4][1] - sh1[0][1] * sh2[0][1]),
                kSqrt05_06 * (sh1[2][1] * sh2[4][2] - sh1[0][1] * sh2[0][2]),
                kSqrt15_16 * (sh1[2][1] * sh2[4][3] - sh1[0][1] * sh2[0][3]),
                kSqrt03_02 * (sh1[2][1] * sh2[4][4] - sh1[0][1] * sh2[0][4]),
                kSqrt01_04 * ((sh1[2][2] * sh2[4][4] - sh1[2][0] * sh2[4][0]) - (sh1[0][2] * sh2[0][4] - sh1[0][0] * sh2[0][0]))
        }};

        apply = [=](std::vector<float>& result, const std::vector<float>& src_p)->void{
            auto src = src_p;
            if (src.empty() || src == result) {
                coeffsIn = result;
                src = coeffsIn;
            }
            // band 1
            if(result.size() < 3) return;
            result[0] = dp(3, 0, src, sh1[0]);
            result[1] = dp(3, 0, src, sh1[1]);
            result[2] = dp(3, 0, src, sh1[2]);

            // band 2
            if (result.size() < 8) return;
            result[3] = dp(5, 3, src, sh2[0]);
            result[4] = dp(5, 3, src, sh2[1]);
            result[5] = dp(5, 3, src, sh2[2]);
            result[6] = dp(5, 3, src, sh2[3]);
            result[7] = dp(5, 3, src, sh2[4]);

            // band 3
            if (result.size() < 15) {
                return;
            }
            result[8] = dp(7, 8, src, sh3[0]);
            result[9] = dp(7, 8, src, sh3[1]);
            result[10] = dp(7, 8, src, sh3[2]);
            result[11] = dp(7, 8, src, sh3[3]);
            result[12] = dp(7, 8, src, sh3[4]);
            result[13] = dp(7, 8, src, sh3[5]);
            result[14] = dp(7, 8, src, sh3[6]);
        };
    }
}