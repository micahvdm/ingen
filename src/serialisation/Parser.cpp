/* This file is part of Ingen.
 * Copyright (C) 2007-2009 David Robillard <http://drobilla.net>
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

#include <locale.h>

#include <set>

#include <boost/format.hpp>

#include <glibmm/convert.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>

#include "raul/Atom.hpp"
#include "raul/AtomRDF.hpp"
#include "raul/TableImpl.hpp"
#include "raul/log.hpp"

#include "serd/serd.h"
#include "sord/sordmm.hpp"

#include "interface/EngineInterface.hpp"
#include "module/World.hpp"
#include "shared/LV2URIMap.hpp"

#include "Parser.hpp"
#include "names.hpp"

#define LOG(s) s << "[Parser] "

#define NS_INGEN "http://drobilla.net/ns/ingen#"
#define NS_LV2   "http://lv2plug.in/ns/lv2core#"
#define NS_RDF   "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_RDFS  "http://www.w3.org/2000/01/rdf-schema#"

using namespace std;
using namespace Raul;
using namespace Ingen::Shared;

static const Sord::Node nil;

typedef set<Sord::Node> RDFNodes;

namespace Ingen {
namespace Serialisation {

static Glib::ustring
relative_uri(Glib::ustring base, const Glib::ustring uri, bool leading_slash)
{
	if (uri == base) {
		return leading_slash ? "/" : "";
	}

	SerdURI base_uri;
	serd_uri_parse((const uint8_t*)base.c_str(), &base_uri);

	SerdURI  normal_base_uri;
	SerdNode normal_base_uri_node = serd_node_new_uri_from_string(
		(const uint8_t*)".", &base_uri, &normal_base_uri);

	Glib::ustring normal_base_str((const char*)normal_base_uri_node.buf);

	Glib::ustring ret = uri;
	if (uri.length() >= normal_base_str.length()
	    && uri.substr(0, normal_base_str.length()) == normal_base_str) {
		ret = uri.substr(normal_base_str.length());
		if (leading_slash && ret[0] != '/')
			ret = Glib::ustring("/") + ret;
	}

	return ret;
}

static void
normalise_uri(Glib::ustring& uri)
{
	size_t dotslash = string::npos;
	while ((dotslash = uri.find("./")) != string::npos)
		uri = uri.substr(0, dotslash) + uri.substr(dotslash + 2);
}

Parser::PatchRecords
Parser::find_patches(Ingen::Shared::World* world,
                     const Glib::ustring&  manifest_uri)
{
	Sord::Model model(*world->rdf_world(), manifest_uri);
	model.load_file(manifest_uri);

	Sord::URI rdf_type(*world->rdf_world(),     NS_RDF   "type");
	Sord::URI rdfs_seeAlso(*world->rdf_world(), NS_RDFS  "seeAlso");
	Sord::URI ingen_Patch(*world->rdf_world(),  NS_INGEN "Patch");

	RDFNodes patches;
	for (Sord::Iter i = model.find(nil, rdf_type, ingen_Patch); !i.end(); ++i) {
		patches.insert(i.get_subject());
	}

	std::list<PatchRecord> records;
	for (RDFNodes::const_iterator i = patches.begin(); i != patches.end(); ++i) {
		Sord::Iter f = model.find(*i, rdfs_seeAlso, nil);
		if (f.end()) {
			LOG(error) << "Patch has no rdfs:seeAlso" << endl;
			continue;
		}
		records.push_back(PatchRecord(i->to_c_string(),
		                              f.get_object().to_c_string()));
	}

	return records;
}

/** Parse a patch from RDF into a CommonInterface (engine or client).
 * @return whether or not load was successful.
 */
