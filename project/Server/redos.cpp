// #include <cstring>
// #include <ctime>
#include <filesystem>
#include <getopt.h>
#include <iostream>
// // #include <csignal>
#include <sys/wait.h>

#include "../include/httplib.h"
#include "../include/json.hpp"

// #include <signal.h>
// #include <string>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <unistd.h>

namespace fs = std::filesystem;
using JSON = nlohmann::json;

#define REDOS_VERSION "V1.0.0"
#define REDOS_HOST "127.0.0.1"
#define REDOS_PORT 1216
#define REDOS_ROOTDIR "/tmp/redos6"

// #define SERVER_CERT_FILE "ssl/cert.pem"
// #define SERVER_PRIVATE_KEY_FILE "ssl/server.key"

#define CheckPoint(fmt, arg...) printf("# CheckPoint: %d(%s): " fmt "\n", (int)__LINE__, __FUNCTION__, ##arg)



httplib::Server* g_server = nullptr;
int g_running_processes = 0;
std::mutex g_running_processes_mutex;
std::string RootDir = REDOS_ROOTDIR;

std::vector<std::string> get_tokens(const std::string& s, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = s.find(delimiter);

    while (end != std::string::npos) {
        std::string token = s.substr(start, end - start);
        tokens.push_back(token);

        start = end + delimiter.length();
        end = s.find(delimiter, start);
    }

    std::string last_token = s.substr(start);
    tokens.push_back(last_token);

    return tokens;
}

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << std::endl;
        std::cout << "Received signal " << signal << ". Shutting down server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
    }
    if (signal == SIGCHLD) {
        int status;
        std::cout << "Received signal " << signal << std::endl;
        std::cout << "Running processes 1:" << g_running_processes << std::endl;
        while (waitpid(-1, &status, WNOHANG) > 0) {
            std::lock_guard<std::mutex> lock(g_running_processes_mutex);
            --g_running_processes;
        }
        std::cout << "Running processes 2:" << g_running_processes << std::endl;
    }
}


void handleRootPath(const httplib::Request& /*req*/, httplib::Response& res) {
    const auto html = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
    <meta charset="UTF-8">
    <title>Redos Server</title>
    </head>
    <body>
        <h1>Redos Server: Hello !</h1>
    </body>
    </html>
    )";

    res.set_content(html, "text/html");
}

// curl -v --HEAD http://127.0.0.1:1216/Files/a.txt
void handleFileExists(const httplib::Request& req, httplib::Response& res) {
    // HEAD /Files/:filename
    CheckPoint("File Exists");

    JSON response;
    std::string filename = req.path_params.at("filename");
    fs::path filePath = fs::path(RootDir) / filename;
    // std::cerr << "filePath: " << filePath << std::endl;

    if (! fs::exists(filePath)) {
        res.status = httplib::StatusCode::NotFound_404;
        response["message"] = "File '" + filename + "' NG: File not exists";
    } else {
        res.status = httplib::StatusCode::OK_200;
        response["message"] = "File '" + filename + "' OK";
    }
    // res.body.clear(); // !!!

    std::string response_text = response.dump(4);
    std::cout << response_text << std::endl;

    res.body = response_text;
    res.set_content(response.dump(4), "application/json");
}

void handleFileDownload(const httplib::Request& req, httplib::Response& res) {
    CheckPoint("File Download");

    JSON response;

    // GET /Files/:filename
    std::string filename = req.path_params.at("filename");
    fs::path filePath = fs::path(RootDir) / filename;
    // std::cerr << "filePath: " << filePath << std::endl;

    if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
        res.status = httplib::StatusCode::NotFound_404;
        response["message"] = "Download '" + filename + "' NG: Not exists or regular";
        res.set_content(response.dump(4), "application/json");
        return;
    }

    // Start download ...
#if 1    
    res.set_file_content(filePath, "application/octet-stream");
