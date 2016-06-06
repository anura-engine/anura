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

#ifndef DISABLE_FORMULA_PROFILER

#include <glm/glm.hpp>

#include <SDL_thread.h>
#include <SDL_timer.h>

#include <assert.h>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <cstdint>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#if !defined(_MSC_VER)
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_profiler.hpp"
#include "level_runner.hpp"
#include "object_events.hpp"
#include "preferences.hpp"
#include "variant.hpp"
#include "widget.hpp"

#include "kre/Canvas.hpp"
#include "kre/Color.hpp"
#include "kre/Font.hpp"
#include "kre/ShadersOGL.hpp"

void init_call_stack(int min_size);

namespace {

PREF_STRING(profile_widget_area, "[20,20,1000,200]", "Area of the profile widget");
PREF_STRING(profile_widget_details_area, "[20,240,1000,400]", "Area of the profile widget");

uint64_t g_first_second;

uint64_t tv_to_us(const timeval& tv) {
	return uint64_t((tv.tv_sec - g_first_second)*1000000LL) + uint64_t(tv.tv_usec);
}

using namespace gui;
using namespace KRE;

struct InstrumentationNode {
	InstrumentationNode() : id(nullptr) {}
	~InstrumentationNode() {
		for(auto p : records) {
			delete p;
		}
	}
	std::vector<InstrumentationNode*> records;
	const char* id;
	uint64_t begin_time, end_time;

	variant info;
};

const char* LegendColors[] = { "lightgreen", "magenta", "cyan", "orange", "darkblue", "salmon", "green", "yellow", "crimson"};
const int NumLegendColors = sizeof(LegendColors)/sizeof(*LegendColors);

class FrameDetailsWidget : public Widget
{
	Color white_color_;
	const InstrumentationNode* node_;
	const InstrumentationNode* selected_node_;

	TexturePtr selected_node_text_;

	struct NodeRegion {
		rect area;
		const InstrumentationNode* node;
	};

	std::vector<NodeRegion> regions_;

	std::map<const char*, Color> id_to_color_;
	std::map<const char*, TexturePtr> id_to_texture_;

	int calculateColors(const InstrumentationNode* node, int index=0, int depth=1) {
		const int x1 = calculateX(node->begin_time);
		const int x2 = calculateX(node->end_time);
		const int y1 = y() + height() - depth*20;

		rect area(x1, y1, x2-x1, 20);
		NodeRegion region = { area, node };
		regions_.push_back(region);

		if(id_to_color_.count(node->id) == 0) {
			Color color;
			
			if(node->id == nullptr) {
				color = Color("gray");
			} else if(strcmp(node->id,"DRAW") == 0) {
				color = Color("yellow");
			} else if(strcmp(node->id,"LEVEL_PROCESS") == 0) {
				color = Color("red");
			} else if(strcmp(node->id,"SLEEP") == 0) {
				color = Color("lightblue");
			} else {
				color = Color(LegendColors[index%NumLegendColors]);
				++index;
			}
			id_to_color_[node->id] = color;
			id_to_texture_[node->id] = Font::getInstance()->renderText(node->id ? node->id : "Frame", white_color_, 12, true, Font::get_default_monospace_font());
		}

		for(auto record : node->records) {
			index = calculateColors(record, index, depth+1);
		}

		return index;
	}
public:
	explicit FrameDetailsWidget(const InstrumentationNode* node) : white_color_("white"), node_(node), selected_node_(nullptr) {

		variant area = game_logic::Formula(variant(g_profile_widget_details_area)).execute();
		std::vector<int> area_int = area.as_list_int();
		ASSERT_LOG(area_int.size() == 4, "--profile-widget-area must have four integers");
		setLoc(area_int[0], area_int[1]);
		setDim(area_int[2], area_int[3]);

		calculateColors(node);
	}

	int calculateX(uint64_t t) const {
		return x() + int((double(t - node_->begin_time)/double(node_->end_time - node_->begin_time))*width());
	}

	void drawNode(const InstrumentationNode* node, int depth) const {
		const int x1 = calculateX(node->begin_time);
		const int x2 = calculateX(node->end_time);

		auto color_itor = id_to_color_.find(node->id);
		ASSERT_LOG(color_itor != id_to_color_.end(), "Unknown color: " << node->id);

		Canvas& c = *Canvas::getInstance();
		c.drawSolidRect(rect(x1, y() + height() - depth*20, x2 - x1, 20), selected_node_ == node ? white_color_ : color_itor->second);
		c.drawSolidRect(rect(x1, y() + height() - depth*20, 1, 20), white_color_);
		c.drawSolidRect(rect(x2, y() + height() - depth*20, 1, 20), white_color_);

		for(auto record : node->records) {
			drawNode(record, depth+1);
		}
	}

