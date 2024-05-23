
internal
void client_side_update(character *player, camera_t *camera) {
    if (player->inventory[inventory_ui.selected_slot].type == IT_WOODEN_FENCE) {
        v2i cam_mod = v2i(1280/2,720/2) - v2i(camera->pos);

        v2i mpos = get_mouse_position();
        v2i selected_tile = mpos - cam_mod;
        selected_tile = {(i32)selected_tile.x/64,(i32)selected_tile.y/64};
        v2i player_tile = player->pos/64;

        if (distance_between_points(player_tile,selected_tile) < 3) {
            // too far away
            if (input.mouse_just_pressed) {
                // place tile
                player->inventory[0].count--;
                if (player->inventory[0].count == 0) {
                    player->inventory[0].type = IT_NONE;
                }
                world_object_t n_obj = {};
                n_obj.type = WO_WOODEN_FENCE;
                n_obj.took_damage_tick=0;
                n_obj.pos = selected_tile*64.f;
                
                world_chunk_t *chunk = &_mp->chunks[selected_tile.x/CHUNK_SIZE][selected_tile.y/CHUNK_SIZE];
                chunk->add_world_object(n_obj);
                _mp->add_wall(selected_tile);
            }
        }
    }
}
