/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <cmath>
#include <chrono>

#include "ModelMatrixScope.hpp"
#include "ParticleSystem.hpp"
#include "ParticleSystemAffectors.hpp"
#include "ParticleSystemParameters.hpp"
#include "ParticleSystemEmitters.hpp"
#include "SceneGraph.hpp"
#include "Shaders.hpp"
#include "spline.hpp"
#include "WindowManager.hpp"
#include "variant_utils.hpp"
#include "profile_timer.hpp"

namespace KRE
{
	namespace Particles
	{
		namespace
		{
			SceneNodeRegistrar<ParticleSystemContainer> psc_register("particle_system_container");

			std::default_random_engine& get_rng_engine()
			{
				static std::unique_ptr<std::default_random_engine> res;
				if(res == nullptr) {
					auto seed = std::chrono::system_clock::now().time_since_epoch().count();
					res.reset(new std::default_random_engine(std::default_random_engine::result_type(seed)));
				}
				return *res;
			}
		}

		void init_physics_parameters(PhysicsParameters& pp)
		{
			pp.position = glm::vec3(0.0f);
			pp.color = color_vector(255,255,255,255);
			pp.dimensions = glm::vec3(1.0f);
			pp.time_to_live = 10.0f;
			pp.mass = 1.0f;
			pp.velocity = 100.0f;
			pp.direction = glm::vec3(0.0f,1.0f,0.0f);
			pp.orientation = glm::quat(1.0f,0.0f,0.0f,0.0f);
			pp.area = rectf::fromCoordinates(0.0f, 0.0f, 1.0f, 1.0f);
		}

