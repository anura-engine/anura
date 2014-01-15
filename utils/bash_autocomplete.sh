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
        opts="--benchmarks --benchmarks= --compiled --config-path= --debug --edit --edit-and-continue --fps --fullscreen --height --host --joystick --level --music --native --no-autopause --no-compiled --no-debug --no-fps --no-joystick --no-music --no-resizable --no-send-stats --no-sound --no-tests --pass= --potonly --relay --resizable --scale --send-stats --server= --set-fps= --show-controls --show-hitboxes --simipad --simiphone --sound --tbs-server --tests --textures16 --textures16 --textures32 --user= --widescreen --width --windowed --wvga"
        utils="codeedit compile_levels compile_objects correct_solidity document_ffl_functions generate_scaling_code hole_punch_test install_module list_modules module_server object_definition publish_module publish_module_stats query render_level sign_game_data stats_server tbs_server test_all_objects textedit"
        
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