bool
Parser::parse_file(Ingen::Shared::World*                    world,
                   Ingen::Shared::CommonInterface*          target,
                   Glib::ustring                            file_uri,
                   boost::optional<Raul::Path>              parent,
                   boost::optional<Raul::Symbol>            symbol,
                   boost::optional<GraphObject::Properties> data)
{
	normalise_uri(file_uri);

	const size_t  colon = file_uri.find(":");
	Glib::ustring file_path = file_uri;
	if (colon != Glib::ustring::npos) {
		const Glib::ustring scheme = file_uri.substr(0, colon);
		if (scheme != "file") {
			LOG(error) << (boost::format("Unsupported URI scheme `%1%'") % scheme) << endl;
			return false;
		}
		if (file_uri.substr(0, 7) == "file://") {
			file_path = file_uri.substr(7);
		} else {
			file_path = file_uri.substr(5);
		}
	}

	std::string filename = Glib::filename_from_uri(file_uri);

	if (file_uri.substr(file_uri.length() - 4) != ".ttl") {
		// Not a Turtle file, maybe a bundle, check for manifest
		if (file_uri[file_uri.length() - 1] != '/') {
			file_uri.append("/");
		}
		Parser::PatchRecords records = find_patches(world, file_uri + "manifest.ttl");
		if (!records.empty()) {
			filename = Glib::filename_from_uri(records.front().file_uri);
		}
	}

	Sord::Model model(*world->rdf_world(), filename);
	model.load_file(filename);

	LOG(info) << "Parsing " << file_uri << endl;
	if (parent)
		LOG(info) << "Parent: " << *parent << endl;
	if (symbol)
		LOG(info) << "Symbol: " << *symbol << endl;

	boost::optional<Path> parsed_path
		= parse(world, target, model, filename, Path("/"), parent, symbol, data);

	if (parsed_path) {
		target->set_property(*parsed_path, "http://drobilla.net/ns/ingen#document",
		                     Atom(Atom::URI, file_uri.c_str()));
	} else {
		LOG(warn) << "Document URI lost" << endl;
	}

	return parsed_path;
}

bool
Parser::parse_string(Ingen::Shared::World*                    world,
                     Ingen::Shared::CommonInterface*          target,
                     const Glib::ustring&                     str,
                     const Glib::ustring&                     base_uri,
                     boost::optional<Raul::Path>              data_path,
                     boost::optional<Raul::Path>              parent,
                     boost::optional<Raul::Symbol>            symbol,
                     boost::optional<GraphObject::Properties> data)
{
	Sord::Model model(*world->rdf_world(), base_uri);
	model.load_string(str.c_str(), str.length(), base_uri);

	LOG(info) << "Parsing " << (data_path ? data_path->str() : "*") << " from string";
	if (!base_uri.empty())
		info << " (base " << base_uri << ")";
	info << endl;

	bool ret = parse(world, target, model, base_uri, data_path, parent, symbol, data);
	Sord::URI subject(*world->rdf_world(), base_uri);
	parse_connections(world, target, model, subject, parent ? *parent : "/");

	return ret;
}

bool
Parser::parse_update(Ingen::Shared::World*                    world,
                     Shared::CommonInterface*                 target,
                     const Glib::ustring&                     str,
                     const Glib::ustring&                     base_uri,
                     boost::optional<Raul::Path>              data_path,
                     boost::optional<Raul::Path>              parent,
                     boost::optional<Raul::Symbol>            symbol,
                     boost::optional<GraphObject::Properties> data)
{
#if 0
	Sord::Model model(*world->rdf_world(), str.c_str(), str.length(), base_uri);

	// Delete anything explicitly declared to not exist
	Glib::ustring query_str = Glib::ustring("SELECT DISTINCT ?o WHERE { ?o a owl:Nothing }");
	Sord::Query query(*world->rdf_world(), query_str);
	SharedPtr<Sord::QueryResults> results(query.run(*world->rdf_world(), model, base_uri));

	for (; !results->finished(); results->next()) {
		const Sord::Node& object = results->get("o");
		target->del(object.to_string());
	}

	// Properties
	query = Sord::Query(*world->rdf_world(),
	                    "SELECT DISTINCT ?s ?p ?o WHERE {\n"
	                    "?s ?p ?o .\n"
	                    "}");

	results = query.run(*world->rdf_world(), model, base_uri);
	for (; !results->finished(); results->next()) {
		Glib::Mutex::Lock lock(world->rdf_world()->mutex());
		string               obj_uri(results->get("s").to_string());
		const string         key(results->get("p").to_string());
		const Sord::Node& val_node(results->get("o"));
		const Atom           a(AtomRDF::node_to_atom(model, val_node));
		if (obj_uri.find(":") == string::npos)
			obj_uri = Path(obj_uri).str();
		obj_uri = relative_uri(base_uri, obj_uri, true);
		if (!key.empty())
			target->set_property(string("path:") + obj_uri, key, a);
	}

	// Connections
	Sord::URI subject(*world->rdf_world(), base_uri);
	parse_connections(world, target, model, subject, "/");

	// Port values
	query = Sord::Query(*world->rdf_world(),
	                    "SELECT DISTINCT ?path ?value WHERE {\n"
	                    "?path ingen:value ?value .\n"
	                    "}");

	results = query.run(*world->rdf_world(), model, base_uri);
	for (; !results->finished(); results->next()) {
		Glib::Mutex::Lock lock(world->rdf_world()->mutex());
		const string      obj_path = results->get("path").to_string();
		const Sord::Node& val_node = results->get("value");
		const Atom a(AtomRDF::node_to_atom(model, val_node));
		target->set_property(obj_path, world->uris()->ingen_value, a);
	}

	return parse(world, target, model, base_uri, data_path, parent, symbol, data);
#endif
	return false;
}