		const float g_random_floats[] = {
			0.049816f,0.351913f,0.464190f,0.132040f,0.757947f,0.726070f,0.664962f,0.881089f,0.252818f,0.779964f,0.630832f,0.384000f,0.272836f,0.103783f,0.663100f,0.009522f,0.557894f,0.077731f,0.879053f,0.566782f,0.949002f,0.376513f,0.451136f,0.887032f,0.048980f,0.069468f,0.029046f,0.393614f,0.079708f,0.668495f,0.470545f,0.660771f,0.704097f,0.276430f,0.514968f,0.240814f,0.846699f,0.267557f,0.300176f,0.724886f,0.497518f,0.331176f,0.083627f,0.963206f,0.248988f,0.885810f,0.319114f,0.115112f,0.854251f,0.748507f,0.453165f,0.126804f,0.441914f,0.959333f,0.733304f,0.885470f,0.542718f,0.019613f,0.711202f,0.603148f,0.719287f,0.723497f,0.427518f,0.513498f,0.111944f,0.731375f,0.885200f,0.181547f,0.470070f,0.051478f,0.960697f,0.232960f,0.406402f,0.429336f,0.427387f,0.098705f,0.445932f,0.075775f,0.035376f,0.076785f,0.524611f,0.240761f,0.322408f,0.159561f,0.587143f,0.426029f,0.940862f,0.328024f,0.826875f,0.853248f,0.147932f,0.428886f,0.471658f,0.511122f,0.795024f,0.022887f,0.243045f,0.304740f,0.696762f,0.310054f,0.175864f,0.649569f,0.890999f,0.127486f,0.182947f,0.182389f,0.703567f,0.195614f,0.965380f,0.009405f,0.092819f,0.018672f,0.810124f,0.250556f,0.810192f,0.773648f,0.770375f,0.257252f,0.017890f,0.939420f,0.303807f,0.232343f,0.150543f,0.504620f,0.842319f,0.535988f,0.694488f,0.170483f,0.004525f,0.454411f,0.839382f,0.745287f,0.337650f,0.891403f,0.488832f,0.762584f,0.015528f,0.331426f,0.208940f,0.489455f,0.828778f,0.550468f,0.359038f,0.807208f,0.915213f,0.214344f,0.853073f,0.719718f,0.903940f,0.517308f,0.912161f,0.389092f,0.724985f,0.955814f,0.410736f,0.920049f,0.084828f,0.797697f,0.315766f,0.559791f,0.172493f,0.656388f,0.548919f,0.385966f,0.264529f,0.188883f,0.846880f,0.899251f,0.591975f,0.446757f,0.789696f,0.587199f,0.748209f,0.978752f,0.748898f,0.491104f,0.511385f,0.834143f,0.666914f,0.987192f,0.765096f,0.086856f,0.110942f,0.427437f,0.090309f,0.095049f,0.195094f,0.404407f,0.012713f,0.149503f,0.684388f,0.280074f,0.093126f,0.431921f,0.786421f,0.212365f,0.392937f,0.802137f,0.907700f,0.327952f,0.008663f,0.127894f,0.561314f,0.418472f,0.604329f,0.944435f,0.923413f,0.713976f,0.260574f,0.731627f,0.463843f,0.936125f,0.666353f,0.087012f,0.040071f,0.119736f,0.191969f,0.383342f,0.715349f,0.170239f,0.768436f,0.973391f,0.917083f,0.992807f,0.653278f,0.753537f,0.917962f,0.373264f,0.060479f,0.546833f,0.731938f,0.838864f,0.574130f,0.707438f,0.554233f,0.096911f,0.131331f,0.988870f,0.726913f,0.599628f,0.858025f,0.731107f,0.719976f,0.464229f,0.957819f,0.994856f,0.826318f,0.677724f,0.302827f,0.359239f,0.674440f,0.917338f,0.049362f,0.671566f,0.465578f,0.740796f,0.020621f,0.329995f,0.195269f,0.558041f,0.361798f,0.442880f,0.379834f,0.039777f,0.007227f,0.958874f,0.785284f,0.191678f,0.308950f,0.131773f,0.862485f,0.609992f,0.094696f,0.271433f,0.493937f,0.709052f,0.861582f,0.950347f,0.923022f,0.388823f,0.509333f,0.763778f,0.488338f,0.496843f,0.290786f,0.474342f,0.848693f,0.979940f,0.653322f,0.270122f,0.537118f,0.153934f,0.586468f,0.804556f,0.905998f,0.521654f,0.529801f,0.308273f,0.254919f,0.909136f,0.961485f,0.024820f,0.305708f,0.875729f,0.439203f,0.446791f,0.296141f,0.785313f,0.138120f,0.754650f,0.099019f,0.143027f,0.188971f,0.691649f,0.683662f,0.735483f,0.305968f,0.064443f,0.372700f,0.333658f,0.079249f,0.625580f,0.111526f,0.688454f,0.738923f,0.272029f,0.967477f,0.871842f,0.353910f,0.049895f,0.370213f,0.684670f,0.063865f,0.171643f,0.956650f,0.499477f,0.639287f,0.176415f,0.008880f,0.057395f,0.084750f,0.059854f,0.290840f,0.057443f,0.132243f,0.996249f,0.971076f,0.919984f,0.883215f,0.517703f,0.339978f,0.569973f,0.944706f,0.312517f,0.218708f,0.056953f,0.640417f,0.284284f,0.545096f,0.034946f,0.063562f,0.065705f,0.293648f,0.684675f,0.254740f,0.714244f,0.030168f,0.331764f,0.842816f,0.003202f,0.357169f,0.446989f,0.366099f,0.250115f,0.818199f,0.698492f,0.809173f,0.448653f,0.494462f,0.441633f,0.839986f,0.060179f,0.245939f,0.301247f,0.832698f,0.912142f,0.974826f,0.035353f,0.113012f,0.493469f,0.278085f,0.521352f,0.390995f,0.548791f,0.717683f,0.718696f,0.056602f,0.285804f,0.684472f,0.719572f,0.738324f,0.021228f,0.430389f,0.275903f,0.176610f,0.514193f,0.462756f,0.401761f,0.280353f,0.150481f,0.755879f,0.100992f,0.034035f,0.141670f,0.464242f,0.906575f,0.350737f,0.850196f,0.296521f,0.578023f,0.745606f,0.620628f,0.911986f,0.963414f,0.887484f,0.230689f,0.565100f,0.061720f,0.302942f,0.429906f,0.387978f,0.727559f,0.215067f,0.432180f,0.754702f,0.144017f,0.235081f,0.609395f,0.910168f,0.189515f,0.252511f,0.700680f,0.716301f,0.572776f,0.831574f,0.325637f,0.920283f,0.176837f,0.988165f,0.455332f,0.686215f,0.400821f,0.741976f,0.252861f,0.359839f,0.519289f,0.988676f,0.612615f,0.115132f,0.686633f,0.272435f,0.086159f,0.524443f,0.331939f,0.042629f,0.230686f,0.416252f,0.342404f,0.111232f,0.816936f,0.055414f,0.785865f,0.778618f,0.616209f,0.572136f,0.530881f,0.532482f,0.495532f,0.252969f,0.182400f,0.243021f,0.396402f,0.318205f,0.349290f,0.350873f,0.081871f,0.876653f,0.624434f,0.974464f,0.700606f,0.055375f,0.406827f,0.030158f,0.642231f,0.736378f,0.087014f,0.070038f,0.518558f,0.735145f,0.301599f,0.384306f,0.588689f,0.671405f,0.229590f,0.102525f,0.506628f,0.707528f,0.933573f,0.252533f,0.877536f,0.964775f,0.863486f,0.518430f,0.998648f,0.925536f,0.052004f,0.378128f,0.083932f,0.847152f,0.590758f,0.323381f,0.744557f,0.959480f,0.428964f,0.475836f,0.052289f,0.506742f,0.764365f,0.444436f,0.947933f,0.990211f,0.575030f,0.133701f,0.385357f,0.940878f,0.584619f,0.148122f,0.502596f,0.322441f,0.968829f,0.762660f,0.379700f,0.060078f,0.843185f,0.288531f,0.938805f,0.987647f,0.359078f,0.280806f,0.141255f,0.686813f,0.553431f,0.961540f,0.481179f,0.690624f,0.815899f,0.599122f,0.521842f,0.478921f,0.464701f,0.349259f,0.793328f,0.694550f,0.857603f,0.261629f,0.206040f,0.351563f,0.248165f,0.153044f,0.438275f,0.236517f,0.159836f,0.785254f,0.747494f,0.671776f,0.963138f,0.033929f,0.819101f,0.718813f,0.065833f,0.343380f,0.604763f,0.356173f,0.265345f,0.906090f,0.380852f,0.230767f,0.359754f,0.441912f,0.258994f,0.014988f,0.702360f,0.761964f,0.152600f,0.921850f,0.539445f,0.147814f,0.309216f,0.454642f,0.356517f,0.946892f,0.287642f,0.368728f,0.130650f,0.247485f,0.035756f,0.176186f,0.041858f,0.446681f,0.120172f,0.357353f,0.421611f,0.435941f,0.298272f,0.988428f,0.239236f,0.803214f,0.398387f,0.435684f,0.365156f,0.267679f,0.792100f,0.492236f,0.962469f,0.731278f,0.776806f,0.183499f,0.294797f,0.136212f,0.625124f,0.986478f,0.788491f,0.374000f,0.354699f,0.392796f,0.532004f,0.991664f,0.897062f,0.296223f,0.355727f,0.404818f,0.032877f,0.255632f,0.923754f,0.854440f,0.464078f,0.130302f,0.737463f,0.860654f,0.727699f,0.215785f,0.842624f,0.437856f,0.353832f,0.838004f,0.230761f,0.673984f,0.028977f,0.356469f,0.206073f,0.353772f,0.359554f,0.853635f,0.266532f,0.360721f,0.717601f,0.068403f,0.627276f,0.442631f,0.186483f,0.692861f,0.342634f,0.216390f,0.669026f,0.270835f,0.896738f,0.742957f,0.149951f,0.096209f,0.109435f,0.674770f,0.060378f,0.642709f,0.612116f,0.495232f,0.831644f,0.802777f,0.996044f,0.191232f,0.471793f,0.161986f,0.923067f,0.361219f,0.289042f,0.481174f,0.055476f,0.127252f,0.426683f,0.748181f,0.038617f,0.732172f,0.382767f,0.544880f,0.028277f,0.633050f,0.151004f,0.956527f,0.356456f,0.598005f,0.537535f,0.935689f,0.976706f,0.452122f,0.848174f,0.481708f,0.816732f,0.954392f,0.784656f,0.471570f,0.225332f,0.904391f,0.584169f,0.694976f,0.357322f,0.135878f,0.247477f,0.024230f,0.758967f,0.043988f,0.956395f,0.560478f,0.153228f,0.780476f,0.325910f,0.616882f,0.675119f,0.133160f,0.080313f,0.054992f,0.659666f,0.354866f,0.712150f,0.514127f,0.765718f,0.125596f,0.069383f,0.649803f,0.346571f,0.299185f,0.026929f,0.513590f,0.811630f,0.810419f,0.106741f,0.504300f,0.411649f,0.886517f,0.304106f,0.942875f,0.713533f,0.632405f,0.411140f,0.767911f,0.689797f,0.834612f,0.771149f,0.315604f,0.154002f,0.974441f,0.885012f,0.790356f,0.438592f,0.555956f,0.374011f,0.206751f,0.088201f,0.865052f,0.999142f,0.099729f,0.672747f,0.687219f,0.277158f,0.670184f,0.994369f,0.656777f,0.516266f,0.723763f,0.244811f,0.200233f,0.132950f,0.440254f,0.480310f,0.210478f,0.985026f,0.801700f,0.925095f,0.964048f,0.259348f,0.234498f,0.538412f,0.516140f,0.546066f,0.462490f,0.320681f,0.101219f,0.990841f,0.705918f,0.469235f,0.482504f,0.483630f,0.629655f,0.591560f,0.861391f,0.374765f,0.981064f,0.252511f,0.835865f,0.128918f,0.753909f,0.571353f,0.021362f,0.073628f,0.264012f,0.934435f,0.682629f,0.175518f,0.471777f,0.080721f,0.013191f,0.803801f,0.686600f,0.421410f,0.310741f,0.202602f,0.841692f,0.933513f,0.080557f,0.891897f,0.441673f,0.922960f,0.231838f,0.218173f,0.118689f,0.378825f,0.768453f,0.562402f,0.248816f,0.170297f,0.298995f,0.332503f,0.994594f,0.496598f,0.962426f,0.383581f,0.370992f,0.452082f,0.974203f,0.250994f,0.852688f,0.370012f,0.510968f,0.893432f,0.725681f,0.455207f,0.465199f,0.136068f,0.864153f,0.694875f,0.188290f,0.803402f,0.115593f,0.875948f,0.503051f,0.529260f,0.688167f,0.854351f,0.384597f,0.334857f,0.241691f,0.775655f,0.989895f,0.447569f,0.128342f,0.386791f,0.447104f,0.181711f,0.955226f,0.695165f,0.610483f,0.472218f,0.345259f,0.205100f,0.037431f,0.312407f,0.155676f,0.613405f,0.771707f,0.633305f,0.128201f,0.887986f,0.219645f,0.325381f,0.847917f,0.938228f,0.911030f,0.160457f,0.288714f,0.856587f,0.248081f,0.354368f,0.134671f,0.851273f,0.297736f,0.717428f,0.454482f,0.274405f,0.185237f,0.583414f,0.339196f,0.493689f,0.940223f,0.567706f,0.414064f,0.064134f,0.547397f,0.276155f,0.028391f,0.675890f,0.107998f,0.228517f,0.691335f,0.521877f,0.149279f,0.849905f,0.437945f,0.021975f,0.278857f,0.562238f,0.213094f,0.627413f,0.198886f,0.932343f,0.688826f,0.718311f,0.029705f,0.927083f,0.774069f,0.298602f,0.479873f,0.527035f,0.228383f,0.325766f,0.503731f,0.963009f,0.500654f,0.518653f,0.151118f,0.383783f,0.129504f,0.436384f,0.607170f,0.824151f,0.668025f,0.476929f,0.291830f,0.420741f,0.237733f,0.307840f,0.015649f,0.185713f,0.034841f,0.607096f,0.825398f,0.577487f,0.668943f,0.992789f,0.403095f,0.151063f,0.325578f,0.680846f,0.866327f,0.791715f,0.829095f,0.895277f,0.045463f,0.263894f,0.612585f,0.921115f,0.851982f,0.886914f,0.952828f,0.132648f,0.097055f,0.190687f,0.128311f,0.435573f,0.044504f,0.034406f,0.139056f,0.558951f,0.388927f,0.280270f,0.472669f,0.139395f,0.118127f,0.110334f,0.999442f,0.814614f,0.556933f,0.130158f,0.271389f,0.735681f,0.583284f,0.706652f,0.329576f,0.307197f,0.671166f,0.493439f,0.088214f,0.252586f,0.741245f,0.774914f,0.662877f,0.599345f,
		};

