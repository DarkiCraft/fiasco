#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
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

    // From user model type via ADL to_json(json_type&, const T&)
    template <typename T>
    json_type(const T& val) : json_type() {
        construct_from_val(&val, [](json_type& j, const void* v) {
            to_json(j, *static_cast<const T*>(v));
        });
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
    [[nodiscard]] bool is_null()    const noexcept;
    [[nodiscard]] bool is_boolean() const noexcept;
    [[nodiscard]] bool is_number()  const noexcept;
    [[nodiscard]] bool is_string()  const noexcept;
    [[nodiscard]] bool is_object()  const noexcept;
    [[nodiscard]] bool is_array()   const noexcept;

    // -- Query ----------------------------------------------------------------
    [[nodiscard]] bool         contains(const std::string& key) const noexcept;
    [[nodiscard]] bool         empty()   const noexcept;
    [[nodiscard]] std::size_t  size()    const noexcept;

    // -- Value extraction with type conversion / default ----------------------
    template <typename T>
    [[nodiscard]] T get() const {
        T val;
        extract_val(&val, [](const json_type& j, void* v) {
            from_json(j, *static_cast<T*>(v));
        });
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
    static json_type        parse(const std::string& str);
    [[nodiscard]] std::string dump(int indent = -1) const;

    // -- Mutation -------------------------------------------------------------
    void push_back(json_type value);
    void erase(const std::string& key);
    void merge_patch(const json_type& patch);
    void clear();

    // -- Comparison -----------------------------------------------------------
    [[nodiscard]] bool operator==(const json_type&) const noexcept;
    [[nodiscard]] bool operator!=(const json_type&) const noexcept;

    // -- Lifetime -------------------------------------------------------------
    ~json_type();

    // -- Bridge accessor for user-defined to_json / from_json -----------------
    // Cast the returned pointer to nlohmann::json_type* in the bridge implementation
    // (i.e. inside the to_json / from_json that FIASCO_MODEL generates).
    void*       data();
    const void* data() const;

  private:
    using path_segment = std::variant<std::string, std::size_t>;

    struct impl;
    std::shared_ptr<impl>       m_data;
    std::vector<path_segment>   m_path;

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

}  // namespace fiasco::detail
