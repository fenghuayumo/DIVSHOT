#pragma once

// Defines all bitwise operators for enum classes so it can be (mostly) used as a regular flags enum
#define ENUM_CLASS_FLAGS(T) \
    inline T operator~ (T a) { return (T)~(int)a; } \
    inline T operator| (T a, T b) { return (T)((int)a | (int)b); } \
    inline int operator& (T a, T b) { return ((int)a & (int)b); } \
    inline T operator^ (T a, T b) { return (T)((int)a ^ (int)b); } \
    inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); } \
    inline T& operator&= (T& a, T b) { return (T&)((int&)a &= (int)b); } \
    inline T& operator^= (T& a, T b) { return (T&)((int&)a ^= (int)b); }

// Friends all bitwise operators for enum classes so the definition can be kept private / protected.
#define FRIEND_ENUM_CLASS_FLAGS(Enum) \
	friend           Enum& operator|=(Enum& Lhs, Enum Rhs); \
	friend           Enum& operator&=(Enum& Lhs, Enum Rhs); \
	friend           Enum& operator^=(Enum& Lhs, Enum Rhs); \
	friend constexpr Enum  operator| (Enum  Lhs, Enum Rhs); \
	friend constexpr Enum  operator& (Enum  Lhs, Enum Rhs); \
	friend constexpr Enum  operator^ (Enum  Lhs, Enum Rhs); \
	friend constexpr bool  operator! (Enum  E); \
	friend constexpr Enum  operator~ (Enum  E);

template<typename Enum>
constexpr bool enum_has_all_flags(Enum Flags, Enum Contains)
{
	return ((int)Flags & (int)Contains) == ((int)Contains);
}

template<typename Enum>
constexpr bool enum_has_any_flags(Enum Flags, Enum Contains)
{
	return ((int)Flags & (int)Contains) != 0;
}

template<typename Enum>
void enum_add_flags(Enum& Flags, Enum FlagsToAdd)
{
	Flags |= FlagsToAdd;
}

template<typename Enum>
void enum_remove_flags(Enum& Flags, Enum FlagsToRemove)
{
	Flags &= ~FlagsToRemove;
}