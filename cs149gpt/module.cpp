#include <torch/extension.h>
#include <ATen/ATen.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <immintrin.h>

// Uncomment for ISPC
//#include "module_ispc.h"
//using namespace ispc;

// ------------------------------------ //
// 	WARM-UP: ACCESSING TENSORS      //
// ------------------------------------ //

// Step #1: Understand Read/Write Accessors for a 2D Tensor
inline float twoDimRead(std::vector<float> &tensor, int &x, int &y, const int &sizeX) {
    // Note that sizeX is the size of a Row, not the number of rows
    // float val = twoDimRead(QK_t, i, j, N);
    return tensor[x * (sizeX)+ y];
}
    /* Here is an example of how to read/write 0's to  QK_t (N, N) using the 2D accessors

           for (int i = 0; i < N; i++) {
	       for (int j = 0; j < N; j++) {
	           float val = twoDimRead(QK_t, i, j, N);
               val = 0.0;
	           twoDimWrite(QK_t, i, j, N, val);
             }
         }
    */
inline void twoDimWrite(std::vector<float> &tensor, int &x, int &y, const int &sizeX, float &val) {
    tensor[x * (sizeX) + y] = val;
}

// Step #2: Implement Read/Write Accessors for a 4D Tensor
inline float fourDimRead(std::vector<float> &tensor, int &x, int &y, int &z, int &b, 
        const int &sizeX, const int &sizeY, const int &sizeZ) { 
    return tensor[x * sizeX * sizeY * sizeZ + y * sizeY * sizeZ + z * sizeZ + b];
    // this is equivalent to (batchIdx * cubeVolume(HxNxD)-locate the cube 
    // + headIndex * planeSize(NxD)-locate the plane + sequenceIndex * rowSize(dimensionSize) + columnOffset(b)
}
    /* Here is an example of how to read/write 0's to  Q (B, H, N, d) using the 4D accessors

        //loop over Batch Size
         for (int b = 0; b < B; b++) {

             //loop over Heads
             for (int h = 0; h < H; h++) {

                 //loop over Sequence Length
                 for (int i = 0; i < N; i++) {

                     //loop over Embedding Dimensionality
                     for (int j = 0; j < d; j++) {
                        float val = fourDimRead(Q, b, h, i, j, H, N, d);
                        val = 0.0;
                        fourDimWrite(Q, b, h, i, j, H, N, d, val);
                     }
                 }
             }
         }
    */

inline void fourDimWrite(std::vector<float> &tensor, int &x, int &y, int &z, int &b, 
        const int &sizeX, const int &sizeY, const int &sizeZ, float &val) {
    tensor[x * sizeX * sizeY * sizeZ + y * sizeY * sizeZ + z * sizeZ + b] = val;
}

// DO NOT EDIT THIS FUNCTION //
std::vector<float> formatTensor(torch::Tensor tensor) {
    tensor = tensor.flatten();
    tensor = tensor.contiguous();
    std::vector<float> vec(tensor.data_ptr<float>(), tensor.data_ptr<float>() + tensor.numel());
    return vec;
}

/* Programming Your Attention Modules.
 * 
 * You are given Q, K, and V Tensors as inputs that are formatted as vectors. We have also created O and QK^t Tensors 
 * that are formatted as vectors. After you have implemented your accessors in the Warm-Up you should be able to
 * read/write to these tensors via the read/write functions above.
 *
 * You are also given 4 integers as parameters: B, H, N, d:
 *
 * B (Batch Size) - The number of samples for your attention layer. Think of it this way - if I asked my dnn
 * a question and it output 5 different answers it had a batch size of 5. These samples are independent of each
 * other and thus can be parallelized.
 *
 * H (Number of Heads) - Each head runs on its own set of Q, K, V matrices. This effectively allows each head
 * to operate the same attention algorithm, but each with each head using different hyperparameters. These
 * allow each head to have their own definition of what relevance is when looking at a token. These heads
 * can operate independently of one another and thus can be parallized.
 *
 * N (Sequence Length) - The number of tokens. You may think of this as the number of words in a sample.
 *
 * d (Embedding Dimensionality) - The number of features each token encodes per attention head. Let's
 * say I encoded a word using the follow (length, number of vowels, has a capital letters). The
 * emvedded dimensionaliy would be 3.
 * */

