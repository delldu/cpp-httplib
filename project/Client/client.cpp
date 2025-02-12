#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <sys/wait.h>

#include "../include/httplib.h"
#include "../include/json.hpp"

namespace fs = std::filesystem;
using JSON = nlohmann::json;

#define REDOS_VERSION "V1.0.0"
#define REDOS_HOST "127.0.0.1"
#define REDOS_PORT 1216
#define REDOS_ROOTDIR "/tmp/redos6"
#define CheckPoint(fmt, arg...) printf("# CheckPoint: %d(%s): " fmt "\n", (int)__LINE__, __FUNCTION__, ##arg)

int file_exists(char *filename) {
    httplib::Client cli(REDOS_HOST, REDOS_PORT);

    auto res = cli.Head("/Files/" + std::string(filename));

    if (res) {
        std::cout << "Status code: " << res->status << std::endl;
        std::cout << "Response body: " << res->body << std::endl;
    } else {
        std::cout << "Request failed." << std::endl;
    }

    return 0;
}


int upload_file(char *filename) {
    httplib::Client cli(REDOS_HOST, REDOS_PORT);

    auto res = cli.Post("/Files/" + std::string(filename));

    if (res) {
        std::cout << "Status code: " << res->status << std::endl;
        std::cout << "Response body: " << res->body << std::endl;
    } else {
        std::cout << "Request failed." << std::endl;
    }
    
    return 0;
}

int download_file(char *filename) {
    httplib::Client cli(REDOS_HOST, REDOS_PORT);

    std::string filePath(filename);
    // std::ofstream ofs{filePath};


    // auto res = cli.Get("/Files/" + std::string(filename),
    //     [&](const httplib::Response& res) {
    //         // m_total = std::stoul(response.get_header_value("Content-Length"));
    //         return true;
    //     },
    //     [&](const char *data, size_t dataLen) {
    //        // write downloaded data to file
    //        std::cout << std::string(data, dataLen);
    //        // ofs.write(data, dataLen);
    //        return true;
    //     }
    // );
    // ofs.close();
    auto res = cli.Get("/Files/" + std::string(filename));

    if (res) {
        std::cout << "Status code: " << res->status << std::endl;
        std::cout << "Response body: " << res->body << std::endl;
    } else {
        std::cout << "Request failed." << std::endl;
    }
    
    return 0;
}


int delete_file(char *filename) {
    httplib::Client cli(REDOS_HOST, REDOS_PORT);

    auto res = cli.Delete("/Files/" + std::string(filename));

    if (res) {
        std::cout << "Status code: " << res->status << std::endl;
        std::cout << "Response body: " << res->body << std::endl;
    } else {
        std::cout << "Request failed." << std::endl;
    }
    
    return 0;
}

void help(char* cmd)
{
    printf("Redos Server %s\n", REDOS_VERSION);
    printf("Usage: %s [option]\n", cmd);
    printf("    -h, --help                  Display this help.\n");
    printf("    -e, --exist <filename>      Test file exists.\n");
    printf("    -u, --upload <filename>     Upload file.\n");
    printf("    -d, --download <filename>   Download file.\n");
    printf("    -x, --delete  <filename>    Delete file.\n");

    exit(1);
}

int main(int argc, char* argv[])
{
    int optc;
    int option_index = 0;

    struct option long_opts[] = {
        { "help", 0, 0, 'h' },
        { "exist", 1, 0, 'e' },
        { "upload", 1, 0, 'u' },
        { "download", 1, 0, 'd' },
        { "delete", 1, 0, 'x' },
        { 0, 0, 0, 0 }
    };

    while ((optc = getopt_long(argc, argv, "e: u: d: x: h", long_opts, &option_index)) != EOF) {
        switch (optc) {
        case 'e':
            return file_exists(optarg);
            break;
        case 'u':
            return upload_file(optarg);
            break;
        case 'd':
            return download_file(optarg);
            break;
        case 'x':
            return delete_file(optarg);
            break;
        case 'h': // help
        default:
            help(argv[0]);
            break;
        }
    }

    return 0;
}
