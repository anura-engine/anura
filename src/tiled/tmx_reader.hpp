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

// Built based on information from https://github.com/bjorn/tiled/wiki/TMX-Map-Format

#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>


#include <string>

#include "tiled.hpp"

namespace tiled
{
	class TmxReader
	{
	public:
		TmxReader(MapPtr map);
		void parseFile(const std::string& filename);
		void parseString(const std::string& filename);
	private:
		void parseMapElement(const boost::property_tree::ptree& pt);
		void parseTileset(const boost::property_tree::ptree& pt);
		std::vector<Property> parseProperties(const boost::property_tree::ptree& pt);
		TileImage parseImageElement(const boost::property_tree::ptree& pt);
		std::vector<char> parseImageDataElement(const boost::property_tree::ptree& pt);
		std::vector<uint32_t> parseDataElement(const boost::property_tree::ptree& pt);
		std::vector<Terrain> parseTerrainTypes(const boost::property_tree::ptree& pt);
		TileDefinition parseTileElement(const TileSet& ts, const boost::property_tree::ptree& pt);
		std::shared_ptr<Layer> parseLayerElement(const boost::property_tree::ptree& pt);
		MapPtr map_;
	};
}