// ---------------------------------------------------------- //
//                  PART 1: NAIVE ATTENTION                   //
// ---------------------------------------------------------- //

torch::Tensor myNaiveAttention(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor, torch::Tensor QK_tTensor,
                int B, int H, int N, int d){

    // Q, K, V are passed in with Shape: (B, H, N, d)
    //QK^t Intermediate Tensor has Shape (N, N)
    
    //Make O Tensor with Shape (B, H, N, d) 
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);

    //Format O, Q, K, and V tensors into 4D vectors
    std::vector<float> O = formatTensor(OTensor); 
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);

    //Format QK_t Tensor into a 2D vector.
    std::vector<float> QK_t = formatTensor(QK_tTensor);
    
    /* Here is an example of how to read/write 0's to  Q (B, H, N, d) using the 4D accessors

        //loop over Batch Size
         for (int b = 0; b < B; b++) {

             //loop over Heads
             for (int h = 0; h < H; h++) {

                 //loop over Sequence Length
                 for (int i = 0; i < N; i++) {

                     //loop over Embedding Dimensionality
                     for (int j = 0; j < d; j++) {
                        float val = fourDimRead(Q, b, h, i, j, H, N, d);
                        val = 0.0;
                        fourDimWrite(Q, b, h, i, j, H, N, d, val);
                     }
                 }
             }
         }
    */

    /* Here is an example of how to read/write 0's to  QK_t (N, N) using the 2D accessors

           for (int i = 0; i < N; i++) {
	       for (int j = 0; j < N; j++) {
	           float val = twoDimRead(QK_t, i, j, N);
               val = 0.0;
	           twoDimWrite(QK_t, i, j, N, val);
             }
         }
    */
    
    // -------- YOUR CODE HERE  -------- //
    /* float fourDimRead(std::vector<float> &tensor, int &x, int &y, int &z, int &b, 
        const int &sizeX, const int &sizeY, const int &sizeZ) */
    
    //loop over Batches
    for (int b = 0; b < B; b++) {
        //loop over Heads
        for (int h = 0; h < H; h++) {
            //loop over Sequence Length
            for (int i = 0; i < N; i++) {
                // loop over each row of K--each column of Kt
                for (int k = 0; k < N; k++) {
                    float sum = 0.f;
                    //loop over Embedding Dimensionality

                    for (int j = 0; j < d; j++){
                        // 因为这里我们的K是untranposed, 实际上我们在使用K进行 Q * KT的操作
                        // 对于Q的每个element，比如当前Q在第i行, 这一整行 Q[i][0,1,2,3....d-1] 需要和KT的某一列点乘得到结果S的某一个element
                        // Eg: Q[i][0,1,2,3...d-1] * KT[0,1,2,3...d-1][K] = Q[i][K]
                        // 但是这里我们使用的是K而不是KT，所以 Q[i][0, 1, 2, 3... d-1] * K[K][0,1,2,3...d-1]
                        // 下面这里就是在进行这个操作，i和k是外层的循环，表示Q的行号不变，内层j进行沿着行计算，
                        // KT对于Q的每一行，都需要将所有的列和Q的一行进行计算得到S的一整行，这也是为什么k的循环在i的循环内部的原因
                        sum += fourDimRead(Q, b, h, i, j, H, N, d) * fourDimRead(K, b, h, k, j, H, N, d);
                    }
                    // 上面的j循环结束，说明Q的i行和KT的k列点乘完毕，得到 S[i][k], 下面就是正常的写操作
                    twoDimWrite(QK_t, i, k, N, sum);     
                }
            }

            // Get P
            for (int i = 0; i < N; i++) {
                float rowSum = 0.f;
                for (int j = 0; j < N; j++) {
                    // Get Exp of each element and write them back. At the same time, calculate the row Sum for next step--getting P
                    float val = std::exp(twoDimRead(QK_t, i, j, N));
                    // twoDimWrite(QK_t, i, j, N, val); 
                    rowSum += val;
                }

                for (int j = 0; j < N; j++) {
                    float val = std::exp(twoDimRead(QK_t, i, j, N)) / rowSum;
                    twoDimWrite(QK_t, i, j, N, val);
                }
            }

            //3. QK_t * V[b][h]
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < d; j++) {
                    float sum = 0.f;
                    for (int k = 0; k < N; k++) {
                        sum += twoDimRead(QK_t, i, k, N) * fourDimRead(V, b, h, k, j, H, N, d);
                    }
                    fourDimWrite(O, b, h, i, j, H, N, d, sum);
                }
            }
        }
    }


    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


