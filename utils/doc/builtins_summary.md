* anura_profiler(): get the interface to the profiler.

* schedule(int cycles_in_future, list of commands): schedules the given list of commands to be run on the current object the given number of cycles in the future. Note that the object must be valid (not destroyed) and still present in the level for the commands to be run.

* fire_event((optional) object target, string id, (optional)callable arg): fires the event with the given id. Targets the current object by default, or target if given. Sends arg as the event argument if given.

* set_language(str): set the language using a new locale code.

* translate(str): returns the translated version of the given string.

* get_texture(string|map): loads a texture.

* texture(objects, rect, bool half_size=false): render a texture.

* desktop_notification(message, title, icon).

* open_url(string url): opens a given url on the platform's web browser.

* get_clipboard_text(): returns the text currentl in the windowing clipboard.

* set_clipboard_text(str): sets the clipboard text to the given string.

* tbs_internal_client(session=-1): creates a client object to the local in-memory tbs server.

* tbs_client(host, port, session=-1): creates a client object to the tbs server.

* tbs_send(tbs_client, msg): sends a message through the given tbs_client connection.

* tbs_process(tbs_client): processes events for the tbs client.

* tbs_blocking_request(tbs_client, request).

* report(): Write a key and a value into [custom] in the stats.

* get_perf_info(): return performance info.

* set_save_slot((optional) int slot): Allows the user to select the save slot, if no slot is specified a dialog is displayed.

* checkpoint_game(): saves a checkpoint of the game.

* get_save_document(int slot): gets the FFL document for the save in the given slot.

* saveGame(): saves the current game state.

* load_game(transition, slot): loads the saved game. If transition (a string) is given, it will use that type of transition.

* can_load_game(): returns true if there is a saved game available to load.

* available_save_slots(): returns a list of numeric indexes of available save slots.

* moveToStanding(): tries to move the object downwards if it's in the air, or upwards if it's in solid space, until it's standing on solid ground.

* music(string id): plays the music file given by 'id' in a loop.

* music_queue(string id, int fade_time=500): plays the music file given by 'id' in a loop after the current music is done.

* music_onetime(string id): plays the music file given by 'id' once.

* sound(string id, decimal volume, decimal fade_in_time, [decimal,decimal] stereo): plays the sound file given by 'id', reaching full volume after fade_in_time seconds.

* sound_loop(string id, decimal volume, decimal fade_in_time, [decimal,decimal] stereo): plays the sound file given by 'id' in a loop, if fade_in_time is given it will reach full volume after this time.

* sound_pan(string id, [decimal,decimal] stereo): pans the sound being played with the given id by the specified stereo amount.

* sound_volume(decimal volume, decimal time_seconds): sets the volume of sound effects.

* stop_sound(string id, (opt) decimal fade_out_time): stops the sound that the current object is playing with the given id, if fade_out_time is given the sound fades out over this time before stopping.

* preload_sound(string id): preload the given sound so it'll be in the sound effects cache.

* create_haptic_effect(string id, map): Creates a haptic effect that can be played later.

* play_haptic_effect(string id, (options) iterations): Plays the given haptic effect. Will default to 'rumble' if it couldn't be created.

* stop_haptic_effect_command(string id): Stops the given haptic effect.

* stop_all_haptic_effect_command(string id): Stops the given haptic effect.

* screen_flash(list int[4] color, (optional) list int[4] delta, int duration): flashes the screen the given color, and keeps the flash going for duration cycles. If delta is given, the color of the flash will be changed every cycle until the duration expires.

* title(string text, int duration=50): shows Level title text on the screen for duration cycles.

* shake_screen(int x_offset, int y_offset, int x_velocity, int y_velocity): makes the screen camera shake.

* radial_current(int intensity, int radius) -> current object: creates a current generator with the given intensity and radius.

* execute_on_level(Level lvl, command cmd): Executes the given commands, with the current level changed to the given level.

* create_level.

* execute(object context, command cmd): this function will execute the command or list of commands given by cmd on the object given by context. For instance, animation('foo') will set the current object to animation 'foo'. execute(obj, animation('foo')) can be used to set the object given by obj to the animation 'foo'.

* execute(object context, string id, command cmd): executes the given commands and instruments the time taken.

