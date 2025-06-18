#pragma once

/* example:
 *
 * ADT_MAKE_SOA_STRUCT(Entity,
 *     (adt::StringFixed<128>, sfName),
 *     (adt::math::V4, color),
 *     (adt::math::V3, pos),
 *     (adt::math::Qt, rot),
 *     (adt::math::V3, scale),
 *     (adt::math::V3, vel),
 *     (adt::i16, assetI),
 *     (adt::i16, modelI),
 *     (bool, bNoDraw)
 * );
 *
 * expands to:
 *
 * struct Entity {
 *   struct Bind {
 *     adt::StringFixed<128>& sfName;
 *     adt::math::V4& color;
 *     adt::math::V3& pos;
 *     adt::math::Qt& rot;
 *     adt::math::V3& scale;
 *     adt::math::V3& vel;
 *     adt::i16& assetI;
 *     adt::i16& modelI;
 *     bool& bNoDraw;
 *   };
 *   adt::StringFixed<128> sfName;
 *   adt::math::V4 color;
 *   adt::math::V3 pos;
 *   adt::math::Qt rot;
 *   adt::math::V3 scale;
 *   adt::math::V3 vel;
 *   adt::i16 assetI;
 *   adt::i16 modelI;
 *   bool bNoDraw;
 * }; */

#define ADT_PP_RSEQ_N() 20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
#define ADT_PP_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,N,...) N
#define ADT_PP_NARG_(...) ADT_PP_ARG_N(__VA_ARGS__)
#define ADT_PP_NARG(...) ADT_PP_NARG_(__VA_ARGS__, ADT_PP_RSEQ_N())

#define ADT_PP_CONCAT(A, B) ADT_PP_CONCAT_I(A, B)
#define ADT_PP_CONCAT_I(A, B) A##B

#define ADT_PP_FOR_EACH(MACRO, ...) ADT_PP_CONCAT(ADT_PP_FOR_EACH_, ADT_PP_NARG(__VA_ARGS__))(MACRO, __VA_ARGS__)

#define PP_FOR_EACH_1(m, x1) \
    m(x1)
#define ADT_PP_FOR_EACH_2(m, x1, x2) \
    m(x1) m(x2)
#define ADT_PP_FOR_EACH_3(m, x1, x2, x3) \
    m(x1) m(x2) m(x3)
#define ADT_PP_FOR_EACH_4(m, x1, x2, x3, x4) \
    m(x1) m(x2) m(x3) m(x4)
#define ADT_PP_FOR_EACH_5(m, x1, x2, x3, x4, x5) \
    m(x1) m(x2) m(x3) m(x4) m(x5)
#define ADT_PP_FOR_EACH_6(m, x1, x2, x3, x4, x5, x6) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6)
#define ADT_PP_FOR_EACH_7(m, x1, x2, x3, x4, x5, x6, x7) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7)
#define ADT_PP_FOR_EACH_8(m, x1, x2, x3, x4, x5, x6, x7, x8) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8)
#define ADT_PP_FOR_EACH_9(m, x1, x2, x3, x4, x5, x6, x7, x8, x9) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9)
#define ADT_PP_FOR_EACH_10(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10)
#define ADT_PP_FOR_EACH_11(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11)
#define ADT_PP_FOR_EACH_12(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12)
#define ADT_PP_FOR_EACH_13(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13)
#define ADT_PP_FOR_EACH_14(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14)
#define ADT_PP_FOR_EACH_15(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15)
#define ADT_PP_FOR_EACH_16(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16)
#define ADT_PP_FOR_EACH_17(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17)
#define ADT_PP_FOR_EACH_18(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18)
#define ADT_PP_FOR_EACH_19(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19)
#define ADT_PP_FOR_EACH_20(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20) \
    m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20)

#define ADT_DECL_FIELD(TUPLE) ADT_DECL_FIELD_I TUPLE;
#define ADT_DECL_FIELD_I(TYPE, NAME) TYPE NAME

#define ADT_DECL_FIELD_REF(TUPLE) ADT_DECL_FIELD_REF_I TUPLE;
#define ADT_DECL_FIELD_REF_I(TYPE, NAME) TYPE& NAME

#define ADT_MAKE_SOA_STRUCT(NAME, ...)                                                                                 \
    struct NAME                                                                                                        \
    {                                                                                                                  \
        struct Bind                                                                                                    \
        {                                                                                                              \
            ADT_PP_FOR_EACH(ADT_DECL_FIELD_REF, __VA_ARGS__)                                                           \
        };                                                                                                             \
                                                                                                                       \
        ADT_PP_FOR_EACH(ADT_DECL_FIELD, __VA_ARGS__)                                                                   \
    }
