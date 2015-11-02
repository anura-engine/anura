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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <sstream>

#include <boost/lexical_cast.hpp>

#include "asserts.hpp"
#include "base64.hpp"
#include "compress.hpp"
#include "filesystem.hpp"
#include "tmx_reader.hpp"
#include "Util.hpp"

namespace tiled
{
	using namespace boost::property_tree;

	namespace
	{
		void display_ptree(ptree const& pt)
		{
			for(auto& v : pt) {
				LOG_DEBUG(v.first << ": " << v.second.get_value<std::string>());
				display_ptree( v.second );
			}
		}

		const uint32_t flipped_horizontally_bit = 0x80000000U;
		const uint32_t flipped_vertically_bit   = 0x40000000U;
		const uint32_t flipped_diagonally_bit	= 0x20000000U;
		const uint32_t flip_mask				= ~(flipped_horizontally_bit | flipped_vertically_bit | flipped_diagonally_bit);

		Orientation convert_orientation(const std::string& o)
		{
			if(o == "orthogonal") {
				return Orientation::ORTHOGONAL;
			} else if(o == "isometric") {
				return Orientation::ISOMETRIC;
			} else if(o == "staggered") {
				return Orientation::STAGGERED;
			} else if(o == "hexagonal") {
				return Orientation::HEXAGONAL;
			}
			ASSERT_LOG(false, "Unrecognised value for orientation: " << o);
			return Orientation::ORTHOGONAL;
		}

		RenderOrder convert_renderorder(const std::string& ro)
		{
			if(ro == "right-down") {
				return RenderOrder::RIGHT_DOWN;
			} else if(ro == "right-up") {
				return RenderOrder::RIGHT_UP;
			} else if(ro == "left-down") {
				return RenderOrder::LEFT_DOWN;
			} else if(ro == "left-up") {
				return RenderOrder::LEFT_UP;
			}
			ASSERT_LOG(false, "Unrecognised value for renderorder: " << ro);
			return RenderOrder::RIGHT_DOWN;
		}

		ImageFormat convert_image_format(const std::string& fmt)
		{
			if(fmt == "png") {
				return ImageFormat::PNG;
			} else if(fmt == "bmp") {
				return ImageFormat::BMP;
			} else if(fmt == "jpg") {
				return ImageFormat::JPEG;
			} else if(fmt == "gif") {
				return ImageFormat::GIF;
			}
			ASSERT_LOG(false, "Unrecognised value for image format: " << fmt);
			return ImageFormat::NONE;
		}


		inline uint32_t make_uint32_le(char n3, char n2, char n1, char n0)
		{
			return (static_cast<uint32_t>(static_cast<uint8_t>(n3)) << 24) 
				| (static_cast<uint32_t>(static_cast<uint8_t>(n2)) << 16) 
				| (static_cast<uint32_t>(static_cast<uint8_t>(n1)) << 8) 
				| static_cast<uint32_t>(static_cast<uint8_t>(n0));
		}
	}

	TmxReader::TmxReader(MapPtr map)
		: map_(map)
	{
	}

	void TmxReader::parseFile(const std::string& filename)
	{
		parseString(sys::read_file(filename));
	}

	void TmxReader::parseString(const std::string& str)
	{
		ptree pt;
		try {
			std::stringstream ss; 
			ss << str;
			read_xml(ss, pt);

			for(auto& v : pt) {
				if(v.first == "map") {
					parseMapElement(v.second);
				}
			}
		} catch(xml_parser_error &e) {
			ASSERT_LOG(false, "Failed to read TMX file " << e.what());
		} catch(...) {
			ASSERT_LOG(false, "Failed to read TMX file with unknown error");
		}
	}