* object(string type_id, int midpoint_x, int midpoint_y, (optional) map properties) -> object: constructs and returns a new object. Note that the difference between this and spawn is that spawn returns a command to actually place the object in the Level. object only creates the object and returns it. It may be stored for later use.

* object_playable(string type_id, int midpoint_x, int midpoint_y, int facing, (optional) map properties) -> object: constructs and returns a new object. Note that the difference between this and spawn is that spawn returns a command to actually place the object in the Level. object_playable only creates the playble object and returns it. It may be stored for later use.

* animation(string id): changes the current object's animation to the given animation. time_in_animation is reset to 0.

* die(): causes the current object to die. The object will receive the on_die signal and may even use it to resurrect itself. Use remove_object() to remove an object from play without it receiving on_die.

* facing(int new_facing): changes the current object's facing according to the value of new_facing (1 for right, otherwise left).

* debug_all_custom_objects(): gets access to all custom objects in memory.

* debug_chart(string id, decimal value): plots a sample in a graph.

* debug_rect(int x, int y, (optional)int w=1, (optional) int h=1) -> Draws, for one frame, a rectangle on the Level.

* plot_x(int x): plots a vertical debug line at the given position.

* plot_y(int x): plots a horizontal debug line at the given position.

* point_solid(Level, object, int x, int y) -> boolean: returns true iff the given point is solid for the given object.

* object_can_stand(Level, object, int x, int y) -> boolean: returns true iff the given point is standable.

* find_point_object_can_stand_on(Level, object, int x, int y, int dx, int dy, int max_search=1000) -> [int,int]|null: returns the first point that an object can stand on, starting at [x,y] and incrementing by [dx,dy] until the point is found.

* standable(Level, int x, int y, (optional)int w=1, (optional) int h=1) -> boolean: returns true iff the Level contains standable space within the given (x,y,w,h) rectangle.

* setSolid(x1, y1, x2, y2, boolean is_solid=false): modifies the solidity of the Level such that the rectangle given by (x1, y1, x2, y2) will have its solidity set to the value of is_solid.

* group_size(Level, int group_id) -> int: gives the number of objects in the object group given by group_id.

* setGroup((optional)int group_id): sets the current object to have the given group id, or to be in no group if group_id is not given.

* tiles_at(x, y): gives a list of the tiles at the given x, y position.

* set_tiles(zorder, [area], tile): modify the tilemap within a certain area.

* complete_rebuild_tiles(bool): run to complete the rebuild of tiles started by a previous call to set_tiles.

* get_objects_at_point(x, y): Returns all objects which intersect the specified x,y point, in absolute Level-coordinates.

* toggle_pause().

* hide_window().

* quit_to_desktop().

* update_and_restart().

* scroll_to(object target): scrolls the screen to the target object.

* transient_speech_dialog(...): schedules a sequence of speech dialogs to be shown. Arguments may include a list of strings, which contain text. An integer which sets the duration of the dialog. An object which sets the speaker.

* begin_skip_dialog_sequence(): command that will cause everything up until the next time end_skip_dialog_sequence() is called to be considered a single storyline sequence. If the player selects to skip the sequence between now and then everything up until the call to end_skip_dialog_sequence() will be skipped.

* end_skip_dialog_sequence(): ends the sequence begun with begin_skip_dialog_sequence().

* speech_dialog(...): schedules a sequence of speech dialogs to be shown modally. Arguments may include a list of strings, which contain text. An integer which sets the duration of the dialog. An object which sets the speaker. A string by itself indicates an option that should be shown for the player to select from. A string should be followed by a list of commands that will be executed should the player choose that option.

* paused_speech_dialog(...): like SpeechDialog(), except the game is paused while the dialog is displayed.

* end_game(): exits the game.

* achievement(id): unlocks the achievement with the given id.

* fire_event((optional) object target, string id, (optional)callable arg): fires the event with the given id. Targets the current object by default, or target if given. Sends arg as the event argument if given.

* proto_event(prototype, event_name, (optional) arg): for the given prototype, fire the named event. e.g. proto_event('playable', 'process').

* get_object(Level, string label) -> custom_obj|null: returns the object that is present in the given Level that has the given label.

* get_object_or_die(Level, string label) -> custom_obj: returns the object that is present in the given Level that has the given label.

