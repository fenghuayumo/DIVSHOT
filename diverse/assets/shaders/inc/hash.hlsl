#ifndef HASH_HLSL
#define HASH_HLSL

#include "math_const.hlsl"

// Jenkins hash function. TODO: check if we need something better.
uint hash1(uint x) {
	x += (x << 10u);
	x ^= (x >>  6u);
	x += (x <<  3u);
	x ^= (x >> 11u);
	x += (x << 15u);
	return x;
}

uint hash1_mut(inout uint h) {
    uint res = h;
    h = hash1(h);
    return res;
}

uint hash_combine2(uint x, uint y) {
    static const uint M = 1664525u, C = 1013904223u;
    uint seed = (x * M + y + C) * M;

    // Tempering (from Matsumoto)
    seed ^= (seed >> 11u);
    seed ^= (seed << 7u) & 0x9d2c5680u;
    seed ^= (seed << 15u) & 0xefc60000u;
    seed ^= (seed >> 18u);
    return seed;
}

uint hash2(uint2 v) {
	return hash_combine2(v.x, hash1(v.y));
}

uint hash3(uint3 v) {
	return hash_combine2(v.x, hash2(v.yz));
}

uint hash4(uint4 v) {
	return hash_combine2(v.x, hash3(v.yzw));
}

float uint_to_u01_float(uint h) {
	static const uint mantissaMask = 0x007FFFFFu;
	static const uint one = 0x3F800000u;

	h &= mantissaMask;
	h |= one;

	float  r2 = asfloat( h );
	return r2 - 1.0;
}

float interleaved_gradient_noise(uint2 px) {
    return frac(52.9829189 * frac(0.06711056f * float(px.x) + 0.00583715f * float(px.y)));
}


// 32 bit Jenkins hash
uint jenkins_hash(uint a)
{
    // http://burtleburtle.net/bob/hash/integer.html
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// https://www.pcg-random.org/
uint pcg(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// xxhash (https://github.com/Cyan4973/xxHash)
//   From https://www.shadertoy.com/view/Xt3cDn
uint xxhash32(uint p)
{
    const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
    const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
    uint h32 = p + PRIME32_5;
    h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
    h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
    h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
    return h32 ^ (h32 >> 16);
}

#endif
