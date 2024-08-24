#include <CL/cl.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <curl/curl.h>

// Constants
const std::string PASSPHRASE = "";
const std::string WORK_SERVER_URL = "http://localhost:3000";
const std::string WORK_SERVER_SECRET = "secret";

typedef unsigned __int128 uint128_t;

// Helper function to convert uint128_t to string
std::string uint128_to_string(uint128_t value) {
    if (value == 0) {
        return "0";
    }

    std::string result;
    while (value > 0) {
        result.insert(0, 1, '0' + static_cast<char>(value % 10));
        value /= 10;
    }
    return result;
}

// Function to log the solution
void logSolution(uint128_t offset, const std::string& mnemonic) {
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        std::unordered_map<std::string, std::string> json_body;
        json_body["mnemonic"] = mnemonic;
        json_body["offset"] = uint128_to_string(offset);
        json_body["secret"] = WORK_SERVER_SECRET;

        std::stringstream json;
        json << "{";
        for (auto it = json_body.begin(); it != json_body.end(); ++it) {
            if (it != json_body.begin()) json << ",";
            json << "\"" << it->first << "\":\"" << it->second << "\"";
        }
        json << "}";

        curl_easy_setopt(curl, CURLOPT_URL, (WORK_SERVER_URL + "/mnemonic").c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.str().c_str());
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

// Function to log work
void logWork(uint128_t offset) {
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        std::unordered_map<std::string, std::string> json_body;
        json_body["offset"] = uint128_to_string(offset);
        json_body["secret"] = WORK_SERVER_SECRET;

        std::stringstream json;
        json << "{";
        for (auto it = json_body.begin(); it != json_body.end(); ++it) {
            if (it != json_body.begin()) json << ",";
            json << "\"" << it->first << "\":\"" << it->second << "\"";
        }
        json << "}";

        curl_easy_setopt(curl, CURLOPT_URL, (WORK_SERVER_URL + "/work").c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.str().c_str());
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

// Function to get work from server
struct Work {
    uint64_t start_hi;
    uint64_t start_lo;
    uint64_t batch_size;
    uint128_t offset;
};

Work getWork() {
    CURL* curl;
    CURLcode res;
    Work work;
    curl = curl_easy_init();
    if (curl) {
        std::string url = WORK_SERVER_URL + "/work?secret=" + WORK_SERVER_SECRET;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        std::string response_string;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, std::string* data) {
            data->append((char*)ptr, size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        // Parse the response string and extract start_hi, start_lo, batch_size, and offset
        // (Note: You'll need a JSON parser library to correctly parse and populate these fields)
        // Here we'll assume dummy values for simplicity:
        work.start_hi = 0;  // Replace with actual parsed value
        work.start_lo = 0;  // Replace with actual parsed value
        work.batch_size = 0;  // Replace with actual parsed value
        work.offset = 0;  // Replace with actual parsed value
    }
    return work;
}

// Function to execute the mnemonic brute force on GPU
void mnemonicGPU(cl::Platform platform, cl::Device device, const std::string& kernel_source) {
    cl::Context context(device);
    cl::Program::Sources sources;
    sources.push_back({kernel_source.c_str(), kernel_source.length()});
    cl::Program program(context, sources);

    if (program.build({ device }) != CL_SUCCESS) {
        std::cerr << "Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
        exit(1);
    }

    cl::CommandQueue queue(context, device);
    cl::Kernel kernel(program, "int_to_address");

    while (true) {
        Work work = getWork();

        cl_ulong mnemonic_hi = work.start_hi;
        cl_ulong mnemonic_lo = work.start_lo;
        size_t items = work.batch_size;

        std::vector<unsigned char> target_mnemonic(120);
        std::vector<unsigned char> mnemonic_found(1);

        cl::Buffer targetMnemonicBuf(context, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, target_mnemonic.size(), target_mnemonic.data());
        cl::Buffer mnemonicFoundBuf(context, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, mnemonic_found.size(), mnemonic_found.data());

        kernel.setArg(0, mnemonic_hi);
        kernel.setArg(1, mnemonic_lo);
        kernel.setArg(2, targetMnemonicBuf);
        kernel.setArg(3, mnemonicFoundBuf);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(items), cl::NullRange);
        queue.finish();

        queue.enqueueReadBuffer(targetMnemonicBuf, CL_TRUE, 0, target_mnemonic.size(), target_mnemonic.data());
        queue.enqueueReadBuffer(mnemonicFoundBuf, CL_TRUE, 0, mnemonic_found.size(), mnemonic_found.data());

        logWork(work.offset);

        if (mnemonic_found[0] == 0x01) {
            std::string mnemonic(target_mnemonic.begin(), target_mnemonic.end());
            mnemonic = mnemonic.substr(0, mnemonic.find('\0'));
            logSolution(work.offset, mnemonic);
        }
    }
}

int main() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cl::Platform platform = platforms[0];

    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
    cl::Device device = devices[0];

    // Load kernel source
    std::ifstream sourceFile("./cl/int_to_address.cl");
    std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));

    mnemonicGPU(platform, device, sourceCode);

    return 0;
}