* resolve_solid(object, int xdir, int ydir, int max_cycles=100): will attempt to move the given object in the direction indicated by xdir/ydir until the object no longer has a solid overlap. Gives up after max_cycles. If called with no arguments other than the object, will try desperately to place the object in the Level.

* add_object(object): inserts the given object into the Level. The object should not currently be present in the Level. The position of the object is tweaked to make sure there are no solid overlaps, however if it is not possible to reasonably place the object without a solid overlap, then the object will not be placed and the object and caller will both receive the event add_object_fail.

* remove_object(object): removes the given object from the Level. If there are no references to the object stored, then the object will immediately be destroyed. However it is possible to keep a reference to the object and even insert it back into the Level later using add_object().

* suspend_Level(string dest_Level).

* resume_level().

* teleport(string dest_Level, (optional)string dest_label, (optional)string transition, (optional)playable): teleports the player to a new Level. The Level is given by dest_Level, with null() for the current Level. If dest_label is given then the player will be teleported to the object in the destination Level with that label. If transition is given, it names a type of transition (such as 'flip' or 'fade') which indicates the kind of visual effect to use for the transition. If a playable is specified it is placed in the Level instead of the current one.  If no_move_to_standing is set to true, rather than auto-positioning the player on the ground under/above the target, the player will appear at precisely the position of the destination object - e.g. this is useful if they need to fall out of a pipe or hole coming out of the ceiling.

* schedule(int cycles_in_future, list of commands): schedules the given list of commands to be run on the current object the given number of cycles in the future. Note that the object must be valid (not destroyed) and still present in the Level for the commands to be run.

* sleep(nseconds).

* sleeps until the current animation is finished.

* add_water(int x1, int y1, int x2, int y2, (optional)[r,g,b,a]=[70,0,0,50]): adds water of the given color in the given rectangle.

* remove_water(int x1, int y1, int x2, int y2): removes water that has the given rectangular area.

* addWave(int x, int y, int xvelocity, int height, int length, int delta_height, int delta_length): will add a wave with the given characteristics at the surface of the water above the (x,y) point. (x,y) must be within a body of water. Waves are a visual effect only and may not display at all on slower devices.

* rect_current(int x, int y, int w, int h, int xvelocity, int yvelocity, int strength) -> current generator object: creates a current generator object that has a current with the given parameters. Set the return value of this function to an object's rect_current to attach it to an object and thus place it in the Level.

* circle_light(object, radius): creates a circle of light with the given radius.

* add_particles(string id): adds the particle system with the given id to the object.

* collides(object a, string area_a, object b, string area_b) -> boolean: returns true iff area_a within object a collides with area_b within object b.

* collides_with_Level(object) -> boolean: returns true iff the given object collides with the Level.

* blur_object(properties, params).

* text(string text, (optional)string font='default', (optional)int size=2, (optional)bool|string centered=false): adds text for the current object.

* swallow_event(): when used in an instance-specific event handler, this causes the event to be swallowed and not passed to the object's main event handler.

* swallow_mouse_event(): when used in an instance-specific event handler, this causes the mouse event to be swallowed and not passed to the next object in the z-order stack.

* animate(object, attributes, options).

* set_widgets((optional) obj, widget, ...): Adds a group of widgets to the current object, or the specified object.

* clear_widgets(obj): Clears all widgets from the object.

* get_widget(object obj, string id): returns the widget with the matching id for given object.

* widget(callable, map w): Constructs a widget defined by w and returns it for later use.

* add_Level_module(string lvl, int xoffset, int yoffset): adds the Level module with the given Level id at the given offset.

* remove_Level_module(string lvl): removes the given Level module.

* cosmic_shift(int xoffset, int yoffet): adjust position of all objects and tiles in the Level by the given offset.

* create_animation(map): creates an animation object from the given data.

* module_client(): creates a module client object. The object will immediately start retrieving basic module info from the server. module_pump() should be called on it every frame. Has the following fields:\n  is_complete: true iff the current operation is complete and a new operation can be started. When the module_client is first created it automatically starts an operation to get the summary of modules.\n  downloaded_modules: a list of downloaded modules that are currently installed.\n  module_info: info about the modules available on the server.\n  error: contains an error string if the operation resulted in an error, null otherwise.\n  kbytes_transferred: number of kbytes transferred in the current operation\n  kbytes_total: total number of kbytes to transfer to complete the operation.

