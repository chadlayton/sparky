// https://stackoverflow.com/a/17479300
uint hash(uint x) 
{
    x += (x << 10u);
    x ^= (x >>  6u);
    x += (x <<  3u);
    x ^= (x >> 11u);
    x += (x << 15u);

    return x;
}

uint hash(uint2 v) { return hash(v.x ^ hash(v.y)                        ); }
uint hash(uint3 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z)            ); }
uint hash(uint4 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w)); }

float noise(float  x) { return asfloat(hash(asuint(x))); }
float noise(float2 v) { return asfloat(hash(asuint(v))); }
float noise(float3 v) { return asfloat(hash(asuint(v))); }
float noise(float4 v) { return asfloat(hash(asuint(v))); }