		const int g_num_random_floats = sizeof(g_random_floats) / sizeof(g_random_floats[0]);

		static int g_random_next = 1;

		float get_random_float(float min, float max)
		{

			g_random_next = g_random_next * 1103515245 + 12345;
			int index = (((unsigned)(g_random_next / 65536) % 32768))%g_num_random_floats;
			return min + (max - min)*g_random_floats[index];

			//old implementation, get really nice random numbers, but too slow. Far. Too. Slow.
			//if(min > max) {
			//	std::swap(min, max);
			//}
			//std::uniform_real_distribution<float> gen(min, max);
			//return gen(get_rng_engine());
		}

		std::ostream& operator<<(std::ostream& os, const glm::vec3& v)
		{
			os << "[" << v.x << "," << v.y << "," << v.z << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const glm::vec4& v)
		{
			os << "[" << v.x << "," << v.y << "," << v.z << "," << v.w << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const glm::quat& v)
		{
			os << "[" << v.w << "," << v.x << "," << v.y << "," << v.z << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const color_vector& c)
		{
			os << "[" << int(c.r) << "," << int(c.g) << "," << int(c.b) << "," << int(c.a) << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const Particle& p)
		{
			os << "P"<< p.current.position
				<< ", IP" << p.initial.position
				<< ", DIM" << p.current.dimensions
				<< ", IDIR" << p.initial.direction
				<< ", DIR" << p.current.direction
				<< ", TTL(" << p.current.time_to_live << ")"
				<< ", ITTL(" <<  p.initial.time_to_live << ")"
				<< ", C" << p.current.color
				<< ", M(" << p.current.mass << ")"
				<< ", V(" << p.current.velocity << ")"
				<< std::endl
				<< "\tO(" << p.current.orientation << ")"
				<< "\tIO(" << p.initial.orientation << ")"
				;
			return os;
		}