boost::optional<Path>
Parser::parse(Ingen::Shared::World*                    world,
              Ingen::Shared::CommonInterface*          target,
              Sord::Model&                             model,
              Glib::ustring                            document_uri,
              boost::optional<Raul::Path>              data_path,
              boost::optional<Raul::Path>              parent,
              boost::optional<Raul::Symbol>            symbol,
              boost::optional<GraphObject::Properties> data)
{
	const Sord::Node::Type res = Sord::Node::URI;

	const Sord::URI rdf_type(*world->rdf_world(), NS_RDF "type");

	const Sord::Node patch_class    (*world->rdf_world(), res, NS_INGEN "Patch");
	const Sord::Node node_class     (*world->rdf_world(), res, NS_INGEN "Node");
	const Sord::Node internal_class (*world->rdf_world(), res, NS_INGEN "Internal");
	const Sord::Node ladspa_class   (*world->rdf_world(), res, NS_INGEN "LADSPAPlugin");
	const Sord::Node in_port_class  (*world->rdf_world(), res, NS_LV2 "InputPort");
	const Sord::Node out_port_class (*world->rdf_world(), res, NS_LV2 "OutputPort");
	const Sord::Node lv2_class      (*world->rdf_world(), res, NS_LV2 "Plugin");

	Sord::Node subject = nil;
	if (data_path && data_path->is_root()) {
		subject = model.base_uri();
	} else if (data_path) {
		subject = Sord::Node(*world->rdf_world(), res, data_path->chop_start("/"));
	} else {
		subject = nil;
	}

	std::string           path_str = data_path ? data_path->chop_scheme() : "/";
	boost::optional<Path> ret;
	boost::optional<Path> root_path;

	for (Sord::Iter i = model.find(subject, rdf_type, nil); !i.end(); ++i) {
		const Sord::Node& subject   = i.get_subject();
		const Sord::Node& rdf_class = i.get_object();

		if (!data_path)
			path_str = relative_uri(document_uri, subject.to_c_string(), true);

		const bool is_plugin =    (rdf_class == ladspa_class)
			|| (rdf_class == lv2_class)
			|| (rdf_class == internal_class);

		const bool is_object =    (rdf_class == patch_class)
			|| (rdf_class == node_class)
			|| (rdf_class == in_port_class)
			|| (rdf_class == out_port_class);

		if (is_object) {
			if (path_str.empty() || path_str[0] != '/')
				path_str = "/" + path_str;

			if (!Path::is_valid(path_str)) {
				LOG(warn) << "Invalid path '" << path_str << "', object skipped" << endl;
				continue;
			}

			string path = (parent && symbol)
				? parent->child(*symbol).str()
				: (parent ? *parent : Path("/")).child(path_str.substr(path_str.find("/")+1)).str();

			if (!Path::is_valid(path)) {
				LOG(warn) << "Invalid path '" << path << "' transformed to /" << endl;
				path = "/";
			}

			if (rdf_class == patch_class) {
				ret = parse_patch(world, target, model, subject, parent, symbol, data);
			} else if (rdf_class == node_class) {
				ret = parse_node(world, target, model, subject, path, data);
			} else if (rdf_class == in_port_class || rdf_class == out_port_class) {
				parse_properties(world, target, model, subject, path, data);
				ret = path;
			}

			if (!ret) {
				LOG(error) << "Failed to parse object " << path << endl;
				return boost::optional<Path>();
			}

			if (data_path && subject.to_string() == data_path->str())
				root_path = ret;

		} else if (is_plugin) {
			string subject_str = subject.to_string();
			if (URI::is_valid(subject_str)) {
				if (subject_str == document_uri)
					subject_str = Path::root().str();
				parse_properties(world, target, model, subject, subject_str);
			}
		}

	}

	return boost::optional<Path>(Path(path_str));
}