* module_pump(module_client): pumps module client events. Should be called every cycle.

* module_install(module_client, string module_id): begins downloading the given module and installing it. This should only be called when module_client.is_complete = true (i.e. there is no operation currently underway).

* module_uninstall(string module_id): uninstalls the given module.

* module_rate(module_client, string module_id, int num_stars (1-5), (optional) string review): begins a request to rate the given module with the given number of stars, optionally with a review.

* module_launch(string module_id, (optional) callable): launch the game using the given module.

* eval(str, [arg map]): evaluate the given string as FFL.

* spawn_voxel(properties, (optional) list of commands cmd): will create a new object of type given by type_id with the given midpoint and facing. Immediately after creation the object will have any commands given by cmd executed on it. The child object will have the spawned event sent to it, and the parent object will have the child_spawned event sent to it.

* geometry_api().

* canvas() -> canvas object.

* time(int unix_time) -> date_time: returns the current real time.

* get_debug_info(value).

* set_user_info(string, any): sets some user info used in stats collection.

* current_level(): return the current level the game is in.

* cancel(): cancel the current command pipeline.

* overload(fn...): makes an overload of functions.

* addr(obj): Provides the address of the given object as a string. Useful for distinguishing objects.

* get_call_stack().

* get_full_call_stack().

* create_cache(max_entries=4096): makes an FFL cache object.

* create_cache(max_entries=4096): makes an FFL cache object.

* game_preferences() ->builtin game_preferences.

* md5(string) ->string.

* if(a,b,c).

* bind(fn, args...).

* bind_command(fn, args..).

* bind_closure(fn, obj): binds the given lambda fn to the given object closure.

* singleton(string typename): create a singleton object with the given typename.

* construct(string typename, arg): construct an object with the given typename.

* update_object(target_instance, src_instance).

* apply_delta(instance, delta).

* delay_until_end_of_loading(string): delays evaluation of the enclosed until loading is finished.

* eval_lua(str).

* compile_lua(str).

* eval_no_recover(str, [arg]): evaluate the given string as FFL.

* eval(str, [arg]): evaluate the given string as FFL.

* set_mouse_cursor(string cursor).

* parse_xml(str): Parses XML into a JSON structure.

* has_timed_out(): will evaluate to true iff the timeout specified by an enclosing eval_with_timeout() has elapsed.

* get_error_message: called after handle_errors() to get the error message.

* switch(value, case1, result1, case2, result2 ... casen, resultn, default) -> value: returns resultn where value = casen, or default otherwise.

* query(object, str): evaluates object.str.

* call(fn, list): calls the given function with 'list' as the arguments.

* abs(value) -> value: evaluates the absolute value of the value given.

* sign(value) -> value: evaluates to 1 if positive, -1 if negative, and 0 if 0.

* median(args...) -> value: evaluates to the median of the given arguments. If given a single argument list, will evaluate to the median of the member items.

* min(args...) -> value: evaluates to the minimum of the given arguments. If given a single argument list, will evaluate to the minimum of the member items.

* max(args...) -> value: evaluates to the maximum of the given arguments. If given a single argument list, will evaluate to the maximum of the member items.

* mix(x, y, ratio): equal to x*(1-ratio) + y*ratio.

* disassemble function.

* convert rgb to hsv.

* convert hsv to rgb.

* keys(map|custom_obj|level) -> list: gives the keys for a map.

* values(map) -> list: gives the values for a map.

* wave(int) -> int: a wave with a period of 1000 and height of 1000.

* decimal(value) -> decimal: converts the value to a decimal.

* int(value) -> int: converts the value to an integer.

* bool(value) -> bool: converts the value to a boolean.

* sin(x): Standard sine function.

* cos(x): Standard cosine function.

* tan(x): Standard tangent function.

* asin(x): Standard arc sine function.

* acos(x): Standard arc cosine function.

* atan(x): Standard arc tangent function.

* atan2(x,y): Standard two-param arc tangent function (to allow determining the quadrant of the resulting angle by passing in the sign value of the operands).

* sinh(x): Standard hyperbolic sine function.

* cosh(x): Standard hyperbolic cosine function.

* tanh(x): Standard hyperbolic tangent function.

* asinh(x): Standard arc hyperbolic sine function.

* acosh(x): Standard arc hyperbolic cosine function.

