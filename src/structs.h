#pragma once

#include <stdexcept>
#include <regex>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>


class BoxesJsonElement {
    public:
    BoxesJsonElement() = default;
    virtual ~BoxesJsonElement() = default;

    private:
    std::string reference;
    int64_t width;
    int64_t length;
    int64_t depth;
    std::optional<double> max_weight;
    std::optional<double> box_weight;
    bool active;
    std::optional<int64_t> maximum_boxes;

    public:
    const std::string & get_reference() const { return reference; }
    std::string & get_mutable_reference() { return reference; }
    void set_reference(const std::string & value) { this->reference = value; }

    const int64_t & get_width() const { return width; }
    int64_t & get_mutable_width() { return width; }
    void set_width(const int64_t & value) { this->width = value; }

    const int64_t & get_length() const { return length; }
    int64_t & get_mutable_length() { return length; }
    void set_length(const int64_t & value) { this->length = value; }

    const int64_t & get_depth() const { return depth; }
    int64_t & get_mutable_depth() { return depth; }
    void set_depth(const int64_t & value) { this->depth = value; }

    const std::optional<double> & get_max_weight() const { return max_weight; }
    std::optional<double> & get_mutable_max_weight() { return max_weight; }
    void set_max_weight(const std::optional<double> & value) { this->max_weight = value; }

    const std::optional<double> & get_box_weight() const { return box_weight; }
    std::optional<double> & get_mutable_box_weight() { return box_weight; }
    void set_box_weight(const std::optional<double> & value) { this->box_weight = value; }

    const bool & get_active() const { return active; }
    bool & get_mutable_active() { return active; }
    void set_active(const bool & value) { this->active = value; }

    const std::optional<int64_t> & get_maximum_boxes() const { return maximum_boxes; }
    std::optional<int64_t> & get_mutable_maximum_boxes() { return maximum_boxes; }
    void set_maximum_boxes(const std::optional<int64_t> & value) { this->maximum_boxes = value; }
};

namespace quicktype {
    using BoxesJson = std::vector<BoxesJsonElement>;
}

class ItemsJsonElement {
    public:
    ItemsJsonElement() = default;
    virtual ~ItemsJsonElement() = default;

    private:
    std::string item_code;
    std::string item_reference;
    int64_t width;
    int64_t length;
    int64_t depth;
    std::optional<std::string> box_group;

    public:
    const std::string & get_item_code() const { return item_code; }
    std::string & get_mutable_item_code() { return item_code; }
    void set_item_code(const std::string & value) { this->item_code = value; }

    const std::string & get_item_reference() const { return item_reference; }
    std::string & get_mutable_item_reference() { return item_reference; }
    void set_item_reference(const std::string & value) { this->item_reference = value; }

    const int64_t & get_width() const { return width; }
    int64_t & get_mutable_width() { return width; }
    void set_width(const int64_t & value) { this->width = value; }

    const int64_t & get_length() const { return length; }
    int64_t & get_mutable_length() { return length; }
    void set_length(const int64_t & value) { this->length = value; }

    const int64_t & get_depth() const { return depth; }
    int64_t & get_mutable_depth() { return depth; }
    void set_depth(const int64_t & value) { this->depth = value; }

    const std::optional<std::string> & get_box_group() const { return box_group; }
    std::optional<std::string> & get_mutable_box_group() { return box_group; }
    void set_box_group(const std::optional<std::string> & value) { this->box_group = value; }
};

namespace quicktype {
    using ItemsJson = std::vector<ItemsJsonElement>;
}