#else
    size_t fileSize = fs::file_size(filePath);

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (! file) {
        res.status = httplib::StatusCode::InternalServerError_500;
        response["message"] = "Download '" + filename + "' NG: Open failed.";
        res.set_content(response.dump(4), "application/json");
        return;
    }

    res.set_header("Content-Length", std::to_string(fileSize));
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Content-Disposition", "attachment; filename=" + filename);
    res.set_header("Content-Transfer-Encoding", "binary");
 

    // Send content with the content provider
    bool success_ok = true;
    res.set_content_provider(
        fileSize,
        "application/octet-stream",
        [&file](size_t offset, size_t length, httplib::DataSink& sink) {
            file.seekg(offset, std::ios::beg);
            std::vector<char> buffer(length);
            file.read(buffer.data(), length);
            // gcount() -- file real read size
            sink.write(buffer.data(), static_cast<size_t>(file.gcount()));
        },
        [&file, &success_ok](bool success) {
            // file.close();
            success_ok = success;
        }
    );
    file.close();

    if (success_ok) {
        res.status = httplib::StatusCode::OK_200;
        response["message"] = "Download '" + filename + "' OK";
        res.set_content(response.dump(4), "application/json");
    } else {
        res.status = httplib::StatusCode::InternalServerError_500;
        response["message"] = "Download '" + filename + "' NG: Reading file or sending to network";
        res.set_content(response.dump(4), "application/json");
    }
#endif    
}

// curl -d @busybox http://127.0.0.1:1216/Files/e/f

// curl -d @README.md http://127.0.0.1:1216/Files/README.md
// const Request &req, Response &res, const ContentReader &content_reader
void handleFileUpload(const httplib::Request& req, httplib::Response& res, const httplib::ContentReader &content_reader) {
    // NOT Support multipart_form_data !!!

    JSON response;
    // Put|Post /Files/:filename
    std::string filename = req.path_params.at("filename");
    fs::path filePath = fs::path(RootDir) / filename;
    CheckPoint("filePath: %s", filePath.c_str());

    std::ofstream file(filePath, std::ios::binary);
    if (! file) {
        res.status = httplib::StatusCode::InternalServerError_500;
        response["message"] = "Upload '" + filename + "' NG: Creating file";
        res.set_content(response.dump(4), "application/json");

        CheckPoint("filePath: %s", filePath.c_str());
        return;
    }

    // Start saving file ...
    auto reader = [&file](const char* data, size_t data_length) {
        file.write(data, data_length);
        return file.good();
    };

    bool success = content_reader(reader);
    file.close();

    try {
        fs::permissions(filePath, fs::perms::owner_read | fs::perms::owner_write |
                                  fs::perms::group_read | fs::perms::others_read);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error setting file permissions: " << e.what() << std::endl;
    }

    if (success) {
        res.status = httplib::StatusCode::OK_200;
        response["message"] = "Upload '" + filename + "' OK";
    } else {
        res.status = httplib::StatusCode::InternalServerError_500;
        response["message"] = "Upload '" + filename + "' NG: Reading from network or saving file";
    }

    res.set_content(response.dump(4), "application/json");
}

// curl -X "DELETE" http://127.0.0.1:1216/Files/e
void handleFileDelete(const httplib::Request& req, httplib::Response& res) {
    JSON response;

    std::string filename = req.path_params.at("filename");
    fs::path filePath = fs::path(RootDir) / filename;
    std::cerr << "Delete filePath: " << filePath << std::endl;

    if (! fs::exists(filePath)) {
        res.status = httplib::StatusCode::NotFound_404;
        response["message"] = "File '" + filename + "' NG: File not exists";
        res.set_content(response.dump(4), "application/json");
        return;
    }

    fs::remove_all(filePath);
    res.status = httplib::StatusCode::OK_200;
    response["message"] = "'" + filename + "' has been removed";
    res.set_content(response.dump(4), "application/json");
}


void help(char* cmd)
{
    printf("Redos Server %s\n", REDOS_VERSION);
    printf("Usage: %s [option]\n", cmd);
    printf("    -h, --help                Display this help.\n");
    printf("    -a, --host <addr>         Set host address (%s).\n", REDOS_HOST);
    printf("    -p, --port <port>         Set host port (%d).\n", REDOS_PORT);
    printf("    -d, --rootdir <dir>       Set root dir (%s).\n", REDOS_ROOTDIR);

    exit(1);
}