boost::optional<Path>
Parser::parse_patch(Ingen::Shared::World*                    world,
                    Ingen::Shared::CommonInterface*          target,
                    Sord::Model&                             model,
                    const Sord::Node&                        subject_node,
                    boost::optional<Raul::Path>              parent,
                    boost::optional<Raul::Symbol>            a_symbol,
                    boost::optional<GraphObject::Properties> data)
{
	const LV2URIMap& uris = *world->uris().get();

	Sord::URI ingen_polyphony(*world->rdf_world(), NS_INGEN "polyphony");
	Sord::URI lv2_port(*world->rdf_world(),        NS_LV2   "port");
	Sord::URI lv2_symbol(*world->rdf_world(),      NS_LV2   "symbol");

	const Sord::Node& patch = subject_node;

	uint32_t patch_poly = 0;

	/* Use parameter overridden polyphony, if given */
	if (data) {
		GraphObject::Properties::iterator poly_param = data.get().find(uris.ingen_polyphony);
		if (poly_param != data.get().end() && poly_param->second.type() == Atom::INT)
			patch_poly = poly_param->second.get_int32();
	}

	/* Load polyphony from file if necessary */
	if (patch_poly == 0) {
		Sord::Iter i = model.find(subject_node, ingen_polyphony, nil);
		if (!i.end()) {
			const Sord::Node& poly_node = i.get_object();
			if (poly_node.is_int())
				patch_poly = poly_node.to_int();
			else
				LOG(warn) << "Patch has non-integer polyphony, assuming 1" << endl;
		}
	}

	/* No polyphony found anywhere, use 1 */
	if (patch_poly == 0)
		patch_poly = 1;

	const Glib::ustring base_uri = model.base_uri().to_string();

	Raul::Symbol symbol = "_";
	if (a_symbol) {
		symbol = *a_symbol;
	} else {
		const std::string basename = Glib::path_get_basename(base_uri);
		symbol = Raul::Symbol::symbolify(basename.substr(0, basename.find('.')));
	}

	string patch_path_str = relative_uri(base_uri, subject_node.to_string(), true);
	if (parent && a_symbol)
		patch_path_str = parent->child(*a_symbol).str();

	if (!Path::is_valid(patch_path_str)) {
		LOG(error) << "Patch has invalid path: " << patch_path_str << endl;
		return boost::optional<Raul::Path>();
	}

	/* Create patch */
	Path patch_path(patch_path_str);
	Resource::Properties props;
	props.insert(make_pair(uris.rdf_type,        Raul::URI(uris.ingen_Patch)));
	props.insert(make_pair(uris.ingen_polyphony, Raul::Atom(int32_t(patch_poly))));
	target->put(patch_path, props);

	Sord::URI rdf_type(*world->rdf_world(),    NS_RDF   "type");
	Sord::URI ingen_Patch(*world->rdf_world(), NS_INGEN "Patch");
	Sord::URI ingen_node(*world->rdf_world(),  NS_INGEN "node");

	typedef multimap<Raul::URI, Raul::Atom> Properties;
	typedef map<string, Sord::Node>         Resources;
	typedef map<string, Properties>         Objects;
	typedef map<string, string>             Types;

	Objects   patch_nodes;
	Objects   plugin_nodes;
	Resources resources;
	Types     types;

	/* For each node in this patch */
	typedef map<Sord::Node, Properties> Nodes;
	Nodes nodes;
	for (Sord::Iter n = model.find(subject_node, ingen_node, nil); !n.end(); ++n) {
		Sord::Node node = n.get_object();

		/* Get all node properties */
		Properties node_properties;
		for (Sord::Iter np = model.find(node, nil, nil); !np.end(); ++np) {
			const Sord::Node& predicate = np.get_predicate();
			const Sord::Node& object    = np.get_object();
			if (!skip_property(predicate)) {
				node_properties.insert(
					make_pair(predicate.to_string(),
					          AtomRDF::node_to_atom(model, object)));
			}
		}

		/* Create node */
		const Path node_path = patch_path.child(
			relative_uri(base_uri, node.to_string(), false));
		target->put(node_path, node_properties);

		/* For each port on this node */
		for (Sord::Iter p = model.find(node, lv2_port, nil); !p.end(); ++p) {
			Sord::Node port = p.get_object();

			/* Get all port properties */
			Properties port_properties;
			for (Sord::Iter pp = model.find(port, nil, nil); !pp.end(); ++pp) {
				const Sord::Node& predicate = pp.get_predicate();
				const Sord::Node& object    = pp.get_object();
				if (!skip_property(predicate)) {
					port_properties.insert(
						make_pair(predicate.to_string(),
						          AtomRDF::node_to_atom(model, object)));
				}
			}

			/* Set port properties */
			Properties::const_iterator s = port_properties.find(uris.lv2_symbol);
			if (s == port_properties.end()) {
				LOG(error) << "Port on " << node_path << " has no symbol" << endl;
				return boost::optional<Path>();
			}

			const Symbol port_sym  = s->second.get_string();
			const Path   port_path = node_path.child(port_sym);
			target->put(port_path, port_properties);
		}
	}

	/* For each port on this patch */
	for (Sord::Iter p = model.find(patch, lv2_port, nil); !p.end(); ++p) {
		Sord::Node port = p.get_object();

		/* Get all port properties */
		Properties port_properties;
		for (Sord::Iter pp = model.find(port, nil, nil); !pp.end(); ++pp) {
			const Sord::Node& predicate = pp.get_predicate();
			const Sord::Node& object    = pp.get_object();
			if (!skip_property(predicate)) {
				port_properties.insert(
					make_pair(predicate.to_string(),
					          AtomRDF::node_to_atom(model, object)));
			}
		}

		/* Set port properties */
		Properties::const_iterator s = port_properties.find(uris.lv2_symbol);
		if (s == port_properties.end()) {
			LOG(error) << "Port on " << patch_path << " has no symbol" << endl;
			return boost::optional<Path>();
		}

		const Symbol port_sym  = s->second.get_string();
		const Path   port_path = patch_path.child(port_sym);
		target->put(port_path, port_properties);
	}

	parse_properties(world, target, model, subject_node, patch_path, data);
	parse_connections(world, target, model, subject_node, patch_path);

	cerr << "FIXME: enable patch" << endl;
	target->set_property(patch_path, uris.ingen_enabled, (bool)true);
#if 0
	/* Enable */
	query = Sord::Query(*world->rdf_world(), Glib::ustring(
		                    "SELECT DISTINCT ?enabled WHERE {\n")
	                    + subject + " ingen:enabled ?enabled .\n"
	                    "}");

	results = query.run(*world->rdf_world(), model, base_uri);
	for (; !results->finished(); results->next()) {
		Glib::Mutex::Lock lock(world->rdf_world()->mutex());
		const Sord::Node& enabled_node = results->get("enabled");
		if (enabled_node.is_bool() && enabled_node) {
			target->set_property(patch_path, uris.ingen_enabled, (bool)true);
			break;
		} else {
			LOG(warn) << "Unknown type for ingen:enabled" << endl;
		}
	}
#endif
	
	return patch_path;
}

