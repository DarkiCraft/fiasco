#include "fiasco/internal/types/json.hpp"

#include <nlohmann/json.hpp>

namespace fiasco::detail {

struct json_type::impl {
    nlohmann::json m_json;
};

json_type::json_type()
    : m_data(std::make_shared<impl>()) {}

json_type::json_type(std::nullptr_t)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = nullptr;
}

json_type::json_type(bool b)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = b;
}

json_type::json_type(int i)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = i;
}

json_type::json_type(long l)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = l;
}

json_type::json_type(long long ll)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = ll;
}

json_type::json_type(unsigned long ul)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = ul;
}

json_type::json_type(unsigned long long ull)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = ull;
}

json_type::json_type(float f)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = f;
}

json_type::json_type(double d)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = d;
}

json_type::json_type(const std::string& s)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = s;
}

json_type::json_type(const char* s)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = s;
}

json_type::json_type(const json_type& other)
    : m_data(other.m_data),
      m_path(other.m_path) {}

json_type::json_type(json_type&& other) noexcept
    : m_data(std::move(other.m_data)),
      m_path(std::move(other.m_path)) {
    other.m_data = std::make_shared<impl>();
}

json_type::json_type(std::initializer_list<json_entry> entries)
    : m_data(std::make_shared<impl>()) {
    auto& obj = m_data->m_json;
    for (const auto& e : entries) {
        obj[e.key] = *static_cast<const nlohmann::json*>(e.value.data());
    }
}

json_type json_type::object() {
    json_type j;
    j.m_data->m_json = nlohmann::json::object();
    return j;
}

json_type json_type::array(std::initializer_list<json_type> values) {
    json_type j;
    j.m_data->m_json = nlohmann::json::array();
    auto& arr = j.m_data->m_json;
    for (const auto& v : values) {
        arr.push_back(*static_cast<const nlohmann::json*>(v.data()));
    }
    return j;
}

json_type& json_type::operator=(const json_type& other) {
    if (this != &other) {
        auto& self = *static_cast<nlohmann::json*>(data());
        const auto& src = *static_cast<const nlohmann::json*>(other.data());
        self = src;
    }
    return *this;
}

json_type& json_type::operator=(json_type&& other) noexcept {
    if (this != &other) {
        auto& self = *static_cast<nlohmann::json*>(data());
        auto& src = *static_cast<nlohmann::json*>(other.data());
        self = std::move(src);
    }
    return *this;
}

json_type json_type::operator[](const std::string& key) {
    json_type result;
    result.m_data = m_data;
    result.m_path = m_path;
    result.m_path.push_back(key);
    return result;
}

json_type json_type::operator[](std::size_t index) {
    json_type result;
    result.m_data = m_data;
    result.m_path = m_path;
    result.m_path.push_back(index);
    return result;
}

json_type json_type::operator[](const std::string& key) const {
    const auto& self = *static_cast<const nlohmann::json*>(data());
    json_type result;
    result.m_data = std::make_shared<impl>();
    result.m_data->m_json = self[key];
    return result;
}

json_type json_type::operator[](std::size_t index) const {
    const auto& self = *static_cast<const nlohmann::json*>(data());
    json_type result;
    result.m_data = std::make_shared<impl>();
    result.m_data->m_json = self[index];
    return result;
}

bool json_type::is_null() const noexcept {
    return static_cast<const nlohmann::json*>(data())->is_null();
}

bool json_type::is_boolean() const noexcept {
    return static_cast<const nlohmann::json*>(data())->is_boolean();
}

bool json_type::is_number() const noexcept {
    return static_cast<const nlohmann::json*>(data())->is_number();
}

bool json_type::is_string() const noexcept {
    return static_cast<const nlohmann::json*>(data())->is_string();
}

bool json_type::is_object() const noexcept {
    return static_cast<const nlohmann::json*>(data())->is_object();
}

bool json_type::is_array() const noexcept {
    return static_cast<const nlohmann::json*>(data())->is_array();
}

bool json_type::contains(const std::string& key) const noexcept {
    return static_cast<const nlohmann::json*>(data())->contains(key);
}

bool json_type::empty() const noexcept {
    return static_cast<const nlohmann::json*>(data())->empty();
}

