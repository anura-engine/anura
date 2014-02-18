#Note: Do not invoke this script directly, rather, use ". bash_autocomplete_setup.sh". Then you may tab-complete after ./game.
_anura() 
{
    local cur prev opts utils module_names level_names
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    case "${prev}" in
    "--level")
        level_names=$(find -type f -regex ".*/level/.*\.cfg" -printf "%f\n")
        COMPREPLY=( $(compgen -W "${level_names}" -- ${cur}) )
        return 0
        ;;
        
     *)
        #opts will get --module= and --utility= filled in later.
        opts="--config-path --fullscreen --height --host --joystick --no-joystick --level --level-path --music --no-music --native --relay --resizable --no-resizable --module-args --scale --send-stats --no-send-stats --server --user --pass --sound --no-sound --widescreen --width --windowed --wvga --debug --no-debug --fps --no-fps --set-fps --potonly --textures16 --benchmarks --textures32 --benchmarks --compiled --no-compiled --edit --show-hitboxes --show-controls --simipad --simiphone --no-autopause --tests --no-tests --utility --assert_on_missing_sound --no-assert_on_missing_sound --auto_update_module --debug_custom_draw --no-debug_custom_draw --debug_shadows --no-debug_shadows --editor_grid --no-editor_grid --fakelag --force_strict_mode --no-force_strict_mode --global_frame_scale --max_ffl_recursion --msaa --respect_difficulty --no-respect_difficulty --strict_mode_warnings --no-strict_mode_warnings --suppress_strict_mode --no-suppress_strict_mode --tbs_bot_delay_ms --tbs_client_prediction --no-tbs_client_prediction --tbs_game_exit_on_winner --no-tbs_game_exit_on_winner --tbs_server_delay_ms --tbs_server_heartbeat_freq --tile_size --vsync"
        utils="bake_spritesheet calculate_normal_map codeedit compile_levels compile_objects correct_solidity document_ffl_functions generate_scaling_code hole_punch_test install_module list_modules load_and_save_all_levels manipulate_image_template module_server multiplayer_server object_definition publish_module publish_module_stats query render_level sign_game_data stats_server tbs_bot_game tbs_matchmaking_server tbs_server test_all_objects textedit voxel_animator voxel_editor widget_editor"
        
        #Appends the module=… commands. Since I don't know how to look for the module= command (it doesn't show up in "prev") we'll just generate every single possible combination.
        module_names=$(ls modules --format=single-column --color=never)
        for i in $module_names
        do
            opts="${opts} --module=${i}"
        done
        
        for i in $utils
        do
            opts="${opts} --utility=${i}"
        done
        
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
        ;;
    esac
}
launches="anura anura~"
for i in $launches
do
    complete -F _anura ${i}
done