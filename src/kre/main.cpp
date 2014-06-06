#include <cstddef>
#include <deque>
#include <list>
#include <vector>

#include <glm/gtc/type_precision.hpp>

#include "AttributeSet.hpp"
#include "json.hpp"
#include "logger.hpp"
#include "profile_timer.hpp"
#include "SDLWrapper.hpp"
#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "LightObject.hpp"
#include "ParticleSystem.hpp"
#include "Renderable.hpp"
#include "RenderManager.hpp"
#include "RenderQueue.hpp"
#include "RenderTarget.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "WindowManager.hpp"
#include "VGraph.hpp"

namespace
{	
	struct vertex_color
	{
		vertex_color() : vertex(0.0f), color(0) {}
		vertex_color(const glm::vec2& v, const glm::u8vec4& c) : vertex(v), color(c) {}
		glm::vec2 vertex;
		glm::u8vec4 color;
	};

	class SquareRenderable : public KRE::SceneObject
	{
	public:
		SquareRenderable() : KRE::SceneObject("square") {
			using namespace KRE;

			auto ab = DisplayDevice::CreateAttributeSet(false, false, false);
			auto pc = new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
			pc->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::POSITION, 2, AttributeDesc::VariableType::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
			pc->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::COLOR, 4, AttributeDesc::VariableType::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
			ab->AddAttribute(AttributeBasePtr(pc));
			ab->SetDrawMode(AttributeSet::DrawMode::TRIANGLE_STRIP);
			AddAttributeSet(ab);

			std::vector<vertex_color> vertices;
			vertices.emplace_back(glm::vec2(0.0f,0.0f), glm::u8vec4(255,0,0,255));
			vertices.emplace_back(glm::vec2(0.0f,100.0f), glm::u8vec4(0,255,0,255));
			vertices.emplace_back(glm::vec2(100.0f,0.0f), glm::u8vec4(0,0,255,255));
			vertices.emplace_back(glm::vec2(100.0f,100.0f), glm::u8vec4(255,0,0,255));
			ab->SetCount(vertices.size());
			pc->Update(&vertices);

			//std::vector<uint8_t> indices;
			//indices.emplace_back(0);
			//indices.emplace_back(1);
			//indices.emplace_back(2);
			//indices.emplace_back(3);
			//ab->UpdateIndicies(indices);

			//pc->Update(vertices, pc->begin()+5);

			/*
			// first parameter is a hint to wether the buffer should be hardware backed.
			// second parameter is whether we are using indexed drawing.
			// third parameter is wether this is instanced.
			auto ab = DisplayDevice::CreateAttributeSet(false, true, false);
			// If these were instanced then there is an extra parameter on the end, which defaults to 1.
			// A 0 indictes that there is no advancement per instance
			auto pos = ab->CreateAttribute();
			pos->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::POSITION, 2, AttributeDesc::VariableType::FLOAT, false));
			auto col = ab->CreateAttribute();
			col->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::COLOR, 4, AttributeDesc::VariableType::UNSIGNED_BYTE, true));
			ab->SetDrawMode(AttributeSet::DrawMode::TRIANGLES);
			AddAttributeSet(ab);

			std::vector<glm::vec2> vertices;
			vertices.emplace_back(0.0f, 0.0f);
			vertices.emplace_back(0.0f, 1.0f);
			vertices.emplace_back(1.0f, 0.0f);
			vertices.emplace_back(1.0f, 1.0f);
			//ab->SetCount(vertices.size());
			pos->Update(&vertices[0], 0, vertices.size() * sizeof(glm::vec2));

			std::vector<glm::u8vec4> colors;
			colors.emplace_back(255,0,0,255);
			colors.emplace_back(0,255,0,255);
			colors.emplace_back(0,0,255,255);
			colors.emplace_back(255,0,0,255);			
			col->Update(&colors[0], 0, colors.size() * sizeof(glm::u8vec4));			

			glm::u8vec4 x;

			std::vector<uint8_t> indices;
			indices.emplace_back(0);
			indices.emplace_back(1);
			indices.emplace_back(2);
			indices.emplace_back(2);
			indices.emplace_back(1);
			indices.emplace_back(3);
			ab->UpdateIndicies(indices);
			*/

			//SetColor(255,255,255);
			SetOrder(0);
		}
		virtual ~SquareRenderable() {}
	protected:
		KRE::DisplayDeviceDef Attach(const KRE::DisplayDevicePtr& dd) {
			KRE::DisplayDeviceDef def(GetAttributeSet()/*, GetUniformSet()*/);
			def.SetHint("shader", "attr_color_shader");
			return def;
		}
	private:
		SquareRenderable(const SquareRenderable&);
		SquareRenderable& operator=(const SquareRenderable&);
	};
	typedef std::shared_ptr<SquareRenderable> SquareRenderablePtr;
}

class SimpleTextureHolder : public KRE::Blittable
{
public:
	SimpleTextureHolder(const std::string& filename) {
		using namespace KRE;
		SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		auto tex = DisplayDevice::CreateTexture(filename, Texture::Type::TEXTURE_2D, 4);
		tex->SetFiltering(Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::POINT);
		tex->SetAddressModes(Texture::AddressMode::BORDER, Texture::AddressMode::BORDER);
		SetTexture(tex);
	}
private:
};