boost::optional<Path>
Parser::parse_node(Ingen::Shared::World*                    world,
                   Ingen::Shared::CommonInterface*          target,
                   Sord::Model&                             model,
                   const Sord::Node&                        subject,
                   const Raul::Path&                        path,
                   boost::optional<GraphObject::Properties> data)
{
	const LV2URIMap& uris = *world->uris().get();

	Sord::URI rdf_instanceOf(*world->rdf_world(), NS_RDF "instanceOf");

	/* Get plugin */
	Sord::Iter i = model.find(subject, rdf_instanceOf, nil);
	if (i.end()) {
		LOG(error) << "Node missing mandatory rdf:instanceOf property" << endl;
		return boost::optional<Path>();
	}

	const Sord::Node& plugin_node = i.get_object();
	if (plugin_node.type() != Sord::Node::URI) {
		LOG(error) << "Node's rdf:instanceOf property is not a resource" << endl;
		return boost::optional<Path>();
	}

	Resource::Properties props;
	props.insert(make_pair(uris.rdf_type,
	                       Raul::URI(uris.ingen_Node)));
	props.insert(make_pair(uris.rdf_instanceOf,
	                       AtomRDF::node_to_atom(model, plugin_node)));
	target->put(path, props);

	parse_properties(world, target, model, subject, path, data);
	return path;
}

