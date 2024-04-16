
#define internal static
#define global_variable static
#define local_persist static


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef int32_t bool32;
typedef bool32 b32;

typedef float real32;
typedef double real64;

typedef real32 f32; // float
typedef real64 d64; // double

typedef u32 entity_id;

#define ID_DONT_EXIST 99999999


struct v2 {
    float x,y;
};

struct v2i {
    int x,y;
};

inline v2 operator*(v2 left, double right) {
    return {left.x*(float)right,left.y*(float)right};
}

inline v2 operator*(v2 left, float right) {
    return {left.x*right,left.y*right};
}

inline v2 operator*(v2 left, int right) {
    return {left.x*right,left.y*right};
}

inline v2 operator+(v2 left, v2 right) {
    return {left.x+right.x,left.y+right.y};
}

inline v2 operator-(v2 left, v2 right) {
    return {left.x-right.x,left.y-right.y};
}

inline v2 operator-(v2 right) {
    return {-right.x,-right.y};
}

inline v2 &operator+=(v2 &left, v2 right) {
    left.x += right.x;
    left.y += right.y;
    return left;
}

inline v2 &operator-=(v2 &left, v2 right) {
    left.x -= right.x;
    left.y -= right.y;
    return left;
}
