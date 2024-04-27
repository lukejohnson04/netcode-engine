
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
#define ID_SERVER 566

#define For(count) for(i32 ind=0;ind<count;ind++)
#define FOR(arr,count) for(auto obj=arr;obj<arr+count;obj++)
#define FORn(arr,count,name) for(auto name=arr;name<arr+count;name++)

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))


#define PI 3.1415926f

struct v2;
struct v2i {
    int x,y;
    v2i() {}
    v2i(int nx, int ny) : x(nx),y(ny) {} 
    v2i(v2 a);
};



struct v2 {
    float x,y;
    v2() {}
    v2(float nx, float ny) : x(nx), y(ny) {}
    v2(v2i a) : x((float)a.x),y((float)a.y) {}
    v2 normalize() {
        float len = sqrt((x * x) + (y * y));
        return v2(x / len, y / len);
    }
};

v2i::v2i(v2 a) : x((int)a.x),y((int)a.y) {}


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

inline bool operator==(v2 &left, v2 &right) {
    return left.x == right.x && left.y == right.y;
}

inline bool operator!=(v2 &left, v2 &right) {
    return left.x != right.x || left.y != right.y;
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

inline v2 &operator*=(v2 &v, float mult) {
    v.x *= mult;
    v.y *= mult;
    return v;
}


struct iRect {
    int x,y,w,h;
};


struct fRect {
    fRect(float nx,float ny,float nw,float nh) : x(nx),y(ny),w(nw),h(nh) {}
    fRect(iRect a) : x((float)a.x),y((float)a.y),w((float)a.w),h((float)a.h) {}
    float x,y,w,h;
};


bool rect_contains_point(iRect rect, v2 point) {
    return (point.x > rect.x && point.x < rect.x+rect.w && point.y > rect.y && point.y < rect.y+rect.h);
}


bool rects_collide(fRect a, fRect b)
{
    if (a.w == 0 || a.h == 0 || b.w == 0 || b.h == 0)
    {
        return false;
    }
    bool res = a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
    return res;
}

bool rects_collide(iRect a, iRect b) {
    return rects_collide(fRect(a),fRect(b));
}

bool rects_collide(fRect a, iRect b) {
    return rects_collide(a,fRect(b));
}


float lerp(float a, float b, float f) {
    return a * (1.0f-f) + (b*f);
}

v2 lerp(v2 a, v2 b, float f) {
    return {lerp(a.x,b.x,f),lerp(a.y,b.y,f)};
}

struct Color {
    Color(u8 nr,u8 ng,u8 nb,u8 na) : r(nr),g(ng),b(nb),a(na) {}
    union {
        struct {
            u8 r,g,b,a;
        };
        u32 hex;
    };
};


inline bool operator==(Color &left, Color &right) {
    return left.hex==right.hex;
}



static float get_angle_to_point(v2 a, v2 b) {
    float angle = atan2(a.y - b.y, a.x - b.x);
    return angle;
}

static v2 convert_angle_to_vec(float angle) {
    v2 p = {-cos(angle), -sin(angle)};
    return p;
}

// Function to determine difference between angles, accounting for wrapping (i.e. comparing 720 and 30)
static float angle_diff(float a, float b) {
    return PI - abs(abs(a-b) - PI);
}

static v2 get_vec_to_point(v2 a, v2 b) {
    v2 diff = {b.x - a.x, b.y - a.y};
    return diff.normalize();
}

static float convert_vec_to_angle(v2 a) {
    return get_angle_to_point({0,0},a);
}

float deg_2_rad(float d) {
    return d * (PI/180.f);
}

float rad_2_deg(float d) {
    return d / (PI/180.f);
}