bool
Parser::parse_connections(Ingen::Shared::World*           world,
                          Ingen::Shared::CommonInterface* target,
                          Sord::Model&                    model,
                          const Sord::Node&               subject,
                          const Raul::Path&               parent)
{
	Sord::URI ingen_connection(*world->rdf_world(),  NS_INGEN "connection");
	Sord::URI ingen_source(*world->rdf_world(),      NS_INGEN "source");
	Sord::URI ingen_destination(*world->rdf_world(), NS_INGEN "destination");

	const Glib::ustring& base_uri = model.base_uri().to_string();

	RDFNodes connections;
	for (Sord::Iter i = model.find(subject, ingen_connection, nil); !i.end(); ++i) {
		connections.insert(i.get_object());
	}

	for (RDFNodes::const_iterator i = connections.begin(); i != connections.end(); ++i) {
		Sord::Iter s = model.find(*i, ingen_source, nil);
		Sord::Iter d = model.find(*i, ingen_destination, nil);

		if (s.end()) {
			LOG(error) << "Connection has no source" << endl;
			return false;
		} else if (d.end()) {
			LOG(error) << "Connection has no destination" << endl;
			return false;
		}

		const Path src_path(
			parent.child(relative_uri(base_uri, s.get_object().to_string(), false)));
		const Path dst_path(
			parent.child(relative_uri(base_uri, d.get_object().to_string(), false)));

		if (!(++s).end()) {
			LOG(error) << "Connection has multiple sources" << endl;
			return false;
		} else if (!(++d).end()) {
			LOG(error) << "Connection has multiple destinations" << endl;
			return false;
		}

		target->connect(src_path, dst_path);
	}

	return true;
}

bool
Parser::parse_properties(Ingen::Shared::World*                    world,
                         Ingen::Shared::CommonInterface*          target,
                         Sord::Model&                             model,
                         const Sord::Node&                        subject,
                         const Raul::URI&                         uri,
                         boost::optional<GraphObject::Properties> data)
{
	Resource::Properties properties;
	for (Sord::Iter i = model.find(subject, nil, nil); !i.end(); ++i) {
		const Sord::Node& key = i.get_predicate();
		const Sord::Node& val = i.get_object();
		if (!skip_property(key)) {
			properties.insert(make_pair(key.to_string(),
			                            AtomRDF::node_to_atom(model, val)));
		}
	}

	target->put(uri, properties);

	// Set passed properties last to override any loaded values
	if (data)
		target->put(uri, data.get());

	return true;
}

bool
Parser::skip_property(const Sord::Node& predicate)
{
	return (predicate.to_string() == "http://drobilla.net/ns/ingen#node"
	        || predicate.to_string() == "http://lv2plug.in/ns/lv2core#port");
}

} // namespace Serialisation
} // namespace Ingen