	void TmxReader::parseMapElement(const boost::property_tree::ptree& pt)
	{
		auto attributes = pt.get_child_optional("<xmlattr>");
		ASSERT_LOG(attributes, "map elements must have a minimum number of attributes: 'version', 'orientation', 'width', 'height', 'tilewidth', 'tileheight'");
		auto version = attributes->get_child("version").data();
		auto orientation = attributes->get_child("orientation").data();
		map_->setOrientation(convert_orientation(orientation));
		auto width = attributes->get<int>("width");
		auto height = attributes->get<int>("height");
		map_->setDimensions(width, height);

		auto tilewidth = attributes->get<int>("tilewidth");
		auto tileheight = attributes->get<int>("tileheight");
		map_->setTileDimensions(tilewidth, tileheight);

		auto backgroundcolor = attributes->get_child_optional("backgroundcolor");
		if(backgroundcolor) {
			map_->setBackgroundColor(KRE::Color(backgroundcolor->data()));
		}

		auto renderorder = attributes->get_child_optional("renderorder");
		if(renderorder) {
			map_->setRenderOrder(convert_renderorder(renderorder->data()));
		}

		auto staggerindex = attributes->get_child_optional("staggerindex");
		if(staggerindex) {
			map_->setStaggerIndex(staggerindex->data() == "even" ? StaggerIndex::EVEN : StaggerIndex::ODD);
		}

		auto staggerdir = attributes->get_child_optional("staggerdirection");
		if(staggerdir) {
			map_->setStaggerDirection(staggerdir->data() == "rows" ? StaggerDirection::ROWS : StaggerDirection::COLS);
		}

		auto hexsidelength = attributes->get_optional<int>("hexsidelength");
		if(hexsidelength) {
			map_->setHexsideLength(*hexsidelength);
		}

		for(auto& v : pt) {
			if(v.first == "properties") {
				LOG_DEBUG("parse map properties");
				auto props = parseProperties(v.second);
				map_->setProperties(&props);				
			} else if(v.first == "tileset") {
				parseTileset(v.second);
			} else if(v.first == "objectgroup") {
				//parseObjectGroup(v.second);
			} else if(v.first == "imagelayer") {
				//parseImageLayer(v.second);
			}
		}

		// parse layer's after other since we might parse tileset's out of order.
		for(auto& v : pt) {
			if(v.first == "layer") {
				map_->addLayer(parseLayerElement(v.second));
			}
		}
	}

	void TmxReader::parseTileset(const boost::property_tree::ptree& pt)
	{
		auto attributes = pt.get_child_optional("<xmlattr>");
		ASSERT_LOG(attributes, "tileset elements must have a minimum number of attributes: 'firstgid'");
		int firstgid = attributes->get<int>("firstgid");
		TileSet ts(firstgid);
		
		auto source = attributes->get_child_optional("source");
		if(source) {
			ASSERT_LOG(false, "read and process tileset data from file: " << source->data());
		}

		auto name = attributes->get_child_optional("name");
		if(name) {
			ts.setName(name->data());
		}

		int max_tile_width = -1;
		int max_tile_height = -1;
		auto tilewidth = attributes->get_child_optional("tilewidth");
		if(tilewidth) {
			max_tile_width = tilewidth->get_value<int>();
		}

		auto tileheight = attributes->get_child_optional("tileheight");
		if(tileheight) {
			max_tile_height = tileheight->get_value<int>();
		}

		if(max_tile_width != -1 || max_tile_height != -1) {
			ts.setTileDimensions(max_tile_width, max_tile_height);
		}

		auto spacing = attributes->get_child_optional("spacing");
		if(spacing) {
			ts.setSpacing(spacing->get_value<int>());
		}

		auto margin = attributes->get_child_optional("margin");
		if(margin) {
			ts.setMargin(margin->get_value<int>());
		}

		for(auto& v : pt) {
			if(v.first == "properties") {
				auto props = parseProperties(v.second);
				map_->setProperties(&props);
			} else if(v.first == "tileoffset") {
				auto to_attributes = v.second.get_child("<xmlattr>");
				int x = to_attributes.get<int>("x");
				int y = to_attributes.get<int>("y");
				ts.setTileOffset(x, y);
			} else if(v.first == "image") {
				ts.setImage(parseImageElement(v.second));
			} else if(v.first == "terraintypes") {
				ts.setTerrainTypes(parseTerrainTypes(v.second));
			} else if(v.first == "tile") {
				ts.addTile(parseTileElement(ts, v.second));
			}
		}
		map_->addTileSet(ts);
	}

