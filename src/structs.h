// #pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>


struct Item
{
    std::string itemCode;
    std::string itemReference;

    int width;
    int length;
    int depth;

    std::optional<std::string> boxGroup;
};

struct Box
{
    std::string reference;

    int width;
    int length;
    int depth;

    std::optional<double> maxWeight;
    std::optional<double> boxWeight;

    bool active;

    std::optional<int> maximumBoxes;
};


using json = nlohmann::json;

void from_json(const json& j, Item& item)
{
    j.at("ItemCode").get_to(item.itemCode);
    j.at("ItemReference").get_to(item.itemReference);
    j.at("Width").get_to(item.width);
    j.at("Length").get_to(item.length);
    j.at("Depth").get_to(item.depth);

    if (j.contains("BoxGroup"))
        j.at("BoxGroup").get_to(item.boxGroup);
}

void from_json(const json& j, Box& box)
{
    j.at("Reference").get_to(box.reference);
    j.at("Width").get_to(box.width);
    j.at("Length").get_to(box.length);
    j.at("Depth").get_to(box.depth);

    if (j.contains("MaxWeight"))
        j.at("MaxWeight").get_to(box.maxWeight);

    if (j.contains("BoxWeight"))
        j.at("BoxWeight").get_to(box.boxWeight);

    box.active = j.value("Active", true);

    if (j.contains("MaximumBoxes"))
        j.at("MaximumBoxes").get_to(box.maximumBoxes);
}


#include <fstream>
#include <iostream>

int main()
{
    std::ifstream itemsFile("items.json");
    std::ifstream boxesFile("boxes.json");

    json itemsJson;
    json boxesJson;

    itemsFile >> itemsJson;
    boxesFile >> boxesJson;

    std::vector<Item> items =
        itemsJson.get<std::vector<Item>>();

    std::vector<Box> boxes =
        boxesJson.get<std::vector<Box>>();

    // Example
    for (const auto& item : items)
    {
        std::cout << item.itemCode
                  << " - "
                  << item.itemReference
                  << '\n';
    }

    return 0;
}

void to_json(json& j, const Item& item)
{
    j = json{
        {"ItemCode", item.itemCode},
        {"ItemReference", item.itemReference},
        {"Width", item.width},
        {"Length", item.length},
        {"Depth", item.depth}
    };
    if (item.boxGroup)
        j["BoxGroup"] = *item.boxGroup;
}

void to_json(json& j, const Box& box)
{
    j = json{
        {"Reference", box.reference},
        {"Width", box.width},
        {"Length", box.length},
        {"Depth", box.depth},
        {"Active", box.active}
    };
    if (box.maxWeight)     j["MaxWeight"] = *box.maxWeight;
    if (box.boxWeight)     j["BoxWeight"] = *box.boxWeight;
    if (box.maximumBoxes)  j["MaximumBoxes"] = *box.maximumBoxes;
}
