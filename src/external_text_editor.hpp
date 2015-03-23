/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#pragma once

#ifndef NO_EDITOR

#include <string>

#include "asserts.hpp"
#include "variant.hpp"

class ExternalTextEditor;

typedef std::shared_ptr<ExternalTextEditor> ExternalTextEditorPtr;

class ExternalTextEditor
{
public:
	struct Manager {
		Manager();
		~Manager();
	};

	static ExternalTextEditorPtr create(variant key);

	virtual ~ExternalTextEditor();

	void process();

	bool replaceInGameEditor() const { return replace_in_game_editor_; }
	
	virtual void loadFile(const std::string& fname) = 0;
	virtual void shutdown() = 0;
protected:
	ExternalTextEditor();
	struct EditorError {};
private:
	ExternalTextEditor(const ExternalTextEditor&);
	void operator=(const ExternalTextEditor&);

	virtual std::string getFileContents(const std::string& fname) = 0;
	virtual int getLine(const std::string& fname) const = 0;
	virtual std::vector<std::string> getLoadedFiles() const = 0;

	bool replace_in_game_editor_;

	//As long as there's one of these things active, we're dynamically loading
	//in code, and so want to recover from asserts.
	assert_recover_scope assert_recovery_;
};

#endif // NO_EDITOR
