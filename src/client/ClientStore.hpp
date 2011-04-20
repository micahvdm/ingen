/* This file is part of Ingen.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
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

#ifndef INGEN_CLIENT_CLIENTSTORE_HPP
#define INGEN_CLIENT_CLIENTSTORE_HPP

#include <cassert>
#include <list>
#include <string>

#include "raul/SharedPtr.hpp"

#include <sigc++/sigc++.h>

#include "ingen/ServerInterface.hpp"
#include "raul/Path.hpp"
#include "raul/PathTable.hpp"
#include "raul/TableImpl.hpp"
#include "shared/Store.hpp"

namespace Raul { class Atom; }

namespace Ingen {

class GraphObject;

namespace Client {

class NodeModel;
class ObjectModel;
class PatchModel;
class PluginModel;
class PortModel;
class SigClientInterface;

/** Automatically manages models of objects in the engine.
 *
 * \ingroup IngenClient
 */
class ClientStore : public Shared::Store
                  , public CommonInterface
                  , public sigc::trackable {
public:
	ClientStore(
		SharedPtr<Shared::LV2URIMap>  uris,
		SharedPtr<ServerInterface>    engine=SharedPtr<ServerInterface>(),
		SharedPtr<SigClientInterface> emitter=SharedPtr<SigClientInterface>());

	SharedPtr<PluginModel>  plugin(const Raul::URI& uri);
	SharedPtr<ObjectModel>  object(const Raul::Path& path);
	SharedPtr<Resource>     resource(const Raul::URI& uri);

	void clear();

	typedef Raul::Table<Raul::URI, SharedPtr<PluginModel> > Plugins;
	SharedPtr<const Plugins> plugins() const                   { return _plugins; }
	SharedPtr<Plugins>       plugins()                         { return _plugins; }
	void                     set_plugins(SharedPtr<Plugins> p) { _plugins = p; }

	Shared::LV2URIMap& uris() { return *_uris.get(); }

	// CommonInterface
	bool new_object(const GraphObject* object);

	void put(const Raul::URI&            uri,
	         const Resource::Properties& properties,
	         Resource::Graph             ctx=Resource::DEFAULT);

	void delta(const Raul::URI&            uri,
	           const Resource::Properties& remove,
	           const Resource::Properties& add);

	void move(const Raul::Path& old_path,
	          const Raul::Path& new_path);

	void set_property(const Raul::URI&  subject_path,
	                  const Raul::URI&  predicate,
	                  const Raul::Atom& value);

	void connect(const Raul::Path& src_port_path,
	             const Raul::Path& dst_port_path);

	void disconnect(const Raul::URI& src,
	                const Raul::URI& dst);

	void disconnect_all(const Raul::Path& parent_patch_path,
	                    const Raul::Path& path);

	void del(const Raul::URI& uri);

	sigc::signal< void, SharedPtr<ObjectModel> > signal_new_object;
	sigc::signal< void, SharedPtr<PluginModel> > signal_new_plugin;

private:
	void add(GraphObject* o) { throw; }

	void add_object(SharedPtr<ObjectModel> object);
	SharedPtr<ObjectModel> remove_object(const Raul::Path& path);

	void add_plugin(SharedPtr<PluginModel> plugin);

	SharedPtr<PatchModel> connection_patch(const Raul::Path& src_port_path,
	                                       const Raul::Path& dst_port_path);

	void bundle_begin() {}
	void bundle_end()   {}

	// Slots for SigClientInterface signals
	void object_moved(const Raul::Path& old_path, const Raul::Path& new_path);
	void activity(const Raul::Path& path);

	bool attempt_connection(const Raul::Path& src_port_path,
	                        const Raul::Path& dst_port_path);

	SharedPtr<Shared::LV2URIMap>  _uris;
	SharedPtr<ServerInterface>    _engine;
	SharedPtr<SigClientInterface> _emitter;

	SharedPtr<Plugins> _plugins; ///< Map, keyed by plugin URI
};

} // namespace Client
} // namespace Ingen

#endif // INGEN_CLIENT_CLIENTSTORE_HPP