* atanh(x): Standard arc hyperbolic tangent function.

* sqrt(x): Returns the square root of x.

* hypot(x,y): Compute the hypotenuse of a triangle without the normal loss of precision incurred by using the pythagoream theorem.

* exp(x): Calculate the exponential function of x, whatever that means.

* angle(x1, y1, x2, y2) -> int: Returns the angle, from 0°, made by the line described by the two points (x1, y1) and (x2, y2).

* angle_delta(a, b) -> int: Given two angles, returns the smallest rotation needed to make a equal to b.

* orbit(x, y, angle, dist) -> [x,y]: Returns the point as a list containing an x/y pair which is dist away from the point as defined by x and y passed in, at the angle passed in.

* Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3.

* Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3.

* Returns the nearest integer that is even.

* Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3.

* regex_replace(string, string, string, [string] flags=[]) -> string: Unknown.

* regex_match(string, re_string) -> string: returns null if not found, else returns the whole string or a list of sub-strings depending on whether blocks were demarcated.

* unzip(list of lists) -> list of lists: Converts [[1,4],[2,5],[3,6]] -> [[1,2,3],[4,5,6]].

* float_array(list) -> callable: Converts a list of floating point values into an efficiently accessible object.

* short_array(list) -> callable: Converts a list of integer values into an efficiently accessible object.

* generate_uuid() -> string: generates a unique string.

* update_controls(map) : Updates the controls based on a list of id:string, pressed:bool pairs.

* map_controls(map) : Creates or updates the mapping on controls to keys.

* get_hex_editor_info() ->[builtin hex_tile].

* tile_pixel_pos_from_loc(loc) -> [x,y].

* tile_pixel_pos_from_loc(loc) -> [x,y].

* directed_graph(list_of_vertexes, adjacent_expression) -> a directed graph.

* weighted_graph(directed_graph, weight_expression) -> a weighted directed graph.

* a_star_search(weighted_directed_graph, src_node, dst_node, heuristic) -> A list of nodes which represents the 'best' path from src_node to dst_node.

* path_cost_search(weighted_directed_graph, src_node, max_cost) -> A list of all possible points reachable from src_node within max_cost.

* create_graph_from_level(level, (optional) tile_size_x, (optional) tile_size_y) -> directed graph : Creates a directed graph based on the current level.

* plot_path(level, from_x, from_y, to_x, to_y, heuristic, (optional) weight_expr, (optional) tile_size_x, (optional) tile_size_y) -> list : Returns a list of points to get from (from_x, from_y) to (to_x, to_y).

* shuffle(list) - Returns a shuffled version of the list. Like shuffling cards.

* remove_from_map(map, key): Removes the given key from the map and returns it.

* flatten(list): Returns a list with a depth of 1 containing the elements of any list passed in.

* unique(list): returns unique elements of list.

* binary_search(list, item) ->bool: returns true iff item is in the list. List must be sorted.

* mapping(x): Turns the args passed in into a map. The first arg is a key, the second a value, the third a key, the fourth a value and so on and so forth.

* visit_objects.

* sum(list[, counter]): Adds all elements of the list together. If counter is supplied, all elements of the list are added to the counter instead of to 0.

* range([start, ]finish[, step]): Returns a list containing all numbers smaller than the finish value and and larger than or equal to the start value. The start value defaults to 0.

* reverse(list): reverses the given list.

* head(list): gives the first element of a list, or null for an empty list.

* head_or_die(list): gives the first element of a list, or die for an empty list.

* back(list): gives the last element of a list, or null for an empty list.

* back_or_die(list): gives the last element of a list, or die for an empty list.

* get_all_files_under_dir(path): Returns a list of all the files in and under the given directory.

* get_files_in_dir(path): Returns a list of the files in the given directory.

* dialog(obj, template): Creates a dialog given an object to operate on and a template for the dialog.

* show_modal(dialog): Displays a modal dialog on the screen.

* index(list, value) -> index of value in list: Returns the index of the value in the list or -1 if value wasn't found in the list.

* CompileLua(object, string, string) Compiles a lua script against a lua-enabled object. Returns the compiled script as an object with an execute method. The second argument is the 'name' of the script as will appear in lua debugging output (normally a filename). The third argument is the script.

* eval_with_lag.

