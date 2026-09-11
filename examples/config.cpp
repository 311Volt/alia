/* Configuration-file loading, typed access, iteration, and mutation.
 * Not implementable yet: alia has no configuration subsystem. The program
 * below is written against a hypothetical API; its shape is intentionally
 * kept here so that the proposed interface can be reviewed. */
#if 0
#include "alia/config/config.hpp"

#include <format>
#include <iostream>

int main() {
    // HYPOTHETICAL alia API: load an INI file into a typed configuration tree.
    alia::config cfg = alia::load_config("./resources/samplecfg.ini");

    for (auto &section : cfg.sections()) {
        std::cout << std::format("processing section {}\n", section.name());
        for (auto &[key, value] : section)
            std::cout << std::format("{} -> {}\n", key.path(), std::string(value));
    }

    auto width = cfg.get<int>("Display.Width");
    auto height = cfg.get<int>("Display.Height");
    auto refresh_rate = cfg.get<float>("Display.RefreshRate");
    if (!width || !height || !refresh_rate)
        return 1;

    std::cout << std::format(
        "old mode: {} x {}, {:.2f} Hz\n", *width, *height, *refresh_rate);

    cfg.set("Display.Width", 3840);
    cfg.set("Display.Height", 2160);
    cfg.set("Display.RefreshRate", 144.0f);

    std::cout << std::format(
        "new mode: {} x {}, {:.2f} Hz\n",
        *cfg.get<int>("Display.Width"),
        *cfg.get<int>("Display.Height"),
        *cfg.get<float>("Display.RefreshRate"));
}
#endif

#include <iostream>

int main() {
    std::cout << "config example: requires an alia configuration subsystem, which does not exist yet. See the #if 0 block in examples/config.cpp\n";
    return 0;
}
