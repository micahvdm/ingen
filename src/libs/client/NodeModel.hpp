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

#ifndef NODEMODEL_H
#define NODEMODEL_H

#include <cstdlib>
#include <iostream>
#include <string>
#include <sigc++/sigc++.h>
#include <raul/Table.hpp>
#include <raul/Path.hpp>
#include <raul/SharedPtr.hpp>
#include "interface/Node.hpp"
#include "interface/Port.hpp"
#include "ObjectModel.hpp"
#include "PortModel.hpp"
#include "PluginModel.hpp"

using std::string;
using Raul::Table;

namespace Ingen {
namespace Client {

class PluginModel;
class Store;

	
/** Node model class, used by the client to store engine's state.
 *
 * \ingroup IngenClient
 */
class NodeModel : public ObjectModel, virtual public Ingen::Shared::Node
{
public:
	virtual ~NodeModel();

	SharedPtr<PortModel> get_port(const string& port_name) const;
	
	Shared::Port* port(uint32_t index) const;
	
	const string&         plugin_uri() const { return _plugin_uri; }
	const Shared::Plugin* plugin()     const { return _plugin.get(); }
	uint32_t              num_ports()  const { return _ports.size(); }
	const PortModelList&  ports()     const { return _ports; }
	
	void port_value_range(SharedPtr<PortModel> port, float& min, float& max);
	
	// Signals
	sigc::signal<void, SharedPtr<PortModel> > signal_new_port; 
	sigc::signal<void, SharedPtr<PortModel> > signal_removed_port; 
	
protected:
	friend class Store;
	
	NodeModel(Store& store, const string& plugin_uri, const Path& path, bool polyphonic);
	NodeModel(Store& store, SharedPtr<PluginModel> plugin, const Path& path, bool polyphonic);

	NodeModel(const Path& path);
	void add_child(SharedPtr<ObjectModel> c);
	bool remove_child(SharedPtr<ObjectModel> c);
	void add_port(SharedPtr<PortModel> pm);
	void remove_port(SharedPtr<PortModel> pm);
	void remove_port(const Path& port_path);
	void add_program(int bank, int program, const string& name);
	void remove_program(int bank, int program);

	virtual void clear();
	
	PortModelList                   _ports;      ///< List of ports (not a Table to preserve order)
	string                          _plugin_uri; ///< Plugin URI (if PluginModel is unknown)
	SharedPtr<PluginModel>          _plugin;     ///< The plugin this node is an instance of
};


typedef Table<string, SharedPtr<NodeModel> > NodeModelMap;


} // namespace Client
} // namespace Ingen

#endif // NODEMODEL_H