	std::vector<Property> TmxReader::parseProperties(const boost::property_tree::ptree& pt)
	{
		std::vector<Property> res;

		// No attributes are expected
		for(auto& v : pt) {
			if(v.first == "property") {
				auto prop = v.second.get_child_optional("<xmlattr>");
				if(prop) {
					auto name = prop->get_child("name").data();
					auto value = prop->get_child("value").data();
					res.emplace_back(name, value);
				}
			} else {
				LOG_WARN("Ignoring element '" << v.first << "' as child of 'properties' element");
			}
		}

		return res;
	}

	TileImage TmxReader::parseImageElement(const boost::property_tree::ptree& pt)
	{
		auto attributes = pt.get_child("<xmlattr>");
		
		TileImage image;

		auto source = attributes.get_child_optional("source");
		if(source) {
			image.setSource(source->data());
		}

		auto width = attributes.get_child_optional("width");
		if(width) {
			image.setWidth(width->get_value<int>());
		}

		auto height = attributes.get_child_optional("height");
		if(height) {
			image.setHeight(height->get_value<int>());
		}

		auto trans = attributes.get_child_optional("trans");
		if(trans) {
			image.setTransparentColor(KRE::Color(trans->data()));
			LOG_DEBUG("transparent color set to: " << trans->data() << " : " << KRE::Color(trans->data()));
		}

		auto format = attributes.get_child_optional("format");
		if(format && !source) {
			auto img_data = parseImageDataElement(pt.get_child("data"));
			ASSERT_LOG(!img_data.empty(), "No image data found and no source tag given");
			image.setImageData(convert_image_format(format->data()), img_data);
		}
		return image;
	}

	std::vector<char> TmxReader::parseImageDataElement(const boost::property_tree::ptree& pt)
	{
		std::vector<char> res;
		auto attributes = pt.get_child_optional("<xmlattr>");
		if(attributes) {
			auto encoding = attributes->get_child_optional("encoding");
			// really only one type of encoding is specified.
			if(encoding && encoding->data() == "base64") {
				std::vector<char> data(pt.data().begin(), pt.data().end());
				return base64::b64decode(data);
			}
		}
		return res;
	}

	std::vector<uint32_t> TmxReader::parseDataElement(const boost::property_tree::ptree& pt)
	{
		std::vector<uint32_t> res;
		bool is_compressed_zlib = false;
		bool is_compressed_gzip = false;
		bool is_base64_encoded = false;
		bool is_csv_encoded = false;
		
		auto attributes = pt.get_child_optional("<xmlattr>");
		if(attributes) {
			auto encoding = attributes->get_child_optional("encoding");
			if(encoding && encoding->data() == "base64") {
				is_base64_encoded = true;
			} else if(encoding && encoding->data() == "csv") {
				is_csv_encoded = true;
			}
			auto compression = attributes->get_child_optional("compression");
			if(compression && compression->data() == "gzip") {
				ASSERT_LOG(false, "gzip compression not currently supported, use zlib.");
				is_compressed_gzip = true;
			} else if(compression && compression->data() == "zlib") {
				is_compressed_zlib = true;
			}
		}
		
		if(is_base64_encoded) {
			std::vector<char> data(pt.data().begin(), pt.data().end());
			auto unencoded = base64::b64decode(data);
			if(is_compressed_zlib) {
				auto uncompressed = zip::decompress(unencoded);
				res.reserve(uncompressed.size() / 4);
				ASSERT_LOG(uncompressed.size() % 4 == 0, "Uncompressed data size must be a multiple of 4, found: " << uncompressed.size());
				for(int n = 0; n != uncompressed.size(); n += 4) {
					res.emplace_back(make_uint32_le(uncompressed[n+3], uncompressed[n+2], uncompressed[n+1], uncompressed[n+0]));
				}
			} else if(is_compressed_gzip) {
				// XXX
			}
		} else if(is_csv_encoded) {
			// CSV encoded.
			auto& data = pt.data();
			for(auto& tile : Util::split(data, ",\r\n")) {
				try {
					int value = boost::lexical_cast<int>(tile);
					res.emplace_back(value);
				} catch(boost::bad_lexical_cast& e) {
					ASSERT_LOG(false, "Couldn't convert '" << tile << " to integer value: " << e.what());
				}
			}
		} else {
			// is encoded in <tile> elements.
			for(auto& v : pt) {
				if(v.first == "tile") {
					auto tile_attribute = v.second.get_child("<xlmattr>");
					uint32_t gid = static_cast<uint32_t>(tile_attribute.get<int>("gid"));
					res.emplace_back(gid);
				} else {
					LOG_WARN("Expected 'tile' child elements, found: " << v.first);
				}
			}
		}

		return res;
	}

