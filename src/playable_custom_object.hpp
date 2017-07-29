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

#include "controls.hpp"
#include "custom_object.hpp"
#include "player_info.hpp"
#include "variant.hpp"
#include "widget_fwd.hpp"

class Level;

class PlayableCustomObject : public CustomObject
{
public:
	PlayableCustomObject(const CustomObject& obj);
	PlayableCustomObject(const PlayableCustomObject& obj);
	PlayableCustomObject(variant node);
	PlayableCustomObject(const std::string& type, int x, int y, bool face_right, bool deferInitProperties = false);

	virtual variant write() const override;

	virtual PlayerInfo* isHuman() override { return &player_info_; }
	virtual const PlayerInfo* isHuman() const override { return &player_info_; }

	void saveGame() override;
	EntityPtr saveCondition() const override { return save_condition_; }

	virtual EntityPtr backup() const override;
	virtual EntityPtr clone() const override;

	virtual int verticalLook() const override { return vertical_look_; }

	virtual bool isActive(const rect& screen_area) const override;

	bool canInteract() const { return can_interact_ != 0; }

	int difficulty() const { return difficulty_; }

	// XXX These would be much better served as taking weak pointers.
	static void registerKeyboardOverrideWidget(gui::Widget* widget);
	static void unregisterKeyboardOverrideWidget(gui::Widget* widget);
protected:
	void surrenderReferences(GarbageCollector* collector) override;

private:
	bool onPlatform() const;

	int walkUpOrDownStairs() const override;

	virtual void process(Level& lvl) override;
	variant getValue(const std::string& key) const override;
	void setValue(const std::string& key, const variant& value) override;

	variant getPlayerValueBySlot(int slot) const override;
	void setPlayerValueBySlot(int slot, const variant& value) override;

	PlayerInfo player_info_;

	int difficulty_;

	EntityPtr save_condition_;

	int vertical_look_;

	int underwater_ctrl_x_, underwater_ctrl_y_;

	bool underwater_controls_;

	int can_interact_;
	
	std::unique_ptr<controls::local_controls_lock> control_lock_;

	variant ctrl_keys_, prev_ctrl_keys_;

	variant getCtrlKeys() const;

	void operator=(const PlayableCustomObject&);
};
