#include "resource.hpp"
#include "resource_loader.hpp"
#include "wrapped_array.hpp"

#include <getopt.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#define RESOURCE_START(name) __start_pirate_res_##name
#define RESOURCE_STOP(name) __stop_pirate_res_##name
#define KNOWN_RESOURCE(name) wrap_array(RESOURCE_START(name), RESOURCE_STOP(name))
#define DECLARE_KNOWN_RESOURCE(name) \
  extern pirate_resource RESOURCE_START(name)[], RESOURCE_STOP (name)[];

DECLARE_KNOWN_RESOURCE(string)
DECLARE_KNOWN_RESOURCE(bool)
DECLARE_KNOWN_RESOURCE(int)
DECLARE_KNOWN_RESOURCE(milliseconds)

enum class HasArg { None, Required, Optional };

struct Handler {
    char const* name;
    HasArg hasArg;
    std::string documentation;
    std::function<bool(char const*)> callback;
    bool required;

    Handler(
        char const* name, HasArg hasArg,
        std::string documentation,
        bool required,
        std::function<bool(char const*)> callback
    ) : name(name), hasArg(hasArg), documentation(documentation), callback(callback),
        required(required) {}
};

namespace {
    int getopt_hasarg(HasArg x) {
        switch (x) {
            case HasArg::None:     return no_argument;
            case HasArg::Required: return required_argument;
            case HasArg::Optional: return optional_argument;
            default:               return -1;
        }
    }

    int getopt_loader(int &argc, char **&argv, std::vector<Handler> const& handlers) {

	std::vector<bool> seen(handlers.size());
        std::vector<option> longopts;
        for (auto const& h : handlers) {
            longopts.push_back({h.name, getopt_hasarg(h.hasArg)});
        }
        longopts.push_back({});

        int ch;
        int optlongindex;
        while (-1 != (ch = getopt_long(argc, argv, "", longopts.data(), &optlongindex))) {
            switch (ch) {
                default: abort();
                case '?': return -1;
                case 0:
                    if (handlers[optlongindex].callback(optarg)) { return -1; }
                    seen[optlongindex] = true;
            }
        }

        bool missing = false;
        for (size_t i = 0; i < handlers.size(); i++) {
            if (handlers[i].required && !seen[i]) {
                missing = true;
                std::cerr << "Missing required flag --" << handlers[i].name << std::endl;
            }
        }

        if (missing) { return -1; }

        // remove flag arguments from program argument array
        argc -= optind;
        argv += optind;

        return 0;
    }
}

/**
 * This loads resources by parsing command line arguments.
 *
 * It returns 0 if successful and -1 if unsuccessful.
 * If errors occur, they are reported by printing to stderr.
 */
int load_resources(int &argc, char **&argv) {

    std::vector<Handler> handlers;

    // Install string resource handlers
    for (auto const& res : KNOWN_RESOURCE(string)) {

        std::string doc = "";
        bool required = false;
        for (auto const& p : wrap_array(res.params, res.params_len)) {
            if (0 == strcmp("doc", p.key)) {
                doc = p.value;
            } else if (0 == strcmp("required", p.key)) {
                required = true;
            }
        }

        auto obj = static_cast<std::string*>(res.object);

        handlers.emplace_back(
            res.name,
            HasArg::Required,
            doc,
            required,
            [obj](const char *arg){ *obj = arg; return false; }
        );
    }

    // Install bool resource handlers
    for (auto const& res : KNOWN_RESOURCE(bool)) {

        std::string doc = "";
        bool required = false;
        for (auto const& p : wrap_array(res.params, res.params_len)) {
            if (0 == strcmp("doc", p.key)) {
                doc = p.value;
            } else if (0 == strcmp("required", p.key)) {
                required = true;
            }
        }

        auto obj = static_cast<bool*>(res.object);

        handlers.emplace_back(
            res.name,
            HasArg::Optional,
            doc,
            required,
            [obj](char const* arg){
                if (nullptr == arg || 0 == strcasecmp(arg, "yes")) {
                    *obj = true;
                } else if (0 == strcasecmp(arg, "no")) {
                    *obj = false;
                } else {
                    return true;
                }
                return false;
            }
        );
    }

    // Install int resource handlers
    for (auto const& res : KNOWN_RESOURCE(int)) {

        std::string doc = "";
        bool required = false;
        auto base = 0;
        for (auto const& p : wrap_array(res.params, res.params_len)) {
            if (0 == strcmp("base", p.key)) {
                base = atoi(p.value);
                if (base < 2 || base > 36) abort(); // bad base parameter
            } else if (0 == strcmp("doc", p.key)) {
                doc = p.value;
            } else if (0 == strcmp("required", p.key)) {
                required = true;
            }
        }

        auto obj = static_cast<int*>(res.object);

        handlers.emplace_back(
            res.name,
            HasArg::Required,
            doc,
            required,
            [obj, base](char const* arg){
                try {
                    *obj = std::stoi(arg, /*pos*/0, base);
                } catch (std::invalid_argument const& e) {
                    return true;
                } catch (std::out_of_range const& e) {
                    return true;
                }
                return false;
            }
        );
    }

    // Install int resource handlers
    for (auto const& res : KNOWN_RESOURCE(milliseconds)) {
        std::string doc = "";
        bool required = false;
        for (auto const& p : wrap_array(res.params, res.params_len)) {
            if (0 == strcmp("doc", p.key)) {
                doc = p.value;
            } else if (0 == strcmp("required", p.key)) {
                required = true;
            }
        }

        auto obj = static_cast<std::chrono::milliseconds*>(res.object);

        handlers.emplace_back(
            res.name,
            HasArg::Required,
            doc,
            required,
            [obj](char const* arg){
                try {
                    auto value = std::stoll(arg);
                    *obj = std::chrono::milliseconds(value);
                } catch (std::invalid_argument const& e) {
                    return true;
                } catch (std::out_of_range const& e) {
                    return true;
                }
                return false;
            }
        );
    }

    // install help handlers
    handlers.emplace_back(
        "help",
        HasArg::None,
        "Show available settings",
        /*required*/false,
        [&](char const* arg) {
            std::cerr << "Available command-line options:" << std::endl;
            for (auto const& h : handlers) {
                std::string arg;
                arg += "--";
                arg += h.name;

                switch (h.hasArg) {
                    case HasArg::None: break;
                    case HasArg::Required: arg += "=ARG"; break;
                    case HasArg::Optional: arg += "[=ARG]"; break;
                }

                std::cerr << "  " << std::setw(30) << std::left << arg << " "
                          << h.documentation << std::endl;
            }
            return true;
        }
    );

    return getopt_loader(argc, argv, handlers);
}
