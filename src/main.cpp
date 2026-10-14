#include "popx_app.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto exe = std::filesystem::canonical("/proc/self/exe");
        AppOptions options;
        options.config_path = (exe.parent_path().parent_path().parent_path() / "config/popx.ini").string();
        bool schema = false, validate = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (++i >= argc)
                    throw std::invalid_argument("Missing value for " + arg);
                return argv[i];
            };
            if (arg == "--help") {
                std::cout << "PopX --headless --config PATH --udp-port PORT --speed 1|2\n"
                             "     --config-schema | --validate-config PATH | --help\n";
                return 0;
            } else if (arg == "--headless")
                options.headless = true;
            else if (arg == "--config")
                options.config_path = value();
            else if (arg == "--udp-port")
                options.udp_port = std::stoul(value());
            else if (arg == "--speed")
                options.speed = std::stoul(value());
            else if (arg == "--config-schema")
                schema = true;
            else if (arg == "--validate-config") {
                validate = true;
                options.config_path = value();
            } else
                throw std::invalid_argument("Unknown option: " + arg);
        }
        if (schema) {
            std::cout << Config::schema().dump() << '\n';
            return 0;
        }
        if (validate) {
            Config config(options.config_path, true);
            std::cout << config.values().dump() << '\n';
            return 0;
        }
        if (options.udp_port == 0 || options.udp_port > 65535 || (options.speed != 1 && options.speed != 2))
            throw std::invalid_argument("Invalid UDP port or speed");
        return PopXApp(std::move(options)).run();
    } catch (const std::exception& e) {
        std::cerr << "PopX: " << e.what() << '\n';
        return 1;
    }
}