int main(int argc, char *argv[])
{
	std::list<double> smoothed_time;
	double cumulative_time = 0.0;
	int cnt = 0;

	using namespace KRE;

	SDL::SDL_ptr manager(new SDL::SDL());

	WindowManagerPtr main_wnd = WindowManager::Factory("SDL", "opengl");
	main_wnd->EnableVsync(false);
	main_wnd->CreateWindow(800,600);

	SceneGraphPtr scene = SceneGraph::Create("main");
	SceneNodePtr root = scene->RootNode();
	root->SetNodeName("root_node");
	auto scenecam = std::make_shared<Camera>("cam0", 0, 800, 0, 600);
	scenecam->AttachFrustum(std::make_shared<Frustum>());
	root->AttachCamera(scenecam);
	auto sunlight = std::make_shared<Light>("the_sun", glm::vec3(1.0f, 1.0f, 1.0f));
	sunlight->SetAmbientColor(glm::vec4(1.0f,1.0f,1.0f,1.0f));
	root->AttachLight(0, sunlight);

	SquareRenderablePtr square(std::make_shared<SquareRenderable>());
	square->SetPosition(600.0f, 400.0f);
	square->SetScale(2.0f,2.0f);
	root->AttachObject(square);

	auto rman = std::make_shared<RenderManager>();
	auto rq = std::make_shared<RenderQueue>("opaques");
	rman->AddQueue(0, rq);

	auto cairo_canvas = Vector::Context::CreateInstance("cairo", 512, 512);
	cairo_canvas->SetSourceColor(0.0, 1.0, 0.0);
	cairo_canvas->Paint();
	cairo_canvas->Fill();
	auto path = cairo_canvas->NewPath();
	path->Circle(256, 256, 100);
	cairo_canvas->AddPath(path);
	cairo_canvas->SetSourceColor(0.0, 0.0, 1.0);
	cairo_canvas->Fill();
	auto text = cairo_canvas->NewPath();
	text->MoveTo(10, 10);
	text->TextPath("ABCDabcde");
	cairo_canvas->AddPath(text);
	cairo_canvas->Fill();
	cairo_canvas->SetOrder(5);
	cairo_canvas->SetPosition(256.0f,256.0f);
	cairo_canvas->SetColor(1.0f,1.0f,1.0f,1.0f);
	root->AttachObject(cairo_canvas);

	auto psystem = scene->CreateNode("particle_system_container", json::parse_from_file("psystem1.cfg"));
	auto particle_cam = std::make_shared<Camera>("particle_cam", main_wnd);
	particle_cam->LookAt(glm::vec3(0.0f, 10.0f, 20.0f), glm::vec3(0.0f), glm::vec3(0.0f,1.0f,0.0f));
	psystem->AttachCamera(particle_cam);
	root->AttachNode(psystem);
	//auto rt = DisplayDevice::RenderTargetInstance(400, 300);
	//rt->SetClearColor(0.0f,0.0f,0.0f,0.0f);
	//rt->SetDrawRect(rect(400,300,400,300));
	//rt->Create();
	//psystem->AttachRenderTarget(rt);
	//root->AttachObject(rt);

	auto tex = std::shared_ptr<Blittable>(new SimpleTextureHolder("card-back.png"));
	tex->SetDrawRect(rectf(0.0f,0.0f,146.0f,260.0f));
	tex->SetPosition(146.0f/2.0f, 600.0f-130.0f);
	tex->SetOrder(10);
	root->AttachObject(tex);

	float angle = 1.0f;
	float angle_step = 0.5f;

	auto canvas = Canvas::GetInstance();
	canvas->SetDimensions(800, 600);

	auto canvas_texture = DisplayDevice::CreateTexture("widgets.png");
	canvas_texture->SetFiltering(Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::NONE);

	SDL_Event e;
	bool done = false;
	profile::timer timer;
	while(!done) {
		timer.start();
		while(SDL_PollEvent(&e)) {
			if(e.type == SDL_KEYUP && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
				done = true;
			} else if(e.type == SDL_KEYDOWN) {
				LOG_DEBUG("KEY PRESSED: " << SDL_GetKeyName(e.key.keysym.sym) << " : " << e.key.keysym.sym << " : " << e.key.keysym.scancode);
			} else if(e.type == SDL_QUIT) {
				done = true;
			}
		}

		main_wnd->Clear(DisplayDevice::ClearFlags::DISPLAY_CLEAR_ALL);

		// Called once a cycle before rendering.
		scene->Process(SDL_GetTicks() / 1000.0f);

		tex->SetRotation(angle, glm::vec3(0.0f,0.0f,1.0f));
		cairo_canvas->SetRotation(angle, glm::vec3(0.0f,0.0f,1.0f));
		angle += angle_step;
		while(angle >= 360.0f) {
			angle -= 360.0f;
		}

		scene->RenderScene(rman);
		rman->Render(main_wnd);

		canvas->BlitTexture(canvas_texture, 
			rect(3,4,56,22), 
			0.0f, 
			//rect(800-56, 0, 56, 22), 
			rect(0,0,112,44),
			Color(1.0f,1.0f,1.0f,0.5f));

		double t1 = timer.check();
		if(t1 < 1.0/50.0) {
			SDL_Delay(Uint32((1.0/50.0-t1)*1000.0));
		}
		double t = timer.check();

		smoothed_time.push_back(t);
		cumulative_time += t;
		if(++cnt > 10) {
			cnt = 0;
			LOG_DEBUG("FPS: " << (smoothed_time.size()/cumulative_time) << ", Time: " << t1*1000.0);
		}
		if(smoothed_time.size() > 50) {
			cumulative_time -= smoothed_time.front();
			smoothed_time.pop_front();
		}

		main_wnd->Swap();
	}
	return 0;
}