		// Compute any vector out of the infinite set perpendicular to v.
		glm::vec3 perpendicular(const glm::vec3& v)
		{
			glm::vec3 perp = glm::cross(v, glm::vec3(1.0f,0.0f,0.0f));
			float len_sqr = perp.x*perp.x + perp.y*perp.y + perp.z*perp.z;
			if(len_sqr < 1e-12) {
				perp = glm::cross(v, glm::vec3(0.0f,1.0f,0.0f));
			}
			float len = glm::length(perp);
			if(len > 1e-14f) {
				return perp / len;
			}
			return perp;
		}

		glm::vec3 create_deviating_vector(float angle, const glm::vec3& v, const glm::vec3& up)
		{
			glm::vec3 up_up = up;
			if(up == glm::vec3(0.0f)) {
				up_up = perpendicular(v);
			}

			glm::quat q = glm::angleAxis(glm::radians(angle), up_up);
			return q * v;
		}

		ParticleSystem::ParticleSystem(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: EmitObject(parent, node),
			  SceneObject(node),
			  default_particle_width_(node["default_particle_width"].as_float(1.0f)),
			  default_particle_height_(node["default_particle_height"].as_float(1.0f)),
			  default_particle_depth_(node["default_particle_depth"].as_float(1.0f)),
			  particle_quota_(node["particle_quota"].as_int32(100)),
			  elapsed_time_(0.0f),
			  scale_velocity_(1.0f),
			  scale_time_(1.0f),
			  scale_dimensions_(1.0f),
			  texture_node_(),
			  use_position_(node["use_position"].as_bool(true))
		{
			if(node.has_key("fast_forward")) {
				float ff_time = float(node["fast_forward"]["time"].as_float());
				float ff_interval = float(node["fast_forward"]["interval"].as_float());
				fast_forward_.reset(new std::pair<float,float>(ff_time, ff_interval));
			}

			if(node.has_key("scale_velocity")) {
				scale_velocity_ = float(node["scale_velocity"].as_float());
			}
			if(node.has_key("scale_time")) {
				scale_time_ = float(node["scale_time"].as_float());
			}
			if(node.has_key("scale")) {
				if(node["scale"].is_list()) {
					scale_dimensions_ = variant_to_vec3(node["scale"]);
				} else {
					float s = node["scale"].as_float();
					scale_dimensions_ = glm::vec3(s, s, s);
				}
			}

			if(node.has_key("emitter")) {
				if(node["emitter"].is_map()) {
					emitter_ = Emitter::factory(parent, node["emitter"]);
				} else {
					ASSERT_LOG(false, "'emitter' attribute must be a map.");
				}
			}
			if(node.has_key("affector")) {
				if(node["affector"].is_map()) {
					auto aff = Affector::factory(parent, node["affector"]);
					affectors_.emplace_back(aff);
				} else if(node["affector"].is_list()) {
					for(int n = 0; n != node["affector"].num_elements(); ++n) {
						auto aff = Affector::factory(parent, node["affector"][n]);
						affectors_.emplace_back(aff);
					}
				} else {
					ASSERT_LOG(false, "'affector' attribute must be a list or map.");
				}
			}

			if(node.has_key("max_velocity")) {
				max_velocity_.reset(new float(node["max_velocity"].as_float()));
			}

			if(node.has_key("texture")) {
				setTextureNode(node["texture"]);
			}
			if(node.has_key("image")) {
				setTextureNode(node["image"]);
			}

			initAttributes();
		}

		void ParticleSystem::init()
		{
			active_emitter_ = emitter_->clone();
			active_emitter_->init();
			// In order to create as few re-allocations of particles, reserve space here
			active_particles_.reserve(particle_quota_);
		}

		void ParticleSystem::setTextureNode(const variant& node)
		{
			texture_node_ = node;
		}

		void ParticleSystem::fastForward()
		{
			if(fast_forward_) {
				for(float t = 0; t < fast_forward_->first; t += fast_forward_->second) {
					update(fast_forward_->second);
					elapsed_time_ += fast_forward_->second;
				}
			}
		}

		std::pair<float,float> ParticleSystem::getFastForward() const
		{
			if(fast_forward_) {
				return *fast_forward_;
			}

			return std::pair<float,float>(0.0f, 0.05f);
		}

		void ParticleSystem::setFastForward(const std::pair<float,float>& p)
		{
			fast_forward_.reset(new std::pair<float,float>(p));
		}

		ParticleSystem::ParticleSystem(const ParticleSystem& ps)
			: EmitObject(ps),
			  SceneObject(ps),
			  default_particle_width_(ps.default_particle_width_),
			  default_particle_height_(ps.default_particle_height_),
			  default_particle_depth_(ps.default_particle_depth_),
			  particle_quota_(ps.particle_quota_),
			  elapsed_time_(0),
			  scale_velocity_(ps.scale_velocity_),
			  scale_time_(ps.scale_time_),
			  scale_dimensions_(ps.scale_dimensions_),
			  texture_node_(),
			  use_position_(ps.use_position_)

		{
			if(ps.fast_forward_) {
				fast_forward_.reset(new std::pair<float,float>(ps.fast_forward_->first, ps.fast_forward_->second));
			}
			setShader(ShaderProgram::getProgram("particles_shader"));

			if(ps.max_velocity_) {
				max_velocity_.reset(new float(*ps.max_velocity_));
			}
			if(ps.texture_node_.is_map()) {
				setTextureNode(ps.texture_node_);
			}
			initAttributes();
		}

		void ParticleSystem::handleWrite(variant_builder* build) const
		{
			Renderable::writeData(build);

			if(use_position_ == false) {
				build->add("use_position", use_position_);
			}

			if(texture_node_.is_null() == false) {
				build->add("texture", texture_node_);
			}

			if(default_particle_width_ != 1.0f) {
				build->add("default_particle_width", default_particle_width_);
			}
			if(default_particle_height_ != 1.0f) {
				build->add("default_particle_height", default_particle_height_);
			}
			if(default_particle_depth_ != 1.0f) {
				build->add("default_particle_depth", default_particle_depth_);
			}
			if(particle_quota_ != 100) {
				build->add("particle_quota", particle_quota_);
			}
			if(scale_velocity_ != 1.0f) {
				build->add("scale_velocity", scale_velocity_);
			}
			if(scale_time_ != 1.0f) {
				build->add("scale_time", scale_time_);
			}
			if(scale_dimensions_ != glm::vec3(1.0f)) {
				if(scale_dimensions_.x == scale_dimensions_.y && scale_dimensions_.x == scale_dimensions_.z) {
					build->add("scale", scale_dimensions_.x);
				} else {
					build->add("scale", vec3_to_variant(scale_dimensions_));
				}
			}
			if(fast_forward_) {
				variant_builder res;
				res.add("time", fast_forward_->first);
				res.add("interval", fast_forward_->second);
				build->add("fast_forward", res.build());
			}
			if(max_velocity_) {
				build->add("max_velocity", *max_velocity_);
			}
			build->add("emitter", emitter_->write());
			for(const auto& aff : affectors_) {
				build->add("affector", aff->write());
			}
		}

		void ParticleSystem::update(float dt)
		{
			// run objects
			active_emitter_->emitProcess(dt);
			for(auto a : affectors_) {
				a->emitProcess(dt);
			}

			// Decrement the ttl on particles
			for(auto& p : active_particles_) {
				p.current.time_to_live -= dt;
			}

			active_emitter_->current.time_to_live -= dt;

			// Kill end-of-life particles
			active_particles_.erase(std::remove_if(active_particles_.begin(), active_particles_.end(),
				[](decltype(active_particles_[0]) p){return p.current.time_to_live <= 0.0f;}),
				active_particles_.end());
			// Kill end-of-life emitters
			if(active_emitter_->current.time_to_live <= 0.0f) {
				active_emitter_.reset();
			}
			//active_affectors_.erase(std::remove_if(active_affectors_.begin(), active_affectors_.end(),
			//	[](decltype(active_affectors_[0]) e){return e->current.time_to_live < 0.0f;}),
			//	active_affectors_.end());

			if(active_emitter_) {
				if(max_velocity_ && active_emitter_->current.velocity*glm::length(active_emitter_->current.direction) > *max_velocity_) {
					active_emitter_->current.direction *= *max_velocity_ / glm::length(active_emitter_->current.direction);
				}
				active_emitter_->current.position += active_emitter_->current.direction * active_emitter_->current.velocity * getScaleVelocity() * dt;
				//std::cerr << *active_emitter_ << std::endl;
			}

			/*for(auto a : active_affectors_) {
			if(max_velocity_ && a->current.velocity*glm::length(a->current.direction) > *max_velocity_) {
			a->current.direction *= *max_velocity_ / glm::length(a->current.direction);
			}
			a->current.position += a->current.direction * getParticleSystem()->getScaleVelocity() * static_cast<float>(t);
			//std::cerr << *a << std::endl;
			}*/

			// update particle positions
			for(auto& p : active_particles_) {
				if(max_velocity_ && p.current.velocity*glm::length(p.current.direction) > *max_velocity_) {
					p.current.direction *= *max_velocity_ / glm::length(p.current.direction);
				}

				p.current.position += p.current.direction * p.current.velocity * getScaleVelocity() * dt;

				//std::cerr << p << std::endl;
			}
			//if(active_particles_.size() > 0) {
			//	std::cerr << active_particles_[0] << std::endl;
			//}

			//std::cerr << "XXX: " << name() << " Active Particle Count: " << active_particles_.size() << std::endl;
			//std::cerr << "XXX: " << name() << " Active Emitter Count: " << active_emitters_.size() << std::endl;
		}

		void ParticleSystem::handleEmitProcess(float t)
		{
			t *= scale_time_;
			update(t);
			elapsed_time_ += t;
		}

		ParticleSystemPtr ParticleSystem::factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			auto ps = std::make_shared<ParticleSystem>(parent, node);
			return ps;
		}