	void handleDraw() const override {
		Canvas& c = *Canvas::getInstance();

		c.drawSolidRect(rect(x(), y(), width(), height()), Color("black"));
		drawNode(node_, 1);

		int n = 0;
		for(auto p : id_to_color_) {
			auto itor = id_to_texture_.find(p.first);
			c.blitTexture(itor->second, 0, x() + 25, 4 + y() + n*16, white_color_);
			c.drawSolidRect(rect(x()+10, 4 + y() + n*16, 10, 10), p.second);
			++n;
		}

		if(selected_node_text_.get() != nullptr) {
			c.blitTexture(selected_node_text_, 0, x() + 140, y() + 5, white_color_);
		}
	}

	bool handleEvent(const SDL_Event& event, bool claimed) override {
		switch(event.type) {
		case SDL_MOUSEWHEEL:
			break;
		case SDL_MOUSEMOTION: {
			const SDL_MouseMotionEvent& motion = event.motion;
			selected_node_ = nullptr;
			selected_node_text_.reset();
			for(const NodeRegion& region : regions_) {
				if(motion.x >= region.area.x() && motion.y >= region.area.y() && motion.x < region.area.x2() && motion.y < region.area.y2()) {
					selected_node_ = region.node;
					selected_node_text_ = calculateNodeText(region.node);
				}
			}
			break;
		}
		}

		return false;
	}

	WidgetPtr clone() const override {
		return WidgetPtr(new FrameDetailsWidget(node_));
	}

	TexturePtr calculateNodeText(const InstrumentationNode* node) const {

		std::string text = formatter() << (node->id ? node->id : "Frame") << ": " << (node->end_time - node->begin_time) << "us";

		auto info = node->info.get_debug_info();
		if(info) {
			text += formatter() << " " << *info->filename << ":" << info->line;
		} else if(node->info.is_null() == false) {
			text += formatter() << " " << node->info.write_json();
		}

		return Font::getInstance()->renderText(text, white_color_, 12, true, Font::get_default_monospace_font());
	}
};

class BarGraphRenderable : public SceneObject
{
	std::shared_ptr<KRE::Attribute<glm::u16vec2>> r_;

	std::vector<glm::u16vec2> vertices_;
public:
	BarGraphRenderable() : SceneObject("BarGraphRenderable")
	{
		setShader(ShaderProgram::getProgram("simple"));
		auto ab = DisplayDevice::createAttributeSet(true, false, false);
		r_ = std::make_shared<Attribute<glm::u16vec2>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
		r_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::SHORT, false));
		ab->addAttribute(r_);

		ab->setDrawMode(DrawMode::TRIANGLES);
		addAttributeSet(ab);

		ab->setBlendState(false);
	}

	bool empty() const { return vertices_.empty(); }

	void addRect(const rect& r) {
		vertices_.emplace_back(glm::u16vec2(r.x(), r.y()));
		vertices_.emplace_back(glm::u16vec2(r.x() + r.w(), r.y()));
		vertices_.emplace_back(glm::u16vec2(r.x(), r.y() + r.h()));

		vertices_.emplace_back(glm::u16vec2(r.x() + r.w(), r.y()));
		vertices_.emplace_back(glm::u16vec2(r.x() + r.w(), r.y() + r.h()));
		vertices_.emplace_back(glm::u16vec2(r.x(), r.y() + r.h()));
	}

	void prepareDraw() {
		r_->update(&vertices_);
	}
};

class ProfilerWidget : public Widget
{
	std::vector<InstrumentationNode*> frames_;

	std::vector<InstrumentationNode*> instrumentation_stack_;

	Color gray_color_, yellow_color_, green_color_, blue_color_, red_color_, white_color_;

	bool paused_;
	int selected_frame_;

	FrameDetailsWidget* details_;

	TexturePtr draw_text_, process_text_, sleep_text_;