* instrument_command(string, expr): Executes expr and outputs debug instrumentation on the time it took with the given string.

* start_profiling().

* compress(string, (optional) compression_level): Compress the given string object.

* size(list).

* split(list, divider.

* str(s).

* strstr(haystack, needle).

* refcount(obj).

* deserialize(obj).

* is_string(any).

* is_null(any).

* is_int(any).

* is_bool(any).

* is_decimal(any).

* is_number(any).

* is_map(any).

* is_function(any).

* is_list(any).

* is_callable(any).

* mod(num,den).

* clear(): clears debug messages.

* log(...): outputs arguments to stderr.

* dump(msg[, expr]): evaluates and returns expr. Will print 'msg' to stderr if it's printable, or execute it if it's an executable command.

* file_backed_map(string filename, function generate_new, map initial_values).

* remove_document(string filename, [enum{game_dir}]): deletes document at the given filename.

* write_document(string filename, doc, [enum{game_dir}]): writes 'doc' to the given filename.

* get_document_from_str(string doc).

* get_document(string filename, [enum{null_on_failure,user_preferences_dir,uncached,json}] flags): return reference to the given JSON document.

* sha1(string) -> string: Returns the sha1 hash of the given string.

* get_module_args() -> callable: Returns the current module callable environment.

* seed_rng() -> none: Seeds the peudo-RNG used.

* deep_copy(any) ->any.

* lower(s) -> string: lowercase version of string.

* upper(s) -> string: lowercase version of string.

* rects_intersect([int], [int]) ->bool.

* edit_and_continue(expr, filename).

* console_output_to_screen(bool) -> none: Turns the console output to the screen on and off.

* user_preferences_path() -> string: Returns the users preferences path.

* set_user_details(string username, (opt) string password) -> none: Sets the username and password in the preferences.

* clamp(numeric value, numeric min_val, numeric max_val) -> numeric: Clamps the given value inside the given bounds.

* set_cookie(data) -> none: Sets the preferences user_data.

* get_cookie() -> none: Returns the preferences user_data.

* types_compatible(string a, string b) ->bool: returns true if type 'b' is a subset of type 'a'.

* typeof(expression) -> string: yields the statically known type of the given expression.

* static_typeof(expression) -> string: yields the statically known type of the given expression.

* all_textures().

* trigger_garbage_collection(num_gens, mandatory): trigger an FFL garbage collection.

* trigger_debug_garbage_collection(): trigger an FFL garbage collection with additional memory usage information.

* objects_known_to_gc().

* destroy_object_references(obj).

* debug_object_info(string) -> give info about the object at the given address.

* build_animation(map).

* inspect_object(object obj) -> map: outputs an object's properties.

* is_simulation(): returns true iff we are in a 'simulation' such as get_modified_objcts() or eval_with_temp_modifications().

* get_modified_object(obj, commands) -> obj: yields a copy of the given object modified by the given commands.

* eval_with_temp_modifications.

* release_object(obj).

* DrawPrimitive(map): create and return a DrawPrimitive.

* auto_update_info(): get info on auto update status.

* rotate_rect(int|decimal center_x, int|decimal center_y, decimal rotation, int|decimal[8] rect) -> int|decimal[8]: rotates rect and returns the result.

* points_along_curve([[decimal,decimal]], int) -> [[decimal,decimal]].

* solid(level, int x, int y, (optional)int w=1, (optional) int h=1, (optional) bool debug=false) -> boolean: returns true iff the level contains solid space within the given (x,y,w,h) rectangle. If 'debug' is set, then the tested area will be displayed on-screen.

* solid_grid(level, int x, int y, int w, int h, int stride_x=1, int stride_y=1, int stride_w=1, int stride_h=1).

* hsv(decimal h, decimal s, decimal v, decimal alpha) -> color_callable.

* format(string, [int|decimal]): Put the numbers in the list into the string. The fractional component of the number will be rounded to the nearest available digit. Example: format('#{01}/#{02}/#{2004}', [20, 5, 2015]) → '20/05/2015'; format('#{02}/#{02}/#{02}', [20, 5, 2015]) → '20/5/2015'; format(#{0.20}, [0.1]) → '0.10'; format(#{0.02}, [0.1]) → '0.1'.

* sprintf(string, ...): Format the string using standard printf formatting.

* anura_objects().