// ---------------------------------------------------------- //
//     PART 2: BLOCKED MATRIX MULTIPLY AND UNFUSED SOFTMAX    //
// ---------------------------------------------------------- //

torch::Tensor myUnfusedAttentionBlocked(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor, torch::Tensor QK_tTensor,
                int B, int H, int N, int d){
    
    // Q, K, V are passed in with Shape: (B, H, N, d)
    //QK^t Intermediate Tensor has Shape (N, N)

    //Make O Tensor with Shape (B, H, N, d) 
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);

    //Format O, Q, K, and V tensors into 4D vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);

    //Format QK_t Tensor into a 2D vector.
    std::vector<float> QK_t = formatTensor(QK_tTensor);

    // -------- YOUR CODE HERE  -------- //
    // L1d = 256kb, cache line = 64 Bytes
    // Just let 3 * blockSize < 256kb would be fine 
    // Coulde select block size = 16 x 16 -- each row of block fits in a line 
    constexpr int dim = 16;
    //loop over Batches
    for (int b = 0; b < B; b++) {
        //loop over Heads
        for (int h = 0; h < H; h++) {
            //loop over Sequence Length
            for (int iblock = 0; iblock < N; iblock += dim) {
                // loop over each row of K--each column of Kt
                for (int kblock = 0; kblock < N; kblock += dim) {
                    for (int jblock = 0; jblock < d; jblock += dim) {
                        int ibound = std::min(N, iblock + dim);
                        int jbound = std::min(d, jblock + dim);
                        int kbound = std::min(N, kblock + dim);
                        
                       
                        for (int i = iblock; i < ibound; ++i) {
                            for (int k = kblock; k < kbound; ++k) {
                                float curVal = twoDimRead(QK_t, i, k, N);
                                for (int j = jblock; j < jbound; ++j) {
                                    curVal += fourDimRead(Q, b, h, i, j, H, N, d) * fourDimRead(K, b, h, k, j, H, N, d);
                                }
                                // End loop j, which means we handled one entire column of a block, we should write it back
                                twoDimWrite(QK_t, i, k, N, curVal);
                            }
                        }
                    }
                }
            }
            // I believe softmax does not need block 
            for (int i = 0; i < N; i++){
                float sum = 0.f;
                for (int j = 0; j < N; j++) {
                    sum += std::exp(twoDimRead(QK_t, i, j, N));
                }
                for (int j = 0; j < N; j++) {
                    float val = std::exp(twoDimRead(QK_t, i, j, N)) / sum;
                    twoDimWrite(QK_t, i, j, N, val);
                }
            }

            // O = P @ V
            for (int iblock = 0; iblock < N; iblock += dim) {
                for (int kblock = 0; kblock < d; kblock += dim) {
                    for (int jblock = 0; jblock < N; jblock += dim) {
                        int ibound = std::min(N, iblock + dim);
                        int jbound = std::min(N, jblock + dim);
                        int kbound = std::min(d, kblock + dim);
                        // fourDimWrite(O, b, h, i, j, H, N, d, sum);
                        

                        for (int i = iblock; i < ibound; ++i) {
                            for (int k = kblock; k < kbound; ++k) {
                                float curVal = fourDimRead(O, b, h, i, k, H, N, d);
                                for (int j = jblock; j < jbound; ++j) {
                                    curVal += twoDimRead(QK_t, i, j, N) * 
                                        fourDimRead(V, b, h, j, k, H, N, d);
                                }
                                fourDimWrite(O, b, h, i, k, H, N, d, curVal); 
                                
                            }
                        }
                    }
                }
            }    

        }
    }    
    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