	TexturePtr frame_text_;

public:
	ProfilerWidget() :
	  gray_color_("gray"), yellow_color_("yellow"), green_color_("green"), blue_color_("lightblue"), red_color_("red"), white_color_("white"), paused_(false), selected_frame_(-1), details_(nullptr) {

		variant area = game_logic::Formula(variant(g_profile_widget_area)).execute();
		std::vector<int> area_int = area.as_list_int();
		ASSERT_LOG(area_int.size() == 4, "--profile-widget-area must have four integers");
		setLoc(area_int[0], area_int[1]);
		setDim(area_int[2], area_int[3]);

		instrumentation_stack_.push_back(nullptr);
		newFrame();

		draw_text_ = Font::getInstance()->renderText("Draw", white_color_, 16, true, Font::get_default_monospace_font());
		process_text_ = Font::getInstance()->renderText("Process", white_color_, 16, true, Font::get_default_monospace_font());
		sleep_text_ = Font::getInstance()->renderText("Sleep", white_color_, 16, true, Font::get_default_monospace_font());
	}

	~ProfilerWidget() {
		for(auto p : frames_) {
			delete p;
		}
	}

	int barWidth() const { return 4; }

	size_t firstDisplayedFrame() const {

		const int max_frames = width()/barWidth();
		size_t begin_frame = 0;
		if(max_frames <= frames_.size()) {
			begin_frame = frames_.size() - max_frames;
		}

		return begin_frame;
	}

	void handleDraw() const override {
		if(details_) {
			details_->draw();
		}

		if(frames_.empty()) {
			return;
		}

		Canvas& c = *Canvas::getInstance();

		c.drawSolidRect(rect(x(), y(), width(), height()), Color("black"));

		const int max_frames = width()/barWidth();

		const int us_per_pixel = 200;

		size_t begin_frame = firstDisplayedFrame();

		BarGraphRenderable renderables[5];
		renderables[0].setColor(gray_color_);
		renderables[1].setColor(red_color_);
		renderables[2].setColor(blue_color_);
		renderables[3].setColor(yellow_color_);
		renderables[4].setColor(white_color_);

		for(size_t i = begin_frame; i < frames_.size()-1; ++i) {
			const InstrumentationNode& f = *frames_[i];

			uint64_t elapsed = f.end_time - f.begin_time;

			const int bar_height = elapsed/us_per_pixel;

			rect area(x() + (i-begin_frame)*barWidth(), y() + height() - bar_height, barWidth(), bar_height);
			renderables[0].addRect(area);

			for(auto* node : f.records) {
				if(node->end_time <= node->begin_time) {
					continue;
				}

				auto* renderable = &renderables[1];

				if(strcmp(node->id, "SLEEP") == 0) {
					renderable = &renderables[2];
				} else if(strcmp(node->id, "DRAW") == 0) {
					renderable = &renderables[3];
				}

				if(i == selected_frame_) {
					renderable = &renderables[4];
				}

				const uint64_t begin_pos = node->begin_time - f.begin_time;
				const uint64_t len = node->end_time - node->begin_time;

				rect area(x() + (i-begin_frame)*barWidth(), y() + height() - (begin_pos+len)/us_per_pixel, barWidth(), len/us_per_pixel);

				renderable->addRect(area);
			}
		}

		for(int i = 1; i != 5; ++i) {
			auto& renderable = renderables[i];
			if(renderable.empty() == false) {
				renderable.prepareDraw();
				auto wnd = KRE::WindowManager::getMainWindow();
				renderable.preRender(wnd);
				wnd->render(&renderable);
			}
		}

		c.drawSolidRect(rect(x(), y() + height() - (preferences::frame_time_millis()*1000)/us_per_pixel, width(), 1), Color("white"));

		c.drawSolidRect(rect(x()+13, y()+8, 10, 10), yellow_color_);
		c.blitTexture(draw_text_, 0, x() + 25, y() + 5, white_color_);
		c.drawSolidRect(rect(x()+13, y()+23, 10, 10), red_color_);
		c.blitTexture(process_text_, 0, x() + 25, y() + 20, white_color_);
		c.drawSolidRect(rect(x()+13, y()+38, 10, 10), blue_color_);
		c.blitTexture(sleep_text_, 0, x() + 25, y() + 35, white_color_);

		if(frame_text_.get() != nullptr) {
			c.blitTexture(frame_text_, 0, x() + width() - 400, y() + 5, white_color_);
		}
	}

