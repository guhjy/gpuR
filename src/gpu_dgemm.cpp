#define __CL_ENABLE_EXCEPTIONS

// clBLAS and OpenCL headers
#include <clBLAS.h>
// armadillo headers for handling the R input data
#include <RcppArmadillo.h>

#include <bigmemory/MatrixAccessor.hpp>

#include "arma_helpers.hpp"

using namespace Rcpp;


int HAS_DOUBLE;

#ifdef cl_khr_fp64
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
HAS_DOUBLE = 1
#elif defined(cl_amd_fp64)
HAS_DOUBLE = 1
#pragma OPENCL EXTENSION cl_amd_fp64 : enable
HAS_DOUBLE = 0
#endif

// can add more arguments for more control of sgemm call
// e.g. if transpose needed?

//[[Rcpp::export]]
SEXP cpp_gpu_dgemm(SEXP A_, SEXP B_, SEXP C_,
                    bool A_isBM, bool B_isBM, bool C_isBM)
{
    if(HAS_DOUBLE == 0){
        Rcpp::stop("GPU does not support double precision");
    }
    
    static const clblasOrder order = clblasColumnMajor;
    static const cl_float alpha = 1;
    static const clblasTranspose transA = clblasNoTrans;

    // matrix dimensions
//    int M = A_.nrow();
//    int N = B_.ncol();
//    int K = A_.ncol();
    
//    int M = A_.ncol();
//    int N = B_.nrow();
//    int K = A_.nrow();
    
    // use armadillo to mat
    
//    Rcpp::XPtr<BigMatrix> xpA(A_);
//    Rcpp::XPtr<BigMatrix> xpB(B_);
//    Rcpp::XPtr<BigMatrix> xpC(C_);
//        
//    static const arma::mat Am = arma::mat( (double*) xpA->matrix(),
//                              xpA->nrow(),
//                              xpA->ncol(),
//                              false);
//                              
//    static const arma::mat Bm = arma::mat( (double*) xpB->matrix(),
//                              xpB->nrow(),
//                              xpB->ncol(),
//                              false);
//                              
//    static arma::mat Cm = arma::mat( (double*) xpC->matrix(),
//                              xpC->nrow(),
//                              xpC->ncol(),
//                              false);
                              
    static const arma::Mat<double> Am = ( A_isBM ? ConvertBMtoArma<double>(A_) : as<arma::mat>(A_) );
    static const arma::Mat<double> Bm = ( B_isBM ? ConvertBMtoArma<double>(B_) : as<arma::mat>(B_) );
    static arma::Mat<double> Cm = ( C_isBM ? ConvertBMtoArma<double>(C_) : as<arma::mat>(C_) );
        
    
    int M = Am.n_cols;
    int N = Bm.n_rows;
    int K = Am.n_rows;
    
//    Am.print("A Matrix");
//    Bm.print("B Matrix");
    
//    static const arma::mat Am(A_.begin(), 
//                                            A_.nrow(),
//                                            A_.ncol(),
//                                            false);
//    static const arma::mat Bm(B_.begin(), 
//                                            B_.nrow(),
//                                            B_.ncol(),
//                                            false);
//    static arma::mat Cm(C_.begin(), 
//                                        C_.nrow(),
//                                        C_.ncol(),
//                                        false);

//    std::cout << "read matrices" << std::endl;

    static const std::size_t lda = K;        /* i.e. lda = K */
    static const clblasTranspose transB = clblasNoTrans;

    static const std::size_t ldb = N;        /* i.e. ldb = N */
    static const cl_float beta = 0;
    
    static const std::size_t ldc = N;        /* i.e. ldc = N */

    // declare OpenCL objects
    cl_int err;
    cl_platform_id platform = 0;
    cl_device_id device = 0;
    cl_context_properties props[3] = { CL_CONTEXT_PLATFORM, 0, 0 };
    cl_context ctx = 0;
    cl_command_queue queue = 0;
    cl_mem bufA, bufB, bufC;
    cl_event event = NULL;
    
    
//    std::cout << "declared all vars" << std::endl;
    
    /* Setup OpenCL environment. */
    err = clGetPlatformIDs(1, &platform, NULL);
    if (err != CL_SUCCESS) {
        stop("clGetPlatformIDs() failed with " + err);
    }
    
//    std::cout << "found platform" << std::endl;
    
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) {
        stop("clGetDeviceIDs() failed with " + err);
    }
    
