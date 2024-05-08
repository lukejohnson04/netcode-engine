
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

typedef u16 entity_id;

#define ID_DONT_EXIST 65500
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
    // gonna regret this implicit constructor later lol
    v2(int nx, int ny) : x((float)nx), y((float)ny) {}    
    v2(float nx, float ny) : x(nx), y(ny) {}
    v2(v2i a) : x((float)a.x),y((float)a.y) {}
    v2 normalize() {
        float len = sqrt((x * x) + (y * y));
        return v2(x / len, y / len);
    }

    float get_length();

    v2 rotate(float rad);
};

struct v2d {
    double x,y;
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

inline bool operator==(const v2 &left, const v2 &right) {
    return left.x == right.x && left.y == right.y;
}

inline bool operator==(const v2i &left, const v2i &right) {
    return left.x == right.x && left.y == right.y;
}

inline bool operator!=(const v2 &left, const v2 &right) {
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


inline v2i &operator+=(v2i &left, v2i right) {
    left.x += right.x;
    left.y += right.y;
    return left;
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
    Color(u8 nr,u8 ng,u8 nb,u8 na=255) : r(nr),g(ng),b(nb),a(na) {}
    u8 r,g,b,a;
};


inline bool operator==(Color left, Color right) {
    return *(u32*)&left == *(u32*)&right;
}



static float get_angle_to_point(v2 a, v2 b) {
    float angle = atan2(a.y - b.y, a.x - b.x);
    return angle;
}


static double dget_angle_to_point(v2 a, v2 b) {
    double angle = atan2(a.y - b.y, a.x - b.x);
    return angle;
}


static v2 convert_angle_to_vec(float angle) {
    v2 p = {-cos(angle), -sin(angle)};
    return p;
}

static v2 dconvert_angle_to_vec(double angle) {
    v2 p = {(float)-cos(angle), (float)-sin(angle)};
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


float fdistance_between_points(v2 p1,v2 p2) {
    return (float)sqrt(pow(p2.x-p1.x,2) + pow(p2.y - p1.y,2));
}


float v2::get_length() {
    return fdistance_between_points({0,0},*this);
}

v2 v2::rotate(float rad) {
    return convert_angle_to_vec(convert_vec_to_angle(*this) + rad) * get_length();
}


struct intersect_props {
    v2 collision_point;
    bool collides;
};


inline double distance_between_points(v2 p1, v2 p2) {
    return sqrt(pow(p2.x-p1.x,2) + pow(p2.y - p1.y,2));
}

inline float cross_product(v2i a, v2i b) {
    return (float)(a.x * b.y - a.y * b.x);
}

inline intersect_props get_intersection(v2i ray_start, v2i ray_end, v2i seg_start, v2i seg_end) {
    intersect_props result;
    result.collides = false;

    v2i ray_direction = {ray_end.x - ray_start.x, ray_end.y - ray_start.y};
    v2i seg_direction = {seg_end.x - seg_start.x, seg_end.y - seg_start.y};

    float ray_seg_cross = cross_product(ray_direction, seg_direction);

    if (ray_seg_cross == 0) // Ray and segment are parallel
        return result;

    v2i start_to_start = {seg_start.x - ray_start.x, seg_start.y - ray_start.y};

    float t = cross_product(start_to_start, seg_direction) / ray_seg_cross;
    float u = cross_product(start_to_start, ray_direction) / ray_seg_cross;

    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        // Collision detected, calculate collision point
        result.collides = true;
        result.collision_point.x = ray_start.x + t * ray_direction.x;
        result.collision_point.y = ray_start.y + t * ray_direction.y;
    }

    return result;
}

/*

  This function also works... i guess... not really.. lol
intersect_props get_intersection(v2i ray_start, v2i ray_end, v2i seg_start, v2i seg_end) {
    intersect_props res;
    res.collides = false;

    // RAY in parametric: Point + Delta*T1
    double r_px = ray_start.x;
    double r_py = ray_start.y;
    double r_dx = ray_end.x-ray_start.x;
    double r_dy = ray_end.y-ray_start.y;

    // SEGMENT in parametric: Point + Delta*T2
    double s_px = seg_start.x;
    double s_py = seg_start.y;
    double s_dx = seg_end.x-seg_start.x;
    double s_dy = seg_end.y-seg_start.y;

    // Are they parallel? If so, no intersect
    double r_mag = sqrt(r_dx*r_dx+r_dy*r_dy);
    double s_mag = sqrt(s_dx*s_dx+s_dy*s_dy);
    if(r_dx/r_mag==s_dx/s_mag && r_dy/r_mag==s_dy/s_mag){
        // Unit vectors are the same.
        return res;
    }

    // SOLVE FOR T1 & T2
    // r_px+r_dx*T1 = s_px+s_dx*T2 && r_py+r_dy*T1 = s_py+s_dy*T2
    // ==> T1 = (s_px+s_dx*T2-r_px)/r_dx = (s_py+s_dy*T2-r_py)/r_dy
    // ==> s_px*r_dy + s_dx*T2*r_dy - r_px*r_dy = s_py*r_dx + s_dy*T2*r_dx - r_py*r_dx
    // ==> T2 = (r_dx*(s_py-r_py) + r_dy*(r_px-s_px))/(s_dx*r_dy - s_dy*r_dx)
    double T2 = (r_dx*(s_py-r_py) + r_dy*(r_px-s_px))/(s_dx*r_dy - s_dy*r_dx);
    double T1 = (s_px+s_dx*T2-r_px)/r_dx;

    // Must be within parametic whatevers for RAY/SEGMENT
    if(T1<0) return res;
    if(T2<0 || T2>1) return res;

    res.collides = true;
    // Return the POINT OF INTERSECTION
    res.collision_point.x = (float)(r_px+r_dx*T1);
    res.collision_point.y = (float)(r_py+r_dy*T1);
    return res;
}
*/


struct segment {
    v2 p1,p2;
    bool operator ==(const segment&o) {
        return p1==o.p1&&p2==o.p2;
    }
};

inline intersect_props get_collision(v2i from, v2 to, std::vector<segment> segments) {
    intersect_props res;
    res.collides=false;

    double closest = -1;
    
    for (i32 ind=0; ind<segments.size(); ind++) {
        segment &seg = segments[ind];
        intersect_props col = get_intersection(from,to,seg.p1,seg.p2);
        if (col.collides) {
            double dist = distance_between_points(from,col.collision_point);
            if (res.collides==false || dist<=closest) {
                closest = dist;
                res = col;
            }
        }
    }

    return res;
}