	TexturePtr calculateFrameText(int nframe) {
		InstrumentationNode* node = frames_[nframe];
		uint64_t draw_time = 0, process_time = 0;

		for(auto record : node->records) {
			if(strcmp(record->id, "DRAW") == 0) {
				draw_time += record->end_time - record->begin_time;
			} else {
				process_time += record->end_time - record->begin_time;
			}
		}

		std::string text = (formatter() << "Frame " << nframe << ": " << (node->end_time - node->begin_time) << "us: " << draw_time << "us draw; " << process_time << "us process");
		return Font::getInstance()->renderText(text, white_color_, 12, true, Font::get_default_monospace_font());
	}

	bool handleEvent(const SDL_Event& event, bool claimed) override {
		if(paused_ == false) {
			return false;
		}

		if(details_) {
			claimed = details_->processEvent(point(), event, claimed) || claimed;
		}

		switch(event.type) {
		case SDL_MOUSEWHEEL:
			break;
		case SDL_MOUSEMOTION: {
			const SDL_MouseMotionEvent& motion = event.motion;
			if(motion.x >= x() && motion.y >= y() && motion.x < x() + width() && motion.y < y() + height()) {
				const int bar = firstDisplayedFrame() + (motion.x - x())/barWidth();
				if(bar >= 0 && bar < frames_.size()) {
					selected_frame_ = bar;
					frame_text_ = calculateFrameText(bar);
				} else {
					selected_frame_ = -1;
					frame_text_.reset();
				}
			} else {
				selected_frame_ = -1;
				frame_text_.reset();
			}
			return claimed;
		}

		case SDL_MOUSEBUTTONDOWN: {
			const SDL_MouseButtonEvent& e = event.button;
			if(selected_frame_ != -1 && selected_frame_ < frames_.size() &&
			   e.x >= x() && e.y >= y() && e.x < x() + width() && e.y < y() + height()) {
				selectFrame(selected_frame_);
				return true;
			}

			break;
		}

		}
		return claimed;
	}

	void selectFrame(int nframe) {
		delete details_;
		details_ = new FrameDetailsWidget(frames_[selected_frame_]);
	}

	WidgetPtr clone() const override {
		return WidgetPtr(new ProfilerWidget);
	}

	void newFrame() {
		if(LevelRunner::getCurrent() && LevelRunner::getCurrent()->is_paused()) {
			paused_ = true;
			instrumentation_stack_.clear();
			instrumentation_stack_.push_back(nullptr);
			return;
		}

		if(details_) {
			delete details_;
			details_ = nullptr;
		}

		paused_ = false;

		ASSERT_LOG(instrumentation_stack_.size() == 1, "Incorrect instrumentation stack size: " << instrumentation_stack_.size());

		InstrumentationNode* last_frame = instrumentation_stack_.back();
		instrumentation_stack_.pop_back();

		struct timeval tv;
		gettimeofday(&tv, nullptr);

		uint64_t t = tv_to_us(tv);

		if(last_frame) {
			frames_.push_back(last_frame);
			last_frame->end_time = t;
		}

		InstrumentationNode* new_frame = new InstrumentationNode;
		new_frame->begin_time = t;

		instrumentation_stack_.push_back(new_frame);
	}

	void beginInstrument(const char* id, const timeval& tv, const variant& info)
	{
		ASSERT_LOG(instrumentation_stack_.empty() == false, "No instrumentation stack: " << id);

		if(instrumentation_stack_.back() == nullptr) {
			return;
		}

		uint64_t t = tv_to_us(tv);

		InstrumentationNode* node = new InstrumentationNode;
		node->id = id;
		node->begin_time = t;
		node->info = info;

		instrumentation_stack_.back()->records.push_back(node);
		instrumentation_stack_.push_back(node);
	}

	void endInstrument(const char* id, const timeval& tv)
	{
		if(instrumentation_stack_.back() == nullptr) {
			return;
		}

		ASSERT_LOG(instrumentation_stack_.empty() == false, "No instrumentation stack: " << id);
		ASSERT_LOG(instrumentation_stack_.back()->id == id, "Instrumentation stack mismatch: " << id << " vs " << instrumentation_stack_.back()->id);

		instrumentation_stack_.back()->end_time = tv_to_us(tv);
		instrumentation_stack_.pop_back();
		ASSERT_LOG(instrumentation_stack_.empty() == false, "No instrumentation stack: " << id);
	}
};

ProfilerWidget* g_profiler_widget;

}

namespace formula_profiler
{
	bool profiler_on = false;
	namespace 
	{
		struct InstrumentationRecord 
		{
			InstrumentationRecord() : time_us(0), nsamples(0)
			{}
			int time_us, nsamples;
		};