// ---------------------------------------------------------- //
//                 PART 3: FUSED ATTENTION     	              //
// ---------------------------------------------------------- //

torch::Tensor myFusedAttention(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor, torch::Tensor temp,
                int B, int H, int N, int d){

    // Q, K, V are passed in with Shape: (B, H, N, d)

    //Make O Tensor with Shape (B, H, N, d)
    //and O Row Tensor with Shape (N)
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);
    at::Tensor ORowTensor = at::zeros({N}, at::kFloat);

    //Format Y, Q, K, and V tensors into 4D vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);
    
    //Format ORow Tensor into a 1D vector
    // You can simply access this as ORow[i]
    std::vector<float> ORow = formatTensor(ORowTensor);


    // -------- YOUR CODE HERE  -------- //
    // We give you a template of the first three loops for your convenience
    //loop over batch
    #pragma omp parallel for collapse(3)
    for (int b = 0; b < B; b++){

        //loop over heads
        for (int h = 0; h < H; h++){
            for (int i = 0; i < N ; i++){

		// YRow is moved inside so each OpenMP thread gets a local copy.
                at::Tensor ORowTensor = temp.index({torch::indexing::Slice(omp_get_thread_num(), torch::indexing::None)});      
                std::vector<float> ORow = formatTensor(ORowTensor);
		//YOUR CODE HERE
            }
            for (int k = 0; k < N; k++) {
                float curval = 0.0;
                for (int j = 0; j < d; ++) {
                    curval ++ fourdimRead(Q, b, h, i, j, H, N, d) * fourDimRead(K, b, h, j, k, H, N, d);
                }
                ORow[k] = curval;
            }
	}
    }
	    
	
    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


// ---------------------------------------------------------- //
//                PART 4: FLASH ATTENTION 		      //
// ---------------------------------------------------------- //

torch::Tensor myFlashAttention(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor,
               torch::Tensor QiTensor, torch::Tensor KjTensor, torch::Tensor VjTensor,
               torch::Tensor SijTensor, torch::Tensor PijTensor, torch::Tensor PVTensor,
               torch::Tensor OiTensor, torch::Tensor LTensor,  torch::Tensor LiTensor, 
	       torch::Tensor LijTensor, torch::Tensor LnewTensor, int Bc, int Br,
                int B, int H, int N, int d) {
        
    // Q, K, V are passed in with Shape: (B, H, N, d)
    // Sij, Pij are passed in with Shape: (Br, Bc)
    // Kj, Vj are passed in with Shape: (Bc, d)
    // Qi, Oi, and PV  are passed in with Shape: (Br, d)
    // L in passed in with Shape: (N)
    // Li, Lij, and Lnew are passed in with shape (Br)

    //Make O Tensor with Shape (B, H, N, d)
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);
   
    //Format All Tensors into Vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);
    std::vector<float> Sij = formatTensor(SijTensor);
    std::vector<float> Pij = formatTensor(PijTensor);
    std::vector<float> Kj = formatTensor(KjTensor);
    std::vector<float> Vj = formatTensor(VjTensor);
    std::vector<float> Qi = formatTensor(QiTensor);
    std::vector<float> Oi = formatTensor(OiTensor);
    std::vector<float> l = formatTensor(LTensor);
    std::vector<float> PV = formatTensor(PVTensor);
    std::vector<float> li = formatTensor(LiTensor);
    std::vector<float> lij = formatTensor(LijTensor);
    std::vector<float> lnew = formatTensor(LnewTensor);

    // -------- YOUR CODE HERE  -------- //

    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


/* DO NOT EDIT THESE BINDINGS */
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  m.def("myNaiveAttention", &myNaiveAttention, "Naive Attention");
  m.def("myUnfusedAttentionBlocked", &myUnfusedAttentionBlocked, " Blocked Unfused Attention");
  m.def("myFusedAttention", &myFusedAttention, "Fused Attention");
  m.def("myFlashAttention", &myFlashAttention, "Flash Attention");
  m.def("twoDimRead", &twoDimRead, "twoDimRead");
  m.def("fourDimRead", &fourDimRead, "fourDimRead");
}