//    std::cout << "found device" << std::endl;
    
    props[1] = (cl_context_properties)platform;
    ctx = clCreateContext(props, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        std::cout << err << std::endl;
        std::cout << "unable to create context" << std::endl;
        clReleaseContext(ctx);
        stop("clCreateContext() failed");
    }
    
//    std::cout << "created context" << std::endl;
    
    queue = clCreateCommandQueue(ctx, device, 0, &err);
    if (err != CL_SUCCESS) {
        clReleaseContext(ctx);
        stop("clCreateCommandQueue() failed");
    }
    
    
//    std::cout << "opencl setup" << std::endl;
    
    /* Setup clblas. */
    err = clblasSetup();
    if (err != CL_SUCCESS) {
        clReleaseCommandQueue(queue);
        clReleaseContext(ctx);
        stop("clblasSetup() failed with " + err);
    }
    
    
//    std::cout << "clblas setup" << std::endl;
    
    /* Prepare OpenCL memory objects and place matrices inside them. */
    bufA = clCreateBuffer(ctx, CL_MEM_READ_ONLY, M * K * sizeof(Am[0]),
                          NULL, &err);
    bufB = clCreateBuffer(ctx, CL_MEM_READ_ONLY, K * N * sizeof(Bm[0]),
                          NULL, &err);
    bufC = clCreateBuffer(ctx, CL_MEM_READ_WRITE, M * N * sizeof(Cm[0]),
                          NULL, &err);
                          
    err = clEnqueueWriteBuffer(queue, bufA, CL_TRUE, 0,
        M * K * sizeof(Am[0]), &Am[0], 0, NULL, NULL);
    err = clEnqueueWriteBuffer(queue, bufB, CL_TRUE, 0,
        K * N * sizeof(Bm[0]), &Bm[0], 0, NULL, NULL);
    err = clEnqueueWriteBuffer(queue, bufC, CL_TRUE, 0,
        M * N * sizeof(Cm[0]), &Cm[0], 0, NULL, NULL);
        
    
//    std::cout << "wrote matrices" << std::endl;
    
    /* Call clblas extended function. Perform gemm */
    err = clblasDgemm(order, transA, transB, M, N, K,
                         alpha, bufA, 0, lda,
                         bufB, 0, ldb, beta,
                         bufC, 0, ldc,
                         1, &queue, 0, NULL, &event);
    if (err != CL_SUCCESS) {
        std::cout << err << std::endl;
        stop("clblasDgemmEx() failed");
    }
    else {
        
//        std::cout << "finished sgemm" << std::endl;
        
        /* Wait for calculations to be finished. */
        err = clWaitForEvents(1, &event);                                  
        err = clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0,
                                  M * N * sizeof(Cm[0]),
                                  &Cm[0], 0, NULL, NULL);
    }
    
    
//    std::cout << "read output" << std::endl;
    
    /* Release OpenCL memory objects. */
    clReleaseMemObject(bufC);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufA);
    /* Finalize work with clblas. */
    clblasTeardown();
    /* Release OpenCL working objects. */
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);
    
    
//    std::cout << "released objects" << std::endl;
    
    // For some reason clBLAS always returns in Row Major format
    // so must be transposed at end here
    
    // use inplace to save memory space
    // additional option to specify 'lowmem' method but much slower
//    arma::inplace_trans(Cm);

//    Cm.print("final matrix");
//    return wrap(Cm);
    if(C_isBM){
        return C_;
    }else{
        return wrap(Cm);
    }
}