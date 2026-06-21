#pragma once

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace fiasco::detail {

struct json_entry;

class json_type {
  public:
    // -- Exception ------------------------------------------------------------
    struct exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // -- Construction ---------------------------------------------------------
    json_type();
    json_type(std::nullptr_t);
    json_type(bool);
    json_type(int);
    json_type(long);
    json_type(long long);
    json_type(unsigned long);
    json_type(unsigned long long);
    json_type(float);
    json_type(double);
    json_type(const std::string&);
    json_type(const char*);
    json_type(const json_type&);
    json_type(json_type&&) noexcept;

    // Object from key-value entries  e.g. json_type{{"key", val}, {"k2", v2}}
    json_type(std::initializer_list<json_entry>);
    // Array from values (defined in .cpp where json_entry is complete)
    static json_type array(std::initializer_list<json_type> values);
    static json_type object();

    // From user model / container / optional / etc. via ADL to_json(json_type&, const T&)
    // The template to_json/t_from_json families (Sequence, Map, Optional, TupleLike)
    // below ensure a direct ADL-visible overload exists for every common type, so no
    // silent fallback to the implicit-conversion path can occur.
    template <typename T>
    json_type(const T& val)
        : json_type() {
        construct_from_val(
            &val, [](json_type& j, const void* v) { to_json(j, *static_cast<const T*>(v)); });
    }

    // -- Assignment -----------------------------------------------------------
    json_type& operator=(const json_type&);
    json_type& operator=(json_type&&) noexcept;

    // -- Element access (returns sub-object view via shared data + path) ------
    json_type operator[](const std::string& key);
    json_type operator[](std::size_t index);
    json_type operator[](const std::string& key) const;
    json_type operator[](std::size_t index) const;

    // -- Type introspection ---------------------------------------------------
    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_boolean() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;

    // -- Query ----------------------------------------------------------------
    [[nodiscard]] bool contains(const std::string& key) const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    // -- Value extraction with type conversion / default ----------------------
    template <typename T>
    [[nodiscard]] T get() const {
        T val;
        extract_val(&val, [](const json_type& j, void* v) { from_json(j, *static_cast<T*>(v)); });
        return val;
    }

    template <typename T>
    [[nodiscard]] T value(const std::string& key, const T& default_val) const {
        if (contains(key)) {
            return (*this)[key].get<T>();
        }
        return default_val;
    }

    // -- Serialization --------------------------------------------------------
    static json_type parse(const std::string& str);
    [[nodiscard]] std::string dump(int indent = -1) const;

    // -- Mutation -------------------------------------------------------------
    void push_back(json_type value);
    void erase(const std::string& key);
    void merge_patch(const json_type& patch);
    void clear();

    // -- Object iteration -----------------------------------------------------
    [[nodiscard]] std::vector<std::string> object_keys() const;

    // -- Comparison -----------------------------------------------------------
    [[nodiscard]] bool operator==(const json_type&) const noexcept;
    [[nodiscard]] bool operator!=(const json_type&) const noexcept;

    // -- Lifetime -------------------------------------------------------------
    ~json_type();

    // -- Bridge accessor for user-defined to_json / from_json -----------------
    // Cast the returned pointer to nlohmann::json_type* in the bridge implementation
    // (i.e. inside the to_json / from_json that FIASCO_MODEL generates).
    void* data();
    const void* data() const;

  private:
    using path_segment = std::variant<std::string, std::size_t>;

    struct impl;
    std::shared_ptr<impl> m_data;
    std::vector<path_segment> m_path;

    // -- Type-erased bridge helpers -------------------------------------------
    using from_val_fn = void (*)(json_type&, const void*);
    void construct_from_val(const void* val, from_val_fn fn);

    using to_val_fn = void (*)(const json_type&, void*);
    void extract_val(void* val, to_val_fn fn) const;
};

// -- Key-value entry for object construction ---------------------------------
// Enables json_type{{"key", val}, {"k2", v2}} without ambiguity vs arrays.
struct json_entry {
    std::string key;
    json_type value;
};

// -- Primitive ADL overloads for json_type -> C++ types ---------------------------
// These allow get<T>(), value<T>(), and FIASCO_MODEL to work with built-in
// types without exposing nlohmann::json_type to callers.
void from_json(const json_type&, std::nullptr_t&);
void from_json(const json_type&, bool&);
void from_json(const json_type&, int&);
void from_json(const json_type&, long&);
void from_json(const json_type&, long long&);
void from_json(const json_type&, unsigned long&);
void from_json(const json_type&, unsigned long long&);
void from_json(const json_type&, float&);
void from_json(const json_type&, double&);
void from_json(const json_type&, std::string&);

void to_json(json_type&, std::nullptr_t);
void to_json(json_type&, bool);
void to_json(json_type&, int);
void to_json(json_type&, long);
void to_json(json_type&, long long);
void to_json(json_type&, unsigned long);
void to_json(json_type&, unsigned long long);
void to_json(json_type&, float);
void to_json(json_type&, double);
void to_json(json_type&, const std::string&);
void to_json(json_type&, const char*);
void to_json(json_type&, const json_type&);
void from_json(const json_type&, json_type&);

// -- Container concepts (defined here to break circular dep with concepts.hpp) -

template <typename T>
concept JsonRange = requires(T& t) {
    std::begin(t);
    std::end(t);
};

template <typename T>
concept JsonMap = JsonRange<T> && requires { typename T::mapped_type; };

template <typename T>
concept JsonSequence = JsonRange<T> && !JsonMap<T>;

template <typename T>
concept JsonOptional = requires(T& t) {
    typename T::value_type;
    t.has_value();
    t.value();
} && !JsonRange<T>;

template <typename T>
concept JsonTupleLike =
    requires(T& t) { std::tuple_size<std::decay_t<T>>::value; } && !JsonRange<T>;

// -- Template ADL overload families -------------------------------------------
// Each family has one to_json + from_json pair (from_json may be omitted when
// the reverse direction isn't practical without key-iteration APIs).

// --- Sequence<T> ---  vector, list, set, array, deque, span… -> JSON array

template <JsonSequence T>
void to_json(json_type& j, const T& seq) {
    json_type arr = json_type::array({});
    for (const auto& elem : seq) {
        arr.push_back(json_type(elem));
    }
    j = std::move(arr);
}

template <JsonSequence T>
void from_json(const json_type& j, T& seq) {
    seq.clear();
    auto n = j.size();
    if constexpr (requires { seq.reserve(n); }) {
        seq.reserve(n);
    }
    for (std::size_t i = 0; i < n; ++i) {
        typename T::value_type elem;
        from_json(j[i], elem);
        if constexpr (requires(T& c, typename T::value_type& v) { c.push_back(v); }) {
            seq.push_back(std::move(elem));
        } else {
            seq.insert(seq.end(), std::move(elem));
        }
    }
}

// --- Map<T> ---  map, unordered_map, any K->V with mapped_type -> JSON object

template <JsonMap T>
void to_json(json_type& j, const T& map) {
    json_type obj;
    for (const auto& [key, value] : map) {
        json_type jv(value);
        // Use lvalue so copy-assignment via data() is picked, not move-assignment
        // (move-assignment on a sub-view temporary would detach from the parent).
        if constexpr (std::convertible_to<decltype(key), std::string>) {
            obj[static_cast<const std::string&>(key)] = jv;
        } else {
            obj[std::to_string(key)] = jv;
        }
    }
    j = std::move(obj);
}

template <JsonMap T>
void from_json(const json_type& j, T& map) {
    map.clear();
    auto keys = j.object_keys();
    for (const auto& key : keys) {
        typename T::mapped_type val;
        from_json(j[key], val);
        if constexpr (std::convertible_to<decltype(key), typename T::key_type>) {
            map[key] = std::move(val);
        } else {
            typename T::key_type k(key);
            map[std::move(k)] = std::move(val);
        }
    }
}

// --- Optional<T> ---  std::optional -> value or null

template <JsonOptional T>
void to_json(json_type& j, const T& opt) {
    if (opt.has_value()) {
        to_json(j, opt.value());
    } else {
        to_json(j, nullptr);
    }
}

template <JsonOptional T>
void from_json(const json_type& j, T& opt) {
    using Inner = typename T::value_type;
    if (j.is_null()) {
        opt.reset();
    } else {
        Inner val;
        from_json(j, val);
        opt = std::move(val);
    }
}

// --- TupleLike<T> ---  pair, tuple -> JSON array

template <JsonTupleLike T>
void to_json(json_type& j, const T& tuple) {
    json_type arr = json_type::array({});
    std::apply([&arr](const auto&... args) { (arr.push_back(json_type(args)), ...); }, tuple);
    j = std::move(arr);
}

template <JsonTupleLike T, size_t... Is>
void from_json_tuple_impl(const json_type& j, T& tuple, std::index_sequence<Is...>) {
    ((from_json(j[Is], std::get<Is>(tuple))), ...);
}

template <JsonTupleLike T>
void from_json(const json_type& j, T& tuple) {
    from_json_tuple_impl(j, tuple, std::make_index_sequence<std::tuple_size_v<T>>{});
}

// -- Field-level helpers for FIASCO_MODEL ------------------------------------
// Called by the macro-generated to_json / from_json to give std::optional
// fields FastAPI-like treatment: tolerate missing keys on input (leaves
// them as std::nullopt).

template <typename T>
void to_json_field(json_type& j, const char* key, const T& field) {
    j[key] = json_type(field);
}

template <typename T>
void from_json_field(const json_type& j, const char* key, std::optional<T>& field) {
    if (j.contains(key)) {
        field = j[key].get<std::optional<T>>();
    } else {
        field.reset();
    }
}

template <typename T>
void from_json_field(const json_type& j, const char* key, T& field) {
    field = j[key].get<T>();
}

}  // namespace fiasco::detail
