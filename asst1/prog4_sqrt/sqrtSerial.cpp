#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>


void sqrtSerial(int N,
                float initialGuess,
                float values[],
                float output[])
{

    static const float kThreshold = 0.00001f;

    for (int i=0; i<N; i++) {

        float x = values[i];
        float guess = initialGuess;

        float error = fabs(guess * guess * x - 1.f);

        while (error > kThreshold) {
            guess = (3.f * guess - x * guess * guess * guess) * 0.5f;
            error = fabs(guess * guess * x - 1.f);
        }

        output[i] = x * guess;
    }
}

void sqrtAVX2(int N,
                float initialGuess,
                float values[],
                float output[]) 
{
    static const __m256 kThreshold = _mm256_set1_ps(0.00001f);
    if (N % 8) {
        printf("The number N has to be a multiple of 8\n");
        return; 
    }
    for (int i=0; i<N; i+=8) {
        __m256 x = _mm256_loadu_ps(values + i); // float x = values[i];

        __m256 guess = _mm256_set1_ps(initialGuess); //float guess = initialGuess;

        while (true) {
            // __mm256_mul_ps(guess, guess) = guess * guess, _mm256_mul_ps(_mm256_mul_ps(guess, guess), x) = gues * gues * x
            __m256 error = _mm256_sub_ps(_mm256_mul_ps(_mm256_mul_ps(guess, guess), x), _mm256_set1_ps(1.f));

            // perform not in -0.f (1 (0x31)) -> (011111111...), which clears sign bit, then AND with error to make sure error is positive, error = fabs(error)
            error = _mm256_andnot_ps(_mm256_set1_ps(-0.f), error);
            
            // if error < threashold, break out of the while loop
            __m256 cmp = _mm256_cmp_ps(error, kThreshold, _CMP_GT_OQ);
            if (_mm256_testz_ps(cmp, cmp)) {
                break;
            }

            // update the guess
            __m256 newGuess = _mm256_mul_ps(_mm256_mul_ps(
                                    _mm256_sub_ps(_mm256_set1_ps(3.f), _mm256_mul_ps(x, _mm256_mul_ps(guess, guess))), guess), 
                                _mm256_set1_ps(0.5f));

            guess = _mm256_blendv_ps(guess, newGuess, cmp);
        }
        _mm256_storeu_ps(output + i, _mm256_mul_ps(x, guess));
    }
}