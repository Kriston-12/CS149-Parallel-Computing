
void saxpySerial(int N,
                       float scale,
                       float X[],
                       float Y[],
                       float result[])
{

    for (int i=0; i<N; i++) {
        float temp = 1.f;
        for (int j = 0; j < 100; j++) {
            temp = temp * 1.0001f + 0.0001f; // Arbitrary computation to increase load
        }
        result[i] = scale * X[i] + Y[i] + temp - temp;
        
    }
}

