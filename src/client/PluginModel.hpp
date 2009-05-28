/* This file is part of Ingen.
 * Copyright (C) 2007-2009 Dave Robillard <http://drobilla.net>
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

#include "ingen-config.h"
#include <iostream>
#include "raul/SharedPtr.hpp"
#include "redlandmm/World.hpp"
#ifdef HAVE_SLV2
#include "slv2/slv2.h"
#endif
#include "interface/EngineInterface.hpp"
#include "interface/Plugin.hpp"
#include "module/World.hpp"
#include "shared/ResourceImpl.hpp"

namespace Ingen {
namespace Client {

class PatchModel;
class NodeModel;
class PluginUI;


/** Model for a plugin available for loading.
 *
 * \ingroup IngenClient
 */
class PluginModel : public Ingen::Shared::Plugin
                  , public Ingen::Shared::ResourceImpl
{
public:
	PluginModel(
			const Raul::URI&                    uri,
			const Raul::URI&                    type_uri,
			const Shared::Resource::Properties& properties);

	Type type() const { return _type; }

	virtual const Raul::Atom& get_property(const Raul::URI& key) const;

	Raul::Symbol default_node_symbol();
	std::string  human_name();
	std::string  port_human_name(uint32_t index) const;

#ifdef HAVE_SLV2
	static SLV2World slv2_world()        { return _slv2_world; }
	SLV2Plugin       slv2_plugin() const { return _slv2_plugin; }

	SLV2Port slv2_port(uint32_t index) {
		Glib::Mutex::Lock lock(_rdf_world->mutex());
		return slv2_plugin_get_port_by_index(_slv2_plugin, index);
	}

	static void set_slv2_world(SLV2World world) {
		Glib::Mutex::Lock lock(_rdf_world->mutex());
		_slv2_world = world;
		_slv2_plugins = slv2_world_get_all_plugins(_slv2_world);
	}

	bool has_ui() const;

	SharedPtr<PluginUI> ui(Ingen::Shared::World* world,
	                       SharedPtr<NodeModel>  node) const;

	const std::string& icon_path() const;
	static std::string get_lv2_icon_path(SLV2Plugin plugin);
#endif

	static void set_rdf_world(Redland::World& world) {
		_rdf_world = &world;
	}

	static Redland::World* rdf_world() { return _rdf_world; }

private:
	const Type _type;

#ifdef HAVE_SLV2
	static SLV2World   _slv2_world;
	static SLV2Plugins _slv2_plugins;

	SLV2Plugin          _slv2_plugin;
	mutable std::string _icon_path;
#endif

	static Redland::World* _rdf_world;
};


} // namespace Client
} // namespace Ingen

#endif // PLUGINMODEL_H

