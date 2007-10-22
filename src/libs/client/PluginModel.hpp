/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PLUGINMODEL_H
#define PLUGINMODEL_H

#include CONFIG_H_PATH
#include <string>
#include <iostream>
#include <raul/Path.hpp>
#include <raul/SharedPtr.hpp>
#include <raul/RDFWorld.hpp>
#ifdef HAVE_SLV2
#include <slv2/slv2.h>
#endif
#include "interface/EngineInterface.hpp"
#include "interface/Plugin.hpp"
using std::string; using std::cerr; using std::endl;

namespace Ingen {
namespace Client {

class PatchModel;
class NodeModel;


/** Model for a plugin available for loading.
 *
 * \ingroup IngenClient
 */
class PluginModel : public Ingen::Shared::Plugin
{
public:
	PluginModel(const string& uri, const string& type_uri, const string& symbol, const string& name)
		: _type(type_from_uri(type_uri))
		, _uri(uri)
		, _symbol(symbol)
		, _name(name)
	{
#ifdef HAVE_SLV2
		_slv2_plugin = slv2_plugins_get_by_uri(_slv2_plugins, uri.c_str());
#endif
	}
	
	Type          type() const { return _type; }
	const string& uri()  const { return _uri; }
	const string& name() const { return _name; }
	
	/** DEPRECATED */
	Type type_from_string(const string& type_string) {
		if (type_string == "LV2") return LV2;
		else if (type_string == "LADSPA") return LADSPA;
		else if (type_string == "Internal") return Internal;
		else if (type_string == "Patch") return Patch;
		else return Internal; // ?
	}
	
	Type type_from_uri(const string& type_uri) {
		if (type_uri.substr(0, 6) != "ingen:") {
			cerr << "INVALID TYPE STRING!" << endl;
			return Plugin::Internal; // ?
		} else {
			return type_from_string(type_uri.substr(6));
		}
	}

	string default_node_name(SharedPtr<PatchModel> parent);

#ifdef HAVE_SLV2
	SLV2Plugin       slv2_plugin() { return _slv2_plugin; }
	static SLV2World slv2_world()  { return _slv2_world; }

	static void set_slv2_world(SLV2World world) {
		_slv2_world = world; 
		_slv2_plugins = slv2_world_get_all_plugins(_slv2_world);
	}

	SLV2UIInstance ui(Ingen::Shared::EngineInterface* engine, NodeModel* node) const;

	const string& icon_path() const;
	static string get_lv2_icon_path(SLV2Plugin plugin);
#endif

	static void set_rdf_world(Raul::RDF::World& world) {
		_rdf_world = &world;
	}

	static Raul::RDF::World* rdf_world() { return _rdf_world; }

private:
	const Type   _type;
	const string _uri;
	const string _symbol;
	const string _name;

#ifdef HAVE_SLV2
	static SLV2World   _slv2_world;
	static SLV2Plugins _slv2_plugins;

	SLV2Plugin _slv2_plugin;
	mutable string _icon_path;
#endif

	static Raul::RDF::World* _rdf_world;
};


} // namespace Client
} // namespace Ingen

#endif // PLUGINMODEL_H

