
enum ITEM_TYPE : u8 {
    IT_NONE,
    IT_STICK,
    IT_WOOD,
    IT_COUNT
};

internal
iRect get_item_rect(ITEM_TYPE type) {
    if (type == IT_STICK) {
        return {16,0,16,16};
    } else if (type == IT_WOOD) {
        return {32,0,16,16};
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
world_item_t create_world_item(ITEM_TYPE type, v2 pos, float spawn_dir) {
    // spawn dir represents the velocity the world item is created with
    // for example if wood comes off of a tree, the spawn dir would be based
    // on the direction the person was hitting the tree was facing
    world_item_t item = {};
    item.type = type;
    item.pos = pos;
    item.z_pos=0.f;
    item.vel = {spawn_dir*3.f,0.f,-14.0f};
    return item;
}

internal
bool add_to_inventory(inventory_item_t *inv, i32 count, inventory_item_t item) {
    for (inventory_item_t *iter=inv; inv<inv+count; inv++) {
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