		void ParticleSystem::initAttributes()
		{
			// XXX We need to render to a billboard style renderer ala
			// http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/
			/*auto& urv_ = std::make_shared<UniformRenderVariable<glm::vec4>>();
			urv_->AddVariableDescription(UniformRenderVariableDesc::COLOR, UniformRenderVariableDesc::FLOAT_VEC4);
			AddUniformRenderVariable(urv_);
			urv_->Update(glm::vec4(1.0f,1.0f,1.0f,1.0f));*/

			setShader(ShaderProgram::getProgram("particles_shader"));

			//auto as = DisplayDevice::createAttributeSet(true, false ,true);
			auto as = DisplayDevice::createAttributeSet(true, false, false);
			as->setDrawMode(DrawMode::TRIANGLES);

			arv_ = std::make_shared<Attribute<particle_s>>(AccessFreqHint::DYNAMIC);
			arv_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, vertex)));
			arv_->addAttributeDesc(AttributeDesc("a_center_position", 3, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, center)));
			arv_->addAttributeDesc(AttributeDesc("a_qrotation", 4, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, q)));
			arv_->addAttributeDesc(AttributeDesc("a_scale", 3, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, scale)));
			arv_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, texcoord)));
			arv_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(particle_s), offsetof(particle_s, color)));

			as->addAttribute(arv_);
			addAttributeSet(as);
		}

		namespace {
			std::vector<glm::vec3> g_particle_system_translation;
		}

		ParticleSystem::TranslationScope::TranslationScope(const glm::vec3& v) {
			g_particle_system_translation.push_back(v);
		}

		ParticleSystem::TranslationScope::~TranslationScope() {
		}

		void ParticleSystem::preRender(const WindowPtr& wnd)
		{
			if(active_particles_.size() == 0) {
				arv_->clear();
				Renderable::disable();
				return;
			}
			Renderable::enable();
			//LOG_DEBUG("Technique::preRender, particle count: " << active_particles_.size());
			std::vector<particle_s> vtc;
			vtc.reserve(active_particles_.size() * 6);

			const auto tex = getTexture();
			//if(!tex) {
			//	return;
			//}
			for(auto it = active_particles_.begin(); it != active_particles_.end(); ++it) {
				auto& p = *it;

				const auto rf = p.current.area;//tex->getSourceRectNormalised();
				const glm::vec2 tl{ rf.x1(), rf.y2() };
				const glm::vec2 bl{ rf.x1(), rf.y1() };
				const glm::vec2 tr{ rf.x2(), rf.y2() };
				const glm::vec2 br{ rf.x2(), rf.y1() };

				if(!p.init_pos) {
					p.current.position += getPosition();
					if(!ignoreGlobalModelMatrix() && !useParticleSystemPosition()) {
						p.current.position += glm::vec3(get_global_model_matrix()[3]); // need global model translation.
					}

					p.init_pos = true;
				} else if(!useParticleSystemPosition() && g_particle_system_translation.empty() == false) {
					//This particle doesn't move relative to its object, so
					//just adjust it according to how much the screen translation
					//has changed since last frame.'
					p.current.position += g_particle_system_translation.back();
				}

				auto cp = p.current.position;

				for(int n = 0; n != 3; ++n) {
					cp[n] *= getScaleDimensions()[n];
				}

				if(!ignoreGlobalModelMatrix()) {
					if(useParticleSystemPosition()) {
						cp += glm::vec3(get_global_model_matrix()[3]); // need global model translation.
					}
				}

				const glm::vec3 p1 = cp - p.current.dimensions / 2.0f;
				const glm::vec3 p2 = cp + p.current.dimensions / 2.0f;
				const glm::vec4 q{ p.current.orientation.x, p.current.orientation.y, p.current.orientation.z, p.current.orientation.w };
				vtc.emplace_back(
					glm::vec3(p1.x, p1.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					tl,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p2.x, p1.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),	// scale
					tr,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p1.x, p2.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					bl,						// tex coord
					p.current.color);		// color

				vtc.emplace_back(
					glm::vec3(p1.x, p2.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					bl,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p2.x, p2.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					br,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p2.x, p1.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					tr,						// tex coord
					p.current.color);		// color
			}
			arv_->update(&vtc);
		}

		void ParticleSystem::postRender(const WindowPtr& wnd)
		{
			if(active_emitter_) {
				if(active_emitter_->doDebugDraw()) {
					active_emitter_->draw(wnd);
				}
			}
			for(auto& aff : affectors_) {
				if(aff->doDebugDraw()) {
					aff->draw(wnd);
				}
			}
		}

		ParticleSystemContainer::ParticleSystemContainer(std::weak_ptr<SceneGraph> sg, const variant& node)
			: SceneNode(sg, node),
			  particle_system_()
		{
		}

		void ParticleSystemContainer::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
		{
			attachObject(particle_system_);
		}

		ParticleSystemContainerPtr ParticleSystemContainer::get_this_ptr()
		{
			return std::static_pointer_cast<ParticleSystemContainer>(shared_from_this());
		}

		variant ParticleSystemContainer::write() const
		{
			if(particle_system_) {
				return particle_system_->write();
			}
			return variant();
		}

		void ParticleSystemContainer::init(const variant& node)
		{
			particle_system_ = ParticleSystem::factory(get_this_ptr(), node);
			particle_system_->init();
		}

		ParticleSystemContainerPtr ParticleSystemContainer::create(std::weak_ptr<SceneGraph> sg, const variant& node)
		{
			auto ps = std::make_shared<ParticleSystemContainer>(sg, node);
			ps->init(node);
			return ps;
		}

		void ParticleSystemContainer::process(float delta_time)
		{
			//LOG_DEBUG("ParticleSystemContainer::Process: " << delta_time);
			particle_system_->emitProcess(delta_time);
		}

		EmitObject::EmitObject(std::weak_ptr<ParticleSystemContainer> parent)
			: name_(),
			  enabled_(true),
			  do_debug_draw_(false),
			  parent_container_(parent)
		{
			ASSERT_LOG(parent.lock() != nullptr, "parent is null");
			std::stringstream ss;
			ss << "emit_object_" << static_cast<int>(get_random_float()*100);
			name_ = ss.str();
		}

		EmitObject::EmitObject(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: name_(),
			  enabled_(node["enabled"].as_bool(true)),
			  do_debug_draw_(node["debug_draw"].as_bool(false)),
			  parent_container_(parent)
		{
			ASSERT_LOG(parent.lock() != nullptr, "parent is null");
			if(node.has_key("name")) {
				name_ = node["name"].as_string();
			} else if(node.has_key("id")) {
				name_ = node["id"].as_string();
			} else {
				std::stringstream ss;
				ss << "emit_object_" << static_cast<int>(get_random_float()*100);
				name_ = ss.str();
			}
		}

		variant EmitObject::write() const
		{
			variant_builder res;
			res.add("name", name_);
			if(!enabled_) {
				res.add("enabled", enabled_);
			}
			if(do_debug_draw_) {
				res.add("debug_draw", do_debug_draw_);
			}
			handleWrite(&res);
			return res.build();
		}

		ParticleSystemContainerPtr EmitObject::getParentContainer() const
		{
			auto parent = parent_container_.lock();
			ASSERT_LOG(parent != nullptr, "parent container is nullptr");
			return parent;
		}

		DebugDrawHelper::DebugDrawHelper()
				: SceneObject("DebugDrawHelper")
		{
			setShader(ShaderProgram::getProgram("attr_color_shader"));

			auto as = DisplayDevice::createAttributeSet(true, false, false);
			as->setDrawMode(DrawMode::LINES);

			attrs_ = std::make_shared<Attribute<vertex_color3>>(AccessFreqHint::DYNAMIC);
			attrs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false, sizeof(vertex_color3), offsetof(vertex_color3, vertex)));
			attrs_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color3), offsetof(vertex_color3, color)));

			as->addAttribute(attrs_);
			addAttributeSet(as);
		}

		void DebugDrawHelper::update(const glm::vec3& p1, const glm::vec3& p2, const Color& col)
		{
			std::vector<vertex_color3> res;
			const glm::u8vec4& color = col.as_u8vec4();
			res.emplace_back(p1, color);
			res.emplace_back(glm::vec3(p1.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p1.y, p1.z), color);

			res.emplace_back(glm::vec3(p1.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p1.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p1.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p1.x, p1.y, p2.z), color);

			res.emplace_back(glm::vec3(p2.x, p2.y, p2.z), color);
			res.emplace_back(glm::vec3(p1.x, p2.y, p2.z), color);

			res.emplace_back(glm::vec3(p2.x, p2.y, p2.z), color);
			res.emplace_back(glm::vec3(p2.x, p1.y, p2.z), color);

			res.emplace_back(glm::vec3(p2.x, p2.y, p2.z), color);
			res.emplace_back(glm::vec3(p2.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p1.x, p2.y, p1.z), color);
			res.emplace_back(glm::vec3(p1.x, p2.y, p2.z), color);

			res.emplace_back(glm::vec3(p1.x, p2.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p2.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p2.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p1.y, p2.z), color);

			attrs_->update(&res);
		}

		void convert_quat_to_axis_angle(const glm::quat& q, float* angle, glm::vec3* axis)
		{
			glm::quat newq = q;
			if(q.w > 1.0f) {
				newq = glm::normalize(q);
			}
			if(angle) {
				*angle = 2.0f * std::acos(newq.w);
			}
			float s = std::sqrt(1.0f - newq.w * newq.w);
			if(s < 0.001f) {
				if(axis) {
					axis->x = newq.x;
					axis->y = newq.y;
					axis->z = newq.z;
				}
			} else {
				if(axis) {
					axis->x = newq.x / s;
					axis->y = newq.y / s;
					axis->z = newq.z / s;
				}
			}
		}
	}
}