int main(int argc, char* argv[])
{
    int optc;
    int option_index = 0;
    std::string RedosHost = REDOS_HOST;
    int RedosPort = REDOS_PORT;

    struct option long_opts[] = {
        { "help", 0, 0, 'h' },
        { "host", 1, 0, 'a' },
        { "port", 1, 0, 'p' },
        { "rootdir", 1, 0, 'd' },
        { 0, 0, 0, 0 }
    };

    while ((optc = getopt_long(argc, argv, "a: p: d: h", long_opts, &option_index)) != EOF) {
        switch (optc) {
        case 'a':
            RedosHost = optarg;
            break;
        case 'p':
            RedosPort = atoi(optarg);
            break;
        case 'd':
            RootDir = optarg;
            break;
        case 'h': // help
        default:
            help(argv[0]);
            break;
        }
    }


    // // Get address and port
    // {
    //     std::vector<std::string> tokens = get_tokens(RedosHost, ":");
    //     std::string Ports = "1215";

    //     if (tokens.size() == 1) {
    //         Ports = tokens[0];
    //     } else {
    //         Addr = tokens[0];
    //         Ports = tokens[1];
    //     }
    //     try {
    //         Port = std::stoi(Ports);
    //     } catch (const std::invalid_argument& e) {
    //         std::cerr << "Error invalid port: " << e.what() << std::endl;
    //     } catch (const std::out_of_range& e) {
    //         std::cerr << "Error port out of range: " << e.what() << std::endl;
    //     }
    // }

    // Create root dir
    {
        if (RootDir == ".") {
            try {
                RootDir = fs::current_path().string();
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error getting working directory: " << e.what() << std::endl;
            }
        }
        if (RootDir.back() != '/') {
            RootDir += '/';
        }
        try {
            fs::create_directories(RootDir);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    httplib::SSLServer redos_server(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE);
#else
    httplib::Server redos_server;
#endif
    g_server = &redos_server;

    // Set timeout and others ...
    {
        redos_server.set_read_timeout(120, 0); // 120 seconds and 0 milliseconds
        redos_server.set_write_timeout(120, 0);
        redos_server.set_idle_interval(0, 100000); // 100 milliseconds        
        redos_server.set_payload_max_length(1024 * 1024 * 512); // 1024 M -- 1G     
    }

    // Register signal handlers
    {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        std::signal(SIGCHLD, signalHandler);
    }

    // Register router handlers
    {
        redos_server.Get("/", handleRootPath);
        // Files
        // -----------------------------------------------------------------------------------------
        redos_server.Head("/Files/:filename", handleFileExists);
        redos_server.Get("/Files/:filename", handleFileDownload);
        // redos_server.Get("/Files/:filename", [](const httplib::Request &req, httplib::Response &res) {
        //     if (req.method == "HEAD") {
        //         handleFileExists(req, res);
        //     } else {
        //         handleFileExists(req, res);
        //     }
        // });

        redos_server.Put("/Files/:filename", handleFileUpload); // Put == Post
        redos_server.Post("/Files/:filename", handleFileUpload);        
        redos_server.Delete("/Files/:filename", handleFileDelete);

        // redos_server.Get("/Files/.*", [](const httplib::Request& req, httplib::Response& res) {
        //     CheckPoint("--------- [%s]", req.path.c_str());
        //     res.set_content("You requested 1: " + req.path, "text/plain");
        // });
        // redos_server.Get(".*", [](const httplib::Request& req, httplib::Response& res) {
        //     CheckPoint("--------- [%s]", req.path.c_str());
        //     res.set_content("You requested 2: " + req.path, "text/plain");
        // });

    }

    // Set mount point ...
    if (! redos_server.set_mount_point("/", RootDir)) {
        std::cerr << "Error root dir " << RootDir << " doesn't exist ..." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Start redos server on [" << RedosHost << ":" << RedosPort << "], root dir is [" << RootDir << "] ..." << std::endl;
    if (! redos_server.listen(RedosHost, RedosPort)) {
        std::cerr << "Error starting server: " << std::strerror(errno) << std::endl;
    }

    g_server = nullptr;


    return 0;
}
