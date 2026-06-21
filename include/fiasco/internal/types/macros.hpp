#pragma once

#include "fiasco/internal/types/json.hpp"

// -- FOREACH helpers (fixed-limit up to 64 fields) --------------------------
// A variadic FOR_EACH is impossible in C preprocessing without an explicit
// macro-per-count because self-referential macros are blue-painted during
// expansion and cannot recurse.  This is the same approach used by both
// Boost.PP and nlohmann/json internally.

#define FIASCO_DETAIL_CAT(a, b) a##b

#define FIASCO_DETAIL_GET_COUNT(_1,  \
                                _2,  \
                                _3,  \
                                _4,  \
                                _5,  \
                                _6,  \
                                _7,  \
                                _8,  \
                                _9,  \
                                _10, \
                                _11, \
                                _12, \
                                _13, \
                                _14, \
                                _15, \
                                _16, \
                                _17, \
                                _18, \
                                _19, \
                                _20, \
                                _21, \
                                _22, \
                                _23, \
                                _24, \
                                _25, \
                                _26, \
                                _27, \
                                _28, \
                                _29, \
                                _30, \
                                _31, \
                                _32, \
                                _33, \
                                _34, \
                                _35, \
                                _36, \
                                _37, \
                                _38, \
                                _39, \
                                _40, \
                                _41, \
                                _42, \
                                _43, \
                                _44, \
                                _45, \
                                _46, \
                                _47, \
                                _48, \
                                _49, \
                                _50, \
                                _51, \
                                _52, \
                                _53, \
                                _54, \
                                _55, \
                                _56, \
                                _57, \
                                _58, \
                                _59, \
                                _60, \
                                _61, \
                                _62, \
                                _63, \
                                _64, \
                                N,   \
                                ...) \
    N
#define FIASCO_DETAIL_COUNT(...)         \
    FIASCO_DETAIL_GET_COUNT(__VA_ARGS__, \
                            64,          \
                            63,          \
                            62,          \
                            61,          \
                            60,          \
                            59,          \
                            58,          \
                            57,          \
                            56,          \
                            55,          \
                            54,          \
                            53,          \
                            52,          \
                            51,          \
                            50,          \
                            49,          \
                            48,          \
                            47,          \
                            46,          \
                            45,          \
                            44,          \
                            43,          \
                            42,          \
                            41,          \
                            40,          \
                            39,          \
                            38,          \
                            37,          \
                            36,          \
                            35,          \
                            34,          \
                            33,          \
                            32,          \
                            31,          \
                            30,          \
                            29,          \
                            28,          \
                            27,          \
                            26,          \
                            25,          \
                            24,          \
                            23,          \
                            22,          \
                            21,          \
                            20,          \
                            19,          \
                            18,          \
                            17,          \
                            16,          \
                            15,          \
                            14,          \
                            13,          \
                            12,          \
                            11,          \
                            10,          \
                            9,           \
                            8,           \
                            7,           \
                            6,           \
                            5,           \
                            4,           \
                            3,           \
                            2,           \
                            1)

#define FIASCO_DETAIL_FOREACH_1(m, x) m(x)
#define FIASCO_DETAIL_FOREACH_2(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_1(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_3(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_2(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_4(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_3(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_5(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_4(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_6(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_5(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_7(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_6(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_8(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_7(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_9(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_8(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_10(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_9(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_11(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_10(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_12(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_11(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_13(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_12(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_14(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_13(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_15(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_14(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_16(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_15(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_17(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_16(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_18(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_17(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_19(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_18(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_20(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_19(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_21(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_20(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_22(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_21(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_23(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_22(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_24(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_23(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_25(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_24(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_26(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_25(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_27(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_26(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_28(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_27(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_29(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_28(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_30(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_29(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_31(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_30(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_32(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_31(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_33(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_32(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_34(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_33(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_35(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_34(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_36(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_35(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_37(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_36(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_38(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_37(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_39(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_38(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_40(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_39(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_41(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_40(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_42(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_41(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_43(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_42(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_44(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_43(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_45(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_44(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_46(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_45(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_47(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_46(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_48(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_47(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_49(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_48(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_50(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_49(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_51(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_50(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_52(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_51(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_53(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_52(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_54(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_53(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_55(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_54(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_56(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_55(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_57(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_56(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_58(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_57(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_59(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_58(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_60(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_59(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_61(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_60(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_62(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_61(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_63(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_62(m, __VA_ARGS__)
#define FIASCO_DETAIL_FOREACH_64(m, x, ...) m(x) FIASCO_DETAIL_FOREACH_63(m, __VA_ARGS__)

#define FIASCO_DETAIL_FOREACH_N(n, m, ...) \
    FIASCO_DETAIL_CAT(FIASCO_DETAIL_FOREACH_, n)(m, __VA_ARGS__)

#define FIASCO_FOREACH(m, ...) \
    FIASCO_DETAIL_FOREACH_N(FIASCO_DETAIL_COUNT(__VA_ARGS__), m, __VA_ARGS__)

// -- Field-level helpers for FIASCO_MODEL ------------------------------------

#define FIASCO_DETAIL_TO_JSON_FIELD(field) ::fiasco::detail::to_json_field(j, #field, v.field);

#define FIASCO_DETAIL_FROM_JSON_FIELD(field) ::fiasco::detail::from_json_field(j, #field, v.field);

/// Generates non-intrusive to_json / from_json for a struct using the
/// PIMPL json class (no nlohmann headers exposed).
///
/// Usage:
///   struct user { std::string name; int age; };
///   FIASCO_MODEL(user, name, age)
///
/// This generates:
///   void to_json(fiasco::detail::json&, const user&);
///   void from_json(const fiasco::detail::json&, user&);
#define FIASCO_MODEL(Type, ...)                                               \
    inline void to_json(::fiasco::detail::json& j, const Type& v) {           \
        j = ::fiasco::detail::json::object();                                  \
        FIASCO_FOREACH(FIASCO_DETAIL_TO_JSON_FIELD, __VA_ARGS__)              \
    }                                                                         \
    inline void from_json(const ::fiasco::detail::json& j, Type& v) {         \
        FIASCO_FOREACH(FIASCO_DETAIL_FROM_JSON_FIELD, __VA_ARGS__)            \
    }
