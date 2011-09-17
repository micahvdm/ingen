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

#include "raul/log.hpp"
#include "raul/PathTable.hpp"

#include "shared/LV2URIMap.hpp"

#include "ingen/client/ClientStore.hpp"
#include "ingen/client/ConnectionModel.hpp"
#include "ingen/client/NodeModel.hpp"
#include "ingen/client/ObjectModel.hpp"
#include "ingen/client/PatchModel.hpp"
#include "ingen/client/PluginModel.hpp"
#include "ingen/client/PortModel.hpp"
#include "ingen/client/SigClientInterface.hpp"

#define LOG(s) s << "[ClientStore] "

//#define INGEN_CLIENT_STORE_DUMP 1

using namespace std;
using namespace Raul;

namespace Ingen {

using namespace Shared;

namespace Client {

ClientStore::ClientStore(SharedPtr<Shared::LV2URIMap>  uris,
                         SharedPtr<ServerInterface>    engine,
                         SharedPtr<SigClientInterface> emitter)
	: _uris(uris)
	, _engine(engine)
	, _emitter(emitter)
	, _plugins(new Plugins())
{
	if (!emitter)
		return;

#define CONNECT(signal, method) \
	emitter->signal_##signal().connect( \
		sigc::mem_fun(this, &ClientStore::method));