		std::map<const char*, InstrumentationRecord> g_instrumentation;
	}

	Instrument::Instrument() : id_(nullptr)
	{
	}

	Instrument::Instrument(const char* id, const game_logic::Formula* formula) : id_(id)
	{
		if(profiler_on) {
			gettimeofday(&tv_, nullptr);
			if(g_profiler_widget) {
				g_profiler_widget->beginInstrument(id, tv_, formula ? formula->strVal() : variant());
			}
		}
	}

	void Instrument::init(const char* id, variant info)
	{
		if(profiler_on) {
			id_ = id;
			gettimeofday(&tv_, nullptr);
			if(g_profiler_widget) {
				g_profiler_widget->beginInstrument(id, tv_, info);
			}
		}
	}

	Instrument::~Instrument()
	{
		if(profiler_on) {
			struct timeval end_tv;
			gettimeofday(&end_tv, nullptr);
			InstrumentationRecord& r = g_instrumentation[id_];
			r.time_us += (end_tv.tv_sec - tv_.tv_sec)*1000000 + (end_tv.tv_usec - tv_.tv_usec);
			r.nsamples++;
			if(g_profiler_widget) {
				g_profiler_widget->endInstrument(id_, end_tv);
			}
		}
	}

	void dump_instrumentation()
	{
		static struct timeval prev_call;
		static bool first_call = true;

		struct timeval tv;
		gettimeofday(&tv, nullptr);

		if(!first_call && g_instrumentation.empty() == false) {
			const int time_us = (tv.tv_sec - prev_call.tv_sec)*1000000 + (tv.tv_usec - prev_call.tv_usec);
			if(time_us) {
				std::ostringstream ss;
				ss << "FRAME INSTRUMENTATION TOTAL TIME: " << time_us << "us. INSTRUMENTS: ";
				for(std::map<const char*,InstrumentationRecord>::const_iterator i = g_instrumentation.begin(); i != g_instrumentation.end(); ++i) {
					const int percent = (i->second.time_us*100)/time_us;
					ss << i->first << ": " << i->second.time_us << "us (" << percent << "%) in " << i->second.nsamples << " calls; ";
				}
				LOG_INFO(ss.str());
			}

			g_instrumentation.clear();
		}

		first_call = false;
		prev_call = tv;
	}

	EventCallStackType event_call_stack;

	namespace 
	{
		bool handler_disabled = false;
		std::string output_fname;
		SDL_threadID main_thread;

		int empty_samples = 0;

		std::map<std::vector<CallStackEntry>, int> expression_call_stack_samples;
		std::vector<CallStackEntry> current_expression_call_stack;

		std::vector<CustomObjectEventFrame> event_call_stack_samples;
		int num_samples = 0;
		const size_t max_samples = 10000;

		int nframes_profiled = 0;

#if defined(_MSC_VER) || MOBILE_BUILD
		SDL_TimerID sdl_profile_timer;
#endif

#if defined(_MSC_VER) || MOBILE_BUILD
		Uint32 sdl_timer_callback(Uint32 interval, void *param)
#else
		void sigprof_handler(int sig)
#endif
		{
			//NOTE: Nothing in this function should allocate memory, since
			//we might be called while allocating memory.
#if defined(_MSC_VER) || MOBILE_BUILD
			if(handler_disabled) {
				return interval;
			}
#else
			if(handler_disabled || main_thread != SDL_ThreadID()) {
				return;
			}
#endif

			if(current_expression_call_stack.empty() && current_expression_call_stack.capacity() >= get_expression_call_stack().size()) {
				bool valid = true;

				//Very important that this does not allocate memory.
				if(valid) {
					current_expression_call_stack = get_expression_call_stack();

					for(int n = 0; n != current_expression_call_stack.size(); ++n) {
						if(current_expression_call_stack[n].expression == nullptr) {
							valid = false;
							break;
						}

						intrusive_ptr_add_ref(current_expression_call_stack[n].expression);
					}

					if(!valid) {
						current_expression_call_stack.clear();
					}
				}
			}

			if(num_samples == max_samples) {
#if defined(_MSC_VER) || MOBILE_BUILD
				return interval;
#else
				return;
#endif
			}

			if(event_call_stack.empty()) {
				++empty_samples;
			} else {
				event_call_stack_samples[num_samples++] = event_call_stack.back();
			}
	#if defined(_MSC_VER) || MOBILE_BUILD
			return interval;
	#endif
		}
	}

