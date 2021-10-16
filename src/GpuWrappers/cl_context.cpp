/*****************************************************************************
 MIT License
 
 Copyright(c) 2021 Alexander Veselov
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this softwareand associated documentation files(the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions :
 
 The above copyright noticeand this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 *****************************************************************************/

#include "cl_context.hpp"
#include "utils/cl_exception.hpp"
#include "render.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

CLContext::CLContext(const cl::Platform& platform, void* display_context, void* gl_context)
    : platform_(platform)
{
    std::cout << "Platform: " << platform.getInfo<CL_PLATFORM_NAME>() << std::endl;

    cl_context_properties props[] =
    {
        // OpenCL platform
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
        // OpenGL context
        CL_GL_CONTEXT_KHR, (cl_context_properties)gl_context,
        // HDC used to create the OpenGL context
        CL_WGL_HDC_KHR, (cl_context_properties)display_context,
        0
    };

    platform.getDevices(CL_DEVICE_TYPE_ALL, &devices_);
    if (devices_.empty())
    {
        throw std::runtime_error("No devices found!");
    }

    for (unsigned int i = 0; i < devices_.size(); ++i)
    {
        std::cout << "Device: " << std::endl;
        std::cout << devices_[i].getInfo<CL_DEVICE_NAME>() << std::endl;
        std::cout << "Status: " << (devices_[i].getInfo<CL_DEVICE_AVAILABLE>() ? "Available" : "Not available") << std::endl;
        std::cout << "Max compute units: " << devices_[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
        std::cout << "Max workgroup size: " << devices_[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << std::endl;
        std::cout << "Max constant buffer size: " << devices_[i].getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() << std::endl;
        //std::cout << "Extensions: " << platform_devices[i].getInfo<CL_DEVICE_EXTENSIONS>() << std::endl;
        std::cout << "Image support: " << (devices_[i].getInfo<CL_DEVICE_IMAGE_SUPPORT>() ? "Yes" : "No") << std::endl;
        std::cout << "2D Image max width: " << devices_[i].getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>() << std::endl;
        std::cout << "2D Image max height: " << devices_[i].getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>() << std::endl;
        std::cout << "Preferred vector width: " << devices_[i].getInfo<CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT>() << std::endl;
    }

    cl_int status;
    context_ = cl::Context(devices_, props, 0, 0, &status);
    ThrowIfFailed(status, "Failed to create OpenCL context");

    queue_ = cl::CommandQueue(context_, devices_[0], 0, &status);
    ThrowIfFailed(status, "Failed to create queue");

    std::cout << "Successfully created context " << std::endl;

}

void CLContext::WriteBuffer(const cl::Buffer& buffer, const void* data, size_t size) const
{
    cl_int status = queue_.enqueueWriteBuffer(buffer, true, 0, size, data);
    ThrowIfFailed(status, "Failed to write buffer");
}

void CLContext::ReadBuffer(const cl::Buffer& buffer, void* data, size_t size) const
{
    cl_int status = queue_.enqueueReadBuffer(buffer, false, 0, size, data);
    ThrowIfFailed(status, "Failed to read buffer");
}

void CLContext::ExecuteKernel(CLKernel const& kernel, std::size_t work_size) const
{
    cl_int status = queue_.enqueueNDRangeKernel(kernel.GetKernel(), cl::NullRange, cl::NDRange(work_size), cl::NullRange, 0);
    ThrowIfFailed(status, "Failed to enqueue kernel");
}

void CLContext::AcquireGLObject(cl_mem mem)
{
    cl_int status = clEnqueueAcquireGLObjects(queue_(), 1, &mem, 0, 0, NULL);
    ThrowIfFailed(status, "Failed to acquire GL object");
}

void CLContext::ReleaseGLObject(cl_mem mem)
{
    cl_int status = clEnqueueReleaseGLObjects(queue_(), 1, &mem, 0, 0, NULL);
    ThrowIfFailed(status, "Failed to release GL object");
}

CLKernel::CLKernel(char const* filename, CLContext const& cl_context, char const* kernel_name,
    std::vector<std::string> const& definitions)
{
    std::ifstream input_file(filename);

    if (!input_file)
    {
        throw std::runtime_error("Failed to load kernel file!");
    }
    
    // std::istreambuf_iterator s should be wrapped by brackets (wat?)
    std::string source((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));

    cl_int status;
    cl::Program program(cl_context.GetContext(), source, false, &status);
    ThrowIfFailed(status, ("Failed to create program from file " + std::string(filename)).c_str());

    std::string build_options = "-I src/kernels/";

    for (auto const& definition : definitions)
    {
        build_options += " -D " + definition;
    }

    status = program.build(build_options.c_str());
    ThrowIfFailed(status, ("Error building " + std::string(filename) + ": " + program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl_context.GetDevices()[0])).c_str());

    m_Kernel = cl::Kernel(program, kernel_name, &status);
    ThrowIfFailed(status, ("Failed to create kernel " + std::string(kernel_name)).c_str());
}

void CLKernel::SetArgument(std::uint32_t argIndex, void const* data, size_t size)
{
    cl_int status = m_Kernel.setArg(argIndex, size, data);
    ThrowIfFailed(status, ("Failed to set kernel argument #" + std::to_string(argIndex)).c_str());
}
