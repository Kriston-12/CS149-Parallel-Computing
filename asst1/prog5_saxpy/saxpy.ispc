
export void saxpy_ispc(uniform int N,
                       uniform float scale,
                            uniform float X[],
                            uniform float Y[],
                            uniform float result[])
{   
    // uniform int tileSize = 8;   // lscpu shows l1 data and l1 instruction are both 256 KiB
                                // getconf LEVEL1_DCACHE_LINESIZE gives 64, which means our cache line is 64
                                // so we could load 64/(4 + 4) = 8--load 64 bytes of data from mem to cache
                                // and using the cache for the computation to use loop locality 
    // for (uniform int t = 0; t < N; t+=tileSize) {
    //     uniform int end = min(t + tileSize, N);
    //     foreach (i = t ... end) {           
    //         result[i] = scale * X[i] + Y[i];
    //     }
    // }
    float temp;
    foreach (i = 0 ... N) {
        temp = scale * X[i];
        result[i] = temp + Y[i];
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
