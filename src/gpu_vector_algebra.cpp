#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

#include <RcppArmadillo.h>

#include "opencl_utils.h"

using namespace cl;
using namespace Rcpp;


//[[Rcpp::export]]
IntegerVector cpp_gpu_two_vec(IntegerVector A_, IntegerVector B_, 
    IntegerVector C_, SEXP sourceCode_, SEXP kernel_function_)
{
    // declarations
    cl_int err;
    std::string sourceCode = as<std::string>(sourceCode_);
    
    std::string kernel_string = as<std::string>(kernel_function_);
    const char* kernel_function = (const char*)kernel_string.c_str();

    // Convert input vectors to cl equivalent
//    const std::vector<int> A = as<std::vector<int> >(A_);
//    const std::vector<int> B = as<std::vector<int> >(B_);
//    std::vector<int> C = as<std::vector<int> >(C_);
    const arma::ivec A = as<arma::ivec >(A_);
    const arma::ivec B = as<arma::ivec >(B_);
    arma::ivec C = as<arma::ivec >(C_);
    
    const int LIST_SIZE = A.size();
    
    try {
        // Get available platforms
        std::vector<Platform> platforms;
        Platform::get(&platforms);
        
        checkErr(platforms.size()!=0 ? CL_SUCCESS : -1, 
        "No platforms found. Check OpenCL installation!\n");

        // Select the default platform and create a context using this platform and the GPU
        cl_context_properties cps[3] = {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)(platforms[0])(),
            0
        };

        Context context( CL_DEVICE_TYPE_GPU, cps, NULL, NULL, &err);
        checkErr(err, "Conext::Context()"); 

        // Get a list of devices on this platform
        std::vector<Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
        
        checkErr(devices.size() > 0 ? CL_SUCCESS : -1, "No devices found!\n");

        // check how many devices found
        //std::cout << devices.size() << std::endl;

        // Create a command queue and use the first device
        CommandQueue queue = CommandQueue(context, devices[0], 0, &err);
        checkErr(err, "CommandQueue::CommandQueue()");

        // Read source file - passed in by R wrapper function            
        Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()+1));

        // Make program of the source code in the context
        Program program = Program(context, source);
        
        // Build program for these specific devices
        err = program.build(devices, "");
        checkErr(err, "Program::build()");
        
        //std::cout << "Program built" << std::endl;

        // Make kernel
//        Kernel kernel(program, "vector_add", &err);
        Kernel kernel(program, kernel_function, &err);
        checkErr(err, "Kernel::Kernel()");
        
//        std::cout << "Kernel made" << std::endl;

        // Create memory buffers
        Buffer bufferA = Buffer(context, CL_MEM_READ_ONLY, LIST_SIZE * sizeof(int), NULL, &err);
        checkErr(err, "Buffer::BufferA()");
        Buffer bufferB = Buffer(context, CL_MEM_READ_ONLY, LIST_SIZE * sizeof(int), NULL, &err);
        checkErr(err, "Buffer::BufferB()");
        Buffer bufferC = Buffer(context, CL_MEM_WRITE_ONLY, LIST_SIZE * sizeof(int), NULL, &err);
        checkErr(err, "Buffer::BufferC()");

//        std::cout << "memory mapped" << std::endl;

        // Copy lists A and B to the memory buffers
        queue.enqueueWriteBuffer(bufferA, CL_TRUE, 0, LIST_SIZE * sizeof(int), &A[0]);
        queue.enqueueWriteBuffer(bufferB, CL_TRUE, 0, LIST_SIZE * sizeof(int), &B[0]);

        // Set arguments to kernel
        err = kernel.setArg(0, bufferA);
        checkErr(err, "Kernel::setArgA()");
        err = kernel.setArg(1, bufferB);
        checkErr(err, "Kernel::setArgB()");
        err = kernel.setArg(2, bufferC);
        checkErr(err, "Kernel::setArgC()");
        
        // Run the kernel on specific ND range
        NDRange global(LIST_SIZE);
        NDRange local(1);
        err = queue.enqueueNDRangeKernel(kernel, NullRange, global, local);
        checkErr(err, "CommandQueue::enqueueNDRangeKernel()");
        
        // Read buffer C into a local list        
        err = queue.enqueueReadBuffer(bufferC, CL_TRUE, 0, LIST_SIZE * sizeof(cl_int), &C[0]);
        checkErr(err, "CommandQueue::enqueueReadBuffer()");
        
        return wrap(C);

    } catch(Error error) {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
    }
    return EXIT_FAILURE;
}