
export void saxpy_ispc(uniform int N,
                       uniform float scale,
                            uniform float X[],
                            uniform float Y[],
                            uniform float result[])
{
    foreach (i = 0 ... N) {           
        uniform float temp;
        for (int j = 0; j < 100; j++) {
            temp = temp * 1.0001f + 0.0001f; // Arbitrary computation to increase load
        }
        result[i] = scale * X[i] + Y[i];
        
    }
}

task void saxpy_ispc_task(uniform int N,
                               uniform int span,
                               uniform float scale,
                               uniform float X[], 
                               uniform float Y[],
                               uniform float result[])
{

    uniform int indexStart = taskIndex * span;
    uniform int indexEnd = min(N, indexStart + span);

    foreach (i = indexStart ... indexEnd) {
        uniform float temp;
        for (int j = 0; j < 100; j++) {
            temp = temp * 1.0001f + 0.0001f; // Arbitrary computation to increase load
        }
        result[i] = scale * X[i] + Y[i];
            
    }
}

export void saxpy_ispc_withtasks(uniform int N,
                               uniform float scale,
                               uniform float X[],
                               uniform float Y[],
                               uniform float result[])
{

    uniform int span = N / 64;  // 64 tasks

    launch[N/span] saxpy_ispc_task(N, span, scale, X, Y, result);
}
