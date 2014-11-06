/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

     1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/
#ifndef EXTERNAL_TEXT_EDITOR_HPP_INCLUDED
#define EXTERNAL_TEXT_EDITOR_HPP_INCLUDED
#ifndef NO_EDITOR

#include <string>

#include <boost/shared_ptr.hpp>

#include "asserts.hpp"
#include "variant.hpp"

class external_text_editor;

typedef boost::shared_ptr<external_text_editor> external_text_editor_ptr;

class external_text_editor
{
public:
	struct manager {
		manager();
		~manager();
	};

	static external_text_editor_ptr create(variant key);

	external_text_editor();
	virtual ~external_text_editor();

	void process();

	bool replace_in_game_editor() const { return replace_in_game_editor_; }
	
	virtual void load_file(const std::string& fname) = 0;
	virtual void shutdown() = 0;
protected:
	struct editor_error {};
private:
	external_text_editor(const external_text_editor&);
	virtual std::string get_file_contents(const std::string& fname) = 0;
	virtual int get_line(const std::string& fname) const = 0;
	virtual std::vector<std::string> loaded_files() const = 0;

	bool replace_in_game_editor_;

	//As long as there's one of these things active, we're dynamically loading
	//in code, and so want to recover from asserts.
	assert_recover_scope assert_recovery_;
};

#endif // NO_EDITOR
#endif