	std::vector<Terrain> TmxReader::parseTerrainTypes(const boost::property_tree::ptree& pt)
	{
		std::vector<Terrain> res;

		for(auto& v : pt) {
			if(v.first == "terrain") {
				auto name = v.second.get_child("<xmlattr>.name").data();
				auto tile_id = v.second.get<int>("<xmlattr>.tile");
				res.emplace_back(name, tile_id);
			} else {
				LOG_WARN("Expected 'terrain' child elements, found: " << v.first);
			}
		}
		return res;
	}

	TileDefinition TmxReader::parseTileElement(const TileSet& ts, const boost::property_tree::ptree& pt)
	{
		auto attributes = pt.get_child("<xmlattr>");
		uint32_t local_id = attributes.get<int>("id");
		TileDefinition res(local_id);
		res.setTexture(ts.getTexture());

		auto probability = attributes.get_child_optional("probability");
		if(probability) {
			float p = probability->get_value<float>();
			res.setProbability(p);
		}

		auto terrain = attributes.get_child_optional("terrain");
		if(terrain) {
			auto& str = terrain->data();
			std::array<int, 4> terrain_array{ { -1, -1, -1, -1 } };

			std::vector<std::string> strs = Util::split(terrain->data(), ",", Util::SplitFlags::ALLOW_EMPTY_STRINGS);
			int n = 0;
			for(auto& s : strs) {
				if(!s.empty()) {
					try {
						int value = boost::lexical_cast<int>(s);
						ASSERT_LOG(n < 4, "parsing too many elements of terrain data" << str);
						terrain_array[n] = value;
					} catch(boost::bad_lexical_cast& e) {
						ASSERT_LOG(false, "Unable to convert string to integer: " << s << ", " << e.what());
					}
				}
				++n;
			}
			res.setTerrain(terrain_array);
		}

		for(auto& v : pt) {
			if(v.first == "properties") {
				auto props = parseProperties(v.second);
				res.setProperties(&props);
			} else if(v.first == "image") {
				res.addImage(parseImageElement(v.second));
			} else if(v.first == "objectgroup") {
				// XXX
				ASSERT_LOG(false, "XXX implement objectgroup parsing.");
			}
		}
		return res;
	}

	std::shared_ptr<Layer> TmxReader::parseLayerElement(const boost::property_tree::ptree& pt)
	{
		auto attributes = pt.get_child("<xmlattr>");
		const std::string name = attributes.get_child("name").data();
		std::shared_ptr<Layer> res = std::make_shared<Layer>(map_, name);
		auto opacity = attributes.get_child_optional("opacity");
		if(opacity) {
			res->setOpacity(opacity->get_value<float>());
		}
		auto visible = attributes.get_child_optional("visible");
		if(visible) {
			res->setVisibility(visible->get_value<int>() != 0 ? true : false);
		}
		for(auto& v : pt) {
			if(v.first == "properties") {
				auto props = parseProperties(v.second);
				res->setProperties(&props);
			} else if(v.first == "data") {
				int row = 0;
				int col = 0;
				for(auto n : parseDataElement(v.second)) {
					const uint32_t tile_gid = n & flip_mask;
					const bool flipped_h = n & flipped_horizontally_bit ? true : false;
					const bool flipped_v = n & flipped_vertically_bit ? true : false;
					const bool flipped_d = n & flipped_diagonally_bit ? true : false;
					if(tile_gid != 0) {
						auto t = map_->createTileInstance(col, row, tile_gid);
						t->setFlipFlags(flipped_h, flipped_v, flipped_d);
						res->addTile(t);
					}

					if(++col >= map_->getWidth()) {
						col = 0;
						++row;
					}
				}
			}
		}
		return res;
	}
}
