
enum ITEM_TYPE : u8 {
    IT_NONE,
    IT_STICK,
    IT_WOOD,
    IT_STONE,
    IT_WOODEN_FENCE,

    IT_WOODEN_PICKAXE,
    IT_SHARPENED_STICK,
    
    IT_COUNT
};

struct item_props {
    bool can_break_walls=false;
    i16 damage_to_walls=0;
};


internal
iRect get_item_rect(ITEM_TYPE type) {
    if (type == IT_STICK) {
        return {16,0,16,16};
    } else if (type == IT_WOOD) {
        return {32,0,16,16};
    } else if (type == IT_WOODEN_FENCE) {
        return {32,16,16,16};
    } else if (type == IT_STONE) {
        return {48,0,16,16};
    } else if (type == IT_WOODEN_PICKAXE) {
        return {16,32,16,16};
    } else if (type == IT_SHARPENED_STICK) {
        return {16,48,16,16};
    } else {
        return {0,0,0,0};
    }
}


struct world_item_t {
    ITEM_TYPE type;
    u8 count;
    v2 pos;
    v3 vel;
    float z_pos;
};

struct inventory_item_t {
    ITEM_TYPE type=IT_NONE;
    u8 count=0;
};

internal
inventory_item_t create_inventory_item(world_item_t *world_item) {
    inventory_item_t item = {};
    item.type = world_item->type;
    item.count = world_item->count;
    return item;
}

internal
world_item_t create_world_item(ITEM_TYPE type, v2 pos, float spawn_dir, u8 count=1) {
    // spawn dir represents the velocity the world item is created with
    // for example if wood comes off of a tree, the spawn dir would be based
    // on the direction the person was hitting the tree was facing
    world_item_t item = {};
    item.type = type;
    item.pos = pos;
    item.z_pos=0.f;
    item.count = count;
    item.vel = {spawn_dir*Random::Float(2.f,7.f),0.f,Random::Float(-16.0f,-22.0f)};
    return item;
}

internal
bool add_to_inventory(inventory_item_t *inv, i32 count, inventory_item_t item) {
    for (inventory_item_t *iter=inv; iter<inv+count; iter++) {
        if (iter->type == IT_NONE) {
            *iter = item;
            return true;
        } else if (iter->type == item.type) {
            // this is going to overflow
            iter->count += item.count;
            return true;
        }
    }
    // return false if the inventory is full
    return false;
}


struct recipe_t {
    static const i32 MAX_RECIPE_INGREDIENTS=4;
    inventory_item_t input[MAX_RECIPE_INGREDIENTS]={};
    inventory_item_t output;
};


i32 recipe_count=0;
recipe_t recipes[256];

void load_recipes() {
    recipes[recipe_count++] = {{{IT_WOOD,2}},{IT_WOODEN_FENCE,1}};
    recipes[recipe_count++] = {{{IT_WOOD,2},{IT_STICK,1}},{IT_WOODEN_PICKAXE,1}};
}

// completely client sided
struct inventory_manager_t {
    i32 selected_slot=0;
    bool crafting_menu_open=false;
};

global_variable inventory_manager_t inventory_ui = {};

// TODO: let player slingshot rocks
