#pragma once
/* Generated with PPGenerator.cc. */

/* NOTE: MSVC needs '/Zc:preprocessor'. */

/* example:
 *
 * #define ENTITY_PP_BIND_I(TYPE, NAME) , &Entity::NAME
 * #define ENTITY_PP_BIND(TUPLE) ENTITY_PP_BIND_I TUPLE
 * #define ENTITY_FIELDS \
 *     (adt::StringFixed<128>, sfName),
 *     (adt::math::V4, color),
 *     (adt::math::V3, pos),
 *     (adt::math::Qt, rot),
 *     (adt::math::V3, scale),
 *     (adt::math::V3, vel),
 *     (adt::i16, assetI),
 *     (adt::i16, modelI),
 *     (bool, bNoDraw)
 * ADT_SOA_GEN_STRUCT_ZERO(Entity, Bind, ENTITY_FIELDS);
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

#define ADT_PP_RSEQ_N() 50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
#define ADT_PP_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,N,...) N
#define ADT_PP_NARG_(...) ADT_PP_ARG_N(__VA_ARGS__)
#define ADT_PP_NARG(...) ADT_PP_NARG_(__VA_ARGS__, ADT_PP_RSEQ_N())

#define ADT_PP_CONCAT(A, B) ADT_PP_CONCAT_I(A, B)
#define ADT_PP_CONCAT_I(A, B) A##B

#define ADT_PP_FOR_EACH(MACRO, ...) ADT_PP_CONCAT(ADT_PP_FOR_EACH_, ADT_PP_NARG(__VA_ARGS__))(MACRO, __VA_ARGS__)

#define ADT_PP_FOR_EACH_1(m, x1) \
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
#define ADT_PP_FOR_EACH_21(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21)
#define ADT_PP_FOR_EACH_22(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22)
#define ADT_PP_FOR_EACH_23(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23)
#define ADT_PP_FOR_EACH_24(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24)
#define ADT_PP_FOR_EACH_25(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25)
#define ADT_PP_FOR_EACH_26(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26)
#define ADT_PP_FOR_EACH_27(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27)
#define ADT_PP_FOR_EACH_28(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28)
#define ADT_PP_FOR_EACH_29(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29)
#define ADT_PP_FOR_EACH_30(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30)
#define ADT_PP_FOR_EACH_31(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31)
#define ADT_PP_FOR_EACH_32(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32)
#define ADT_PP_FOR_EACH_33(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33)
#define ADT_PP_FOR_EACH_34(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34)
#define ADT_PP_FOR_EACH_35(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35)
#define ADT_PP_FOR_EACH_36(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36)
#define ADT_PP_FOR_EACH_37(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37)
#define ADT_PP_FOR_EACH_38(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38)
#define ADT_PP_FOR_EACH_39(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39)
#define ADT_PP_FOR_EACH_40(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40)
#define ADT_PP_FOR_EACH_41(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41)
#define ADT_PP_FOR_EACH_42(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42)
#define ADT_PP_FOR_EACH_43(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43)
#define ADT_PP_FOR_EACH_44(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44)
#define ADT_PP_FOR_EACH_45(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44) m(x45)
#define ADT_PP_FOR_EACH_46(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44) m(x45) m(x46)
#define ADT_PP_FOR_EACH_47(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44) m(x45) m(x46) m(x47)
#define ADT_PP_FOR_EACH_48(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44) m(x45) m(x46) m(x47) m(x48)
#define ADT_PP_FOR_EACH_49(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48, x49) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44) m(x45) m(x46) m(x47) m(x48) m(x49)
#define ADT_PP_FOR_EACH_50(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x50) \
	m(x1) m(x2) m(x3) m(x4) m(x5) m(x6) m(x7) m(x8) m(x9) m(x10) m(x11) m(x12) m(x13) m(x14) m(x15) m(x16) m(x17) m(x18) m(x19) m(x20) m(x21) m(x22) m(x23) m(x24) m(x25) m(x26) m(x27) m(x28) m(x29) m(x30) m(x31) m(x32) m(x33) m(x34) m(x35) m(x36) m(x37) m(x38) m(x39) m(x40) m(x41) m(x42) m(x43) m(x44) m(x45) m(x46) m(x47) m(x48) m(x49) m(x50)

#define ADT_DECL_FIELD_ZERO(TUPLE) ADT_DECL_FIELD_ZERO_I TUPLE;
#define ADT_DECL_FIELD_ZERO_I(TYPE, NAME) TYPE NAME {}

#define ADT_DECL_FIELD(TUPLE) ADT_DECL_FIELD_I TUPLE;
#define ADT_DECL_FIELD_I(TYPE, NAME) TYPE NAME

#define ADT_DECL_FIELD_REF(TUPLE) ADT_DECL_FIELD_REF_I TUPLE;
#define ADT_DECL_FIELD_REF_I(TYPE, NAME) TYPE& NAME

#define ADT_SOA_GEN_STRUCT(NAME, BIND, ...)                                                                            \
    struct NAME                                                                                                        \
    {                                                                                                                  \
        struct BIND                                                                                                    \
        {                                                                                                              \
            ADT_PP_FOR_EACH(ADT_DECL_FIELD_REF, __VA_ARGS__)                                                           \
        };                                                                                                             \
                                                                                                                       \
        ADT_PP_FOR_EACH(ADT_DECL_FIELD, __VA_ARGS__)                                                                   \
    }

#define ADT_SOA_GEN_STRUCT_ZERO(NAME, BIND, ...)                                                                       \
    struct NAME                                                                                                        \
    {                                                                                                                  \
        struct BIND                                                                                                    \
        {                                                                                                              \
            ADT_PP_FOR_EACH(ADT_DECL_FIELD_REF, __VA_ARGS__)                                                           \
        };                                                                                                             \
                                                                                                                       \
        ADT_PP_FOR_EACH(ADT_DECL_FIELD_ZERO, __VA_ARGS__)                                                              \
    }