std::size_t json_type::size() const noexcept {
    return static_cast<const nlohmann::json*>(data())->size();
}

json_type json_type::parse(const std::string& str) {
    try {
        json_type j;
        j.m_data->m_json = nlohmann::json::parse(str);
        return j;
    } catch (const nlohmann::json::exception& e) {
        throw json_type::exception(e.what());
    }
}

std::string json_type::dump(int indent) const {
    return static_cast<const nlohmann::json*>(data())->dump(indent);
}

void json_type::push_back(json_type value) {
    auto& arr = *static_cast<nlohmann::json*>(data());
    arr.push_back(std::move(*static_cast<nlohmann::json*>(value.data())));
}

void json_type::erase(const std::string& key) {
    auto& obj = *static_cast<nlohmann::json*>(data());
    obj.erase(key);
}

void json_type::merge_patch(const json_type& patch) {
    auto& obj = *static_cast<nlohmann::json*>(data());
    const auto& p = *static_cast<const nlohmann::json*>(patch.data());
    obj.merge_patch(p);
}

void json_type::clear() {
    static_cast<nlohmann::json*>(data())->clear();
}

bool json_type::operator==(const json_type& other) const noexcept {
    return *static_cast<const nlohmann::json*>(data()) ==
           *static_cast<const nlohmann::json*>(other.data());
}

bool json_type::operator!=(const json_type& other) const noexcept {
    return !(*this == other);
}

std::vector<std::string> json_type::object_keys() const {
    std::vector<std::string> keys;
    const auto& j = *static_cast<const nlohmann::json*>(data());
    if (!j.is_object()) {
        return keys;
    }
    for (const auto& [key, value] : j.items()) {
        keys.push_back(key);
    }
    return keys;
}

void* json_type::data() {
    nlohmann::json* current = &m_data->m_json;
    for (const auto& seg : m_path) {
        if (auto* key = std::get_if<std::string>(&seg)) {
            current = &(*current)[*key];
        } else {
            current = &(*current)[std::get<std::size_t>(seg)];
        }
    }
    return current;
}

const void* json_type::data() const {
    const nlohmann::json* current = &m_data->m_json;
    for (const auto& seg : m_path) {
        if (auto* key = std::get_if<std::string>(&seg)) {
            current = &(*current)[*key];
        } else {
            current = &(*current)[std::get<std::size_t>(seg)];
        }
    }
    return current;
}

void json_type::construct_from_val(const void* val, from_val_fn fn) {
    fn(*this, val);
}

void json_type::extract_val(void* val, to_val_fn fn) const {
    fn(*this, val);
}

void from_json(const json_type& j, std::nullptr_t&) {
    static_cast<const nlohmann::json*>(j.data())->get<std::nullptr_t>();
}

void from_json(const json_type& j, bool& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<bool>();
}

void from_json(const json_type& j, int& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<int>();
}

void from_json(const json_type& j, long& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<long>();
}

void from_json(const json_type& j, long long& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<long long>();
}

void from_json(const json_type& j, unsigned long& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<unsigned long>();
}

void from_json(const json_type& j, unsigned long long& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<unsigned long long>();
}

void from_json(const json_type& j, float& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<float>();
}

void from_json(const json_type& j, double& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<double>();
}

void from_json(const json_type& j, std::string& val) {
    val = static_cast<const nlohmann::json*>(j.data())->get<std::string>();
}

void to_json(json_type& j, std::nullptr_t) {
    *static_cast<nlohmann::json*>(j.data()) = nullptr;
}

void to_json(json_type& j, bool val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, int val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, long val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, long long val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, unsigned long val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, unsigned long long val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, float val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, double val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, const std::string& val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, const char* val) {
    *static_cast<nlohmann::json*>(j.data()) = val;
}

void to_json(json_type& j, const json_type& val) {
    *static_cast<nlohmann::json*>(j.data()) = *static_cast<const nlohmann::json*>(val.data());
}

void from_json(const json_type& j, json_type& val) {
    *static_cast<nlohmann::json*>(val.data()) = *static_cast<const nlohmann::json*>(j.data());
}

json_type::~json_type() = default;

}  // namespace fiasco::detail