	CONNECT(object_deleted, del);
	CONNECT(object_moved, move);
	CONNECT(put, put);
	CONNECT(delta, delta);
	CONNECT(connection, connect);
	CONNECT(disconnection, disconnect);
	CONNECT(disconnect_all, disconnect_all);
	CONNECT(property_change, set_property);
	CONNECT(activity, activity);
}

void
ClientStore::clear()
{
	Store::clear();
	_plugins->clear();
}

void
ClientStore::add_object(SharedPtr<ObjectModel> object)
{
	// If we already have "this" object, merge the existing one into the new
	// one (with precedence to the new values).
	iterator existing = find(object->path());
	if (existing != end()) {
		PtrCast<ObjectModel>(existing->second)->set(object);
	} else {
		if (!object->path().is_root()) {
			SharedPtr<ObjectModel> parent = _object(object->path().parent());
			if (parent) {
				assert(object->path().is_child_of(parent->path()));
				object->set_parent(parent);
				parent->add_child(object);
				assert(parent && (object->parent() == parent));

				(*this)[object->path()] = object;
				_signal_new_object.emit(object);
			}
		} else {
			(*this)[object->path()] = object;
			_signal_new_object.emit(object);
		}

	}

	typedef Resource::Properties::const_iterator Iterator;
	for (Iterator i = object->properties().begin();
	     i != object->properties().end(); ++i)
		object->signal_property().emit(i->first, i->second);

	LOG(debug) << "Added " << object->path() << " {" << endl;
	for (iterator i = begin(); i != end(); ++i) {
		LOG(debug) << "\t" << i->first << endl;
	}
	LOG(debug) << "}" << endl;
}

SharedPtr<ObjectModel>
ClientStore::remove_object(const Path& path)
{
	iterator i = find(path);

	if (i != end()) {
		assert((*i).second->path() == path);
		SharedPtr<ObjectModel>    result  = PtrCast<ObjectModel>((*i).second);
		iterator                  end     = find_descendants_end(i);
		SharedPtr<Store::Objects> removed = yank(i, end);

		LOG(debug) << "Removing " << i->first << " {" << endl;
		for (iterator i = removed->begin(); i != removed->end(); ++i) {
			LOG(debug) << "\t" << i->first << endl;
		}
		LOG(debug) << "}" << endl;

		if (result)
			result->signal_destroyed().emit();

		if (!result->path().is_root()) {
			assert(result->parent());

			SharedPtr<ObjectModel> parent = _object(result->path().parent());
			if (parent) {
				parent->remove_child(result);
			}
		}

		assert(!object(path));

		return result;

	} else {
		return SharedPtr<ObjectModel>();
	}
}

SharedPtr<PluginModel>
ClientStore::_plugin(const URI& uri)
{
	assert(uri.length() > 0);
	Plugins::iterator i = _plugins->find(uri);
	if (i == _plugins->end())
		return SharedPtr<PluginModel>();
	else
		return (*i).second;
}

SharedPtr<const PluginModel>
ClientStore::plugin(const Raul::URI& uri) const
{
	return const_cast<ClientStore*>(this)->_plugin(uri);
}

SharedPtr<ObjectModel>
ClientStore::_object(const Path& path)
{
	assert(path.length() > 0);
	iterator i = find(path);
	if (i == end()) {
		return SharedPtr<ObjectModel>();
	} else {
		SharedPtr<ObjectModel> model = PtrCast<ObjectModel>(i->second);
		assert(model);
		assert(model->path().is_root() || model->parent());
		return model;
	}
}

SharedPtr<const ObjectModel>
ClientStore::object(const Path& path) const
{
	return const_cast<ClientStore*>(this)->_object(path);
}

SharedPtr<Resource>
ClientStore::_resource(const URI& uri)
{
	if (Path::is_path(uri))
		return _object(uri.str());
	else
		return _plugin(uri);
}

SharedPtr<const Resource>
ClientStore::resource(const Raul::URI& uri) const
{
	return const_cast<ClientStore*>(this)->_resource(uri);
}

void
ClientStore::add_plugin(SharedPtr<PluginModel> pm)
{
	SharedPtr<PluginModel> existing = _plugin(pm->uri());
	if (existing) {
		existing->set(pm);
	} else {
		_plugins->insert(make_pair(pm->uri(), pm));
		_signal_new_plugin.emit(pm);
	}
}

/* ****** Signal Handlers ******** */

void
ClientStore::del(const URI& uri)
{
	if (!Raul::Path::is_path(uri))
		return;

	const Raul::Path path(uri.str());
	SharedPtr<ObjectModel> removed = remove_object(path);
	removed.reset();
	LOG(debug) << "Removed object " << path << ", count: " << removed.use_count();
}

void
ClientStore::move(const Path& old_path_str, const Path& new_path_str)
{
	Path old_path(old_path_str);
	Path new_path(new_path_str);

	iterator parent = find(old_path);
	if (parent == end()) {
		LOG(error) << "Failed to find object " << old_path << " to move." << endl;
		return;
	}

	typedef Table<Path, SharedPtr<GraphObject> > Removed;

	iterator           end     = find_descendants_end(parent);
	SharedPtr<Removed> removed = yank(parent, end);

	assert(removed->size() > 0);

	typedef Table<Path, SharedPtr<GraphObject> > PathTable;
	for (PathTable::iterator i = removed->begin(); i != removed->end(); ++i) {
		const Path& child_old_path = i->first;
		assert(Path::descendant_comparator(old_path, child_old_path));

		Path child_new_path;
		if (child_old_path == old_path)
			child_new_path = new_path;
		else
			child_new_path = new_path.base() + child_old_path.substr(old_path.length()+1);

		LOG(info) << "Renamed " << child_old_path << " -> " << child_new_path << endl;
		PtrCast<ObjectModel>(i->second)->set_path(child_new_path);
		i->first = child_new_path;
	}

	cram(*removed.get());
}

void
ClientStore::put(const URI&                  uri,
                 const Resource::Properties& properties,
                 Resource::Graph             ctx)
{
	typedef Resource::Properties::const_iterator Iterator;
#ifdef INGEN_CLIENT_STORE_DUMP
	LOG(info) << "PUT " << uri << " {" << endl;
	for (Iterator i = properties.begin(); i != properties.end(); ++i)
		LOG(info) << '\t' << i->first << " = " << i->second
		          << " :: " << i->second.type() << endl;
	LOG(info) << "}" << endl;
#endif

	bool is_patch, is_node, is_port, is_output;
	PortType data_type(PortType::UNKNOWN);
	ResourceImpl::type(uris(), properties,
	                   is_patch, is_node, is_port, is_output,
	                   data_type);
	// Check if uri is a plugin
	const Atom& type = properties.find(_uris->rdf_type)->second;
	if (type.type() == Atom::URI) {
		const URI&         type_uri    = type.get_uri();
		const Plugin::Type plugin_type = Plugin::type_from_uri(type_uri);
		if (plugin_type == Plugin::Patch) {
			is_patch = true;
		} else if (plugin_type != Plugin::NIL) {
			SharedPtr<PluginModel> p(new PluginModel(uris(), uri, type_uri, properties));
			add_plugin(p);
			return;
		}
	}

	if (!Path::is_valid(uri.str())) {
		LOG(error) << "Bad path `" << uri.str() << "'" << endl;
		return;
	}

	const Path path(uri.str());

	SharedPtr<ObjectModel> obj = PtrCast<ObjectModel>(_object(path));
	if (obj) {
		obj->set_properties(properties);
		return;
	}

	if (path.is_root()) {
		is_patch = true;
	}

	if (is_patch) {
		SharedPtr<PatchModel> model(new PatchModel(uris(), path));
		model->set_properties(properties);
		add_object(model);
	} else if (is_node) {
		const Iterator p = properties.find(_uris->rdf_instanceOf);
		SharedPtr<PluginModel> plug;
		if (p->second.is_valid() && p->second.type() == Atom::URI) {
			if (!(plug = _plugin(p->second.get_uri()))) {
				LOG(warn) << "Unable to find plugin " << p->second.get_uri() << endl;
				plug = SharedPtr<PluginModel>(
					new PluginModel(uris(),
					                p->second.get_uri(),
					                _uris->ingen_nil,
					                Resource::Properties()));
				add_plugin(plug);
			}

			SharedPtr<NodeModel> n(new NodeModel(uris(), plug, path));
			n->set_properties(properties);
			add_object(n);
		} else {
			LOG(warn) << "Node " << path << " has no plugin" << endl;
		}
	} else if (is_port) {
		if (data_type != PortType::UNKNOWN) {
			PortModel::Direction pdir = is_output ? PortModel::OUTPUT : PortModel::INPUT;
			const Iterator i = properties.find(_uris->lv2_index);
			if (i != properties.end() && i->second.type() == Atom::INT) {
				const uint32_t index = i->second.get_int32();
				SharedPtr<PortModel> p(
					new PortModel(uris(), path, index, data_type, pdir));
				p->set_properties(properties);
				add_object(p);
			} else {
				LOG(error) << "Port " << path << " has no index" << endl;
			}
		} else {
			LOG(warn) << "Port " << path << " has no type" << endl;
		}
	} else {
		LOG(warn) << "Ignoring object " << path << " with unknown type "
			<< is_patch << " " << is_node << " " << is_port << endl;
	}
}

void
ClientStore::delta(const URI&                  uri,
                   const Resource::Properties& remove,
                   const Resource::Properties& add)
{
	typedef Resource::Properties::const_iterator iterator;
#ifdef INGEN_CLIENT_STORE_DUMP
	LOG(info) << "DELTA " << uri << " {" << endl;
	for (iterator i = remove.begin(); i != remove.end(); ++i)
		LOG(info) << "    - " << i->first << " = " << i->second
		          << " :: " << i->second.type() << endl;
	for (iterator i = add.begin(); i != add.end(); ++i)
		LOG(info) << "    + " << i->first << " = " << i->second
		          << " :: " << i->second.type() << endl;
	LOG(info) << "}" << endl;
#endif

	if (!Path::is_valid(uri.str())) {
		LOG(error) << "Bad path `" << uri.str() << "'" << endl;
		return;
	}

	const Path path(uri.str());

	SharedPtr<ObjectModel> obj = _object(path);
	if (obj) {
		obj->remove_properties(remove);
		obj->add_properties(add);
	} else {
		LOG(warn) << "Failed to find object `" << path << "'" << endl;
	}
}

void
ClientStore::set_property(const URI& subject_uri, const URI& predicate, const Atom& value)
{
	if (subject_uri == _uris->ingen_engine) {
		LOG(info) << "Engine property " << predicate << " = " << value << endl;
		return;
	}
	SharedPtr<Resource> subject = _resource(subject_uri);
	if (subject) {
		subject->set_property(predicate, value);
	} else {
		SharedPtr<PluginModel> plugin = _plugin(subject_uri);
		if (plugin)
			plugin->set_property(predicate, value);
		else
			LOG(warn) << "Property '" << predicate << "' for unknown object "
			          << subject_uri << endl;
	}
}

void
ClientStore::activity(const Path& path)
{
	SharedPtr<PortModel> port = PtrCast<PortModel>(_object(path));
	if (port)
		port->signal_activity().emit();
	else
		LOG(error) << "Activity for non-existent port " << path << endl;
}

SharedPtr<PatchModel>
ClientStore::connection_patch(const Path& src_port_path, const Path& dst_port_path)
{
	SharedPtr<PatchModel> patch;

	if (src_port_path.parent() == dst_port_path.parent())
		patch = PtrCast<PatchModel>(_object(src_port_path.parent()));

	if (!patch && src_port_path.parent() == dst_port_path.parent().parent())
		patch = PtrCast<PatchModel>(_object(src_port_path.parent()));

	if (!patch && src_port_path.parent().parent() == dst_port_path.parent())
		patch = PtrCast<PatchModel>(_object(dst_port_path.parent()));

	if (!patch)
		patch = PtrCast<PatchModel>(_object(src_port_path.parent().parent()));

	if (!patch)
		LOG(error) << "Unable to find connection patch " << src_port_path
			<< " -> " << dst_port_path << endl;

	return patch;
}

bool
ClientStore::attempt_connection(const Path& src_port_path,
                                const Path& dst_port_path)
{
	SharedPtr<PortModel> src_port = PtrCast<PortModel>(_object(src_port_path));
	SharedPtr<PortModel> dst_port = PtrCast<PortModel>(_object(dst_port_path));

	if (src_port && dst_port) {
		SharedPtr<PatchModel>      patch = connection_patch(src_port_path, dst_port_path);
		SharedPtr<ConnectionModel> cm(new ConnectionModel(src_port, dst_port));

		src_port->connected_to(dst_port);
		dst_port->connected_to(src_port);

		patch->add_connection(cm);
		return true;
	}

	return false;
}

void
ClientStore::connect(const Path& src_path,
                     const Path& dst_path)
{
	attempt_connection(src_path, dst_path);
}

void
ClientStore::disconnect(const URI& src,
                        const URI& dst)
{
	if (!Path::is_path(src) && !Path::is_path(dst)) {
		std::cerr << "Bad disconnect notification " << src << " => " << dst << std::endl;
		return;
	}

	const Path src_path(src.str());
	const Path dst_path(dst.str());

	SharedPtr<PortModel> src_port = PtrCast<PortModel>(_object(src_path));
	SharedPtr<PortModel> dst_port = PtrCast<PortModel>(_object(dst_path));

	if (src_port)
		src_port->disconnected_from(dst_port);

	if (dst_port)
		dst_port->disconnected_from(src_port);

	SharedPtr<PatchModel> patch = connection_patch(src_path, dst_path);
	if (patch)
		patch->remove_connection(src_port.get(), dst_port.get());
}

void
ClientStore::disconnect_all(const Raul::Path& parent_patch_path,
                            const Raul::Path& path)
{
	SharedPtr<PatchModel>  patch  = PtrCast<PatchModel>(_object(parent_patch_path));
	SharedPtr<ObjectModel> object = _object(path);

	if (!patch || !object) {
		std::cerr << "Bad disconnect all notification " << path
		          << " in " << parent_patch_path << std::endl;
		return;
	}

	const PatchModel::Connections connections = patch->connections();
	for (PatchModel::Connections::const_iterator i = connections.begin();
	     i != connections.end(); ++i) {
		SharedPtr<ConnectionModel> c = PtrCast<ConnectionModel>(i->second);
		if (c->src_port()->parent() == object
		    || c->dst_port()->parent() == object
		    || c->src_port()->path() == path
		    || c->dst_port()->path() == path) {
			c->src_port()->disconnected_from(c->dst_port());
			c->dst_port()->disconnected_from(c->src_port());
			patch->remove_connection(c->src_port().get(), c->dst_port().get());
		}
	}
}

} // namespace Client
} // namespace Ingen