	namespace {
		Manager* manager_instance;
	}

	Manager::Manager(const char* output_file)
	{
		manager_instance = this;
		init(output_file);
	}

	Manager* Manager::get()
	{
		return manager_instance;
	}

	void Manager::init(const char* output_file)
	{
		if(output_file && profiler_on == false) {
			current_expression_call_stack.reserve(10000);
			event_call_stack_samples.resize(max_samples);

			main_thread = SDL_ThreadID();

			LOG_INFO("SETTING UP PROFILING: " << output_file);
			profiler_on = true;
			output_fname = output_file;

			if(g_first_second == 0) {
				struct timeval tv;
				gettimeofday(&tv, nullptr);
				g_first_second = tv.tv_sec;
			}

			init_call_stack(65536);

#if defined(_MSC_VER) || MOBILE_BUILD
			// Crappy windows approximation.
			sdl_profile_timer = SDL_AddTimer(10, sdl_timer_callback, 0);
			if(sdl_profile_timer == 0) {
				LOG_WARN("Couldn't create a profiling timer!");
			}
#else
			signal(SIGPROF, sigprof_handler);

			struct itimerval timer;
			timer.it_interval.tv_sec = 0;
			timer.it_interval.tv_usec = 10000;
			timer.it_value = timer.it_interval;
			setitimer(ITIMER_PROF, &timer, 0);
#endif

			g_profiler_widget = new ProfilerWidget;
		}
	}

	Manager::~Manager()
	{
		end_profiling();

		delete g_profiler_widget;
		g_profiler_widget = nullptr;
	}

	void end_profiling()
	{
		LOG_INFO("END PROFILING: " << (int)profiler_on);
		if(profiler_on) {
#if defined(_MSC_VER) || MOBILE_BUILD
			SDL_RemoveTimer(sdl_profile_timer);
#else
			struct itimerval timer;
			memset(&timer, 0, sizeof(timer));
			setitimer(ITIMER_PROF, &timer, 0);
#endif

			std::map<std::string, int> samples_map;

			for(int n = 0; n != num_samples; ++n) {
				const CustomObjectEventFrame& frame = event_call_stack_samples[n];

				std::string str = formatter() << frame.type->id() << ":" << get_object_event_str(frame.event_id) << ":" << (frame.executing_commands ? "CMD" : "FFL");

				samples_map[str]++;
			}

			std::vector<std::pair<int, std::string> > sorted_samples, cum_sorted_samples;
			for(std::map<std::string, int>::const_iterator i = samples_map.begin(); i != samples_map.end(); ++i) {
				sorted_samples.push_back(std::pair<int, std::string>(i->second, i->first));
			}

			std::sort(sorted_samples.begin(), sorted_samples.end());
			std::reverse(sorted_samples.begin(), sorted_samples.end());

			const int total_samples = empty_samples + num_samples;
			if(!total_samples) {
				return;
			}

			std::ostringstream s;
			s << "TOTAL SAMPLES: " << total_samples << "\n";
			s << (100*empty_samples)/total_samples << "% (" << empty_samples << ") CORE ENGINE (non-FFL processing)\n";

			for(int n = 0; n != sorted_samples.size(); ++n) {
				s << (100*sorted_samples[n].first)/total_samples << "% (" << sorted_samples[n].first << ") " << sorted_samples[n].second << "\n";
			}

			sorted_samples.clear();

			std::map<const game_logic::FormulaExpression*, int> expr_samples, cum_expr_samples;

			int total_expr_samples = 0;

			for(std::map<std::vector<CallStackEntry>, int>::const_iterator i = expression_call_stack_samples.begin(); i != expression_call_stack_samples.end(); ++i) {
				const std::vector<CallStackEntry>& sample = i->first;
				const int nsamples = i->second;
				if(sample.empty()) {
					continue;
				}

				for(const CallStackEntry& entry : sample) {
					cum_expr_samples[entry.expression] += nsamples;
				}

				expr_samples[sample.back().expression] += nsamples;

				total_expr_samples += nsamples;
			}

			for(std::map<const game_logic::FormulaExpression*, int>::const_iterator i = expr_samples.begin(); i != expr_samples.end(); ++i) {
				sorted_samples.push_back(std::pair<int, std::string>(i->second, formatter() << i->first->debugPinpointLocation() << " (called " << double(i->first->getNTimesCalled())/double(nframes_profiled) << " times per frame)"));
			}

			for(std::map<const game_logic::FormulaExpression*, int>::const_iterator i = cum_expr_samples.begin(); i != cum_expr_samples.end(); ++i) {
				cum_sorted_samples.push_back(std::pair<int, std::string>(i->second, formatter() << i->first->debugPinpointLocation() << " (called " << double(i->first->getNTimesCalled())/double(nframes_profiled) << " times per frame)"));
			}

			std::sort(sorted_samples.begin(), sorted_samples.end());
			std::reverse(sorted_samples.begin(), sorted_samples.end());

			std::sort(cum_sorted_samples.begin(), cum_sorted_samples.end());
			std::reverse(cum_sorted_samples.begin(), cum_sorted_samples.end());

			s << "\n\nPROFILE BROKEN DOWN INTO FFL EXPRESSIONS:\n\nTOTAL SAMPLES: " << total_expr_samples << "\n OVER " << nframes_profiled << " FRAMES\nSELF TIME:\n";

			for(int n = 0; n != sorted_samples.size(); ++n) {
				s << (100*sorted_samples[n].first)/total_expr_samples << "% (" << sorted_samples[n].first << ") " << sorted_samples[n].second << "\n";
			}

			s << "\n\nCUMULATIVE TIME:\n";
			for(int n = 0; n != cum_sorted_samples.size(); ++n) {
				s << (100*cum_sorted_samples[n].first)/total_expr_samples << "% (" << cum_sorted_samples[n].first << ") " << cum_sorted_samples[n].second << "\n";
			}

			if(!output_fname.empty()) {
				sys::write_file(output_fname, s.str());
				LOG_INFO("WROTE PROFILE TO " << output_fname);
			} else {
				LOG_INFO("===\n=== PROFILE REPORT ===");
				LOG_INFO(s.str());
				LOG_INFO("=== END PROFILE REPORT ===");
			}

			profiler_on = false;
		}
	}

	void pump()
	{
		static int instr_count = 0;
		if(++instr_count%50 == 0) {
			dump_instrumentation();
		}

		if(current_expression_call_stack.empty() == false) {
			expression_call_stack_samples[current_expression_call_stack]++;
			current_expression_call_stack.clear();
		}

		++nframes_profiled;

		if(g_profiler_widget != nullptr) {
			g_profiler_widget->process();
			g_profiler_widget->newFrame();
		}
	}

	void draw()
	{
		if(g_profiler_widget != nullptr) {
			g_profiler_widget->draw();
		}
	}

	bool handle_sdl_event(const SDL_Event& event, bool claimed)
	{
		if(g_profiler_widget != nullptr) {
			return g_profiler_widget->processEvent(point(), event, claimed);
		}
		return false;
	}

	bool CustomObjectEventFrame::operator<(const CustomObjectEventFrame& f) const
	{
		return type < f.type || (type == f.type && event_id < f.event_id) ||
			   (type == f.type && event_id == f.event_id && executing_commands < f.executing_commands);
	}

	std::string get_profile_summary()
	{
		if(!profiler_on) {
			return "";
		}

		handler_disabled = true;

		static int last_empty_samples = 0;
		static int last_num_samples = 0;

		const int nsamples = num_samples - last_num_samples;
		const int nempty = empty_samples - last_empty_samples;

		std::sort(event_call_stack_samples.end() - nsamples, event_call_stack_samples.end());

		std::ostringstream s;

		s << "PROFILE: " << (nsamples + nempty) << " CPU. " << nsamples << " IN FFL ";


		std::vector<std::pair<int, std::string> > samples;
		int count = 0;
		for(int n = last_num_samples; n < num_samples; ++n) {
			if(n+1 == num_samples || event_call_stack_samples[n].type != event_call_stack_samples[n+1].type) {
				samples.push_back(std::pair<int, std::string>(count + 1, event_call_stack_samples[n].type->id()));
				count = 0;
			} else {
				++count;
			}
		}

		std::sort(samples.begin(), samples.end());
		std::reverse(samples.begin(), samples.end());
		for(int n = 0; n != samples.size(); ++n) {
			s << samples[n].second << " " << samples[n].first << " ";
		}

		last_empty_samples = empty_samples;
		last_num_samples = num_samples;

		handler_disabled = false;

		return s.str();
	}

}

#endif
