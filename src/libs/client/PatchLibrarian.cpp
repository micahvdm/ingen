/* This file is part of Om.  Copyright (C) 2005 Dave Robillard.
 * 
 * Om is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Om is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PatchLibrarian.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include "PatchModel.h"
#include "NodeModel.h"
#include "ModelClientInterface.h"
#include "ConnectionModel.h"
#include "PortModel.h"
#include "PresetModel.h"
#include "OSCModelEngineInterface.h"
#include "PluginModel.h"
#include "util/Path.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility> // for pair, make_pair
#include <cassert>
#include <cstring>
#include <string>
#include <unistd.h> // for usleep
#include <cstdlib>  // for atof
#include <cmath>

using std::string; using std::vector; using std::pair;
using std::cerr; using std::cout; using std::endl;

namespace LibOmClient {

	
/** Searches for the filename passed in the path, returning the full
 * path of the file, or the empty string if not found.
 *
 * This function tries to be as friendly a black box as possible - if the path
 * passed is an absolute path and the file is found there, it will return
 * that path, etc.
 *
 * additional_path is a list (colon delimeted as usual) of additional
 * directories to look in.  ie the directory the parent patch resides in would
 * be a good idea to pass as additional_path, in the case of a subpatch.
 */
string
PatchLibrarian::find_file(const string& filename, const string& additional_path)
{
	string search_path = additional_path + ":" + m_patch_path;
	
	// Try to open the raw filename first
	std::ifstream is(filename.c_str(), std::ios::in);
	if (is.good()) {
		is.close();
		return filename;
	}
	
	string directory;
	string full_patch_path = "";
	
	while (search_path != "") {
		directory = search_path.substr(0, search_path.find(':'));
		if (search_path.find(':') != string::npos)
			search_path = search_path.substr(search_path.find(':')+1);
		else
			search_path = "";

		full_patch_path = directory +"/"+ filename;
		
		std::ifstream is;
		is.open(full_patch_path.c_str(), std::ios::in);
	
		if (is.good()) {
			is.close();
			return full_patch_path;
		} else {
			cerr << "[PatchLibrarian] Could not find patch file " << full_patch_path << endl;
		}
	}

	return "";
}


/** Save a patch from a PatchModel to a filename.
 *
 * The filename passed is the true filename the patch will be saved to (with no prefixing or anything
 * like that), and the patch_model's filename member will be set accordingly.
 *
 * This will break if:
 * - The filename does not have an extension (ie contain a ".")
 * - The patch_model has no (Om) path
 */
void
PatchLibrarian::save_patch(PatchModel* patch_model, const string& filename, bool recursive)
{
	assert(filename != "");
	assert(patch_model->path() != "");
	
	cout << "Saving patch " << patch_model->path() << " to " << filename << endl;

	patch_model->filename(filename);
	
	string dir = filename.substr(0, filename.find_last_of("/"));
	
	NodeModel* nm = NULL;
	PatchModel* spm = NULL; // subpatch model
	
	xmlDocPtr  xml_doc = NULL;
    xmlNodePtr xml_root_node = NULL;
	xmlNodePtr xml_node = NULL;
	xmlNodePtr xml_child_node = NULL;
	xmlNodePtr xml_grandchild_node = NULL;
	
    xml_doc = xmlNewDoc((xmlChar*)"1.0");
    xml_root_node = xmlNewNode(NULL, (xmlChar*)"patch");
    xmlDocSetRootElement(xml_doc, xml_root_node);

	const size_t temp_buf_length = 255;
	char temp_buf[temp_buf_length];
	
	string patch_name;
	if (patch_model->path() != "/") {
	  patch_name = patch_model->name();
	} else {
	  patch_name = filename;
	  if (patch_name.find("/") != string::npos)
	    patch_name = patch_name.substr(patch_name.find_last_of("/") + 1);
	  if (patch_name.find(".") != string::npos)
	    patch_name = patch_name.substr(0, patch_name.find_last_of("."));
	}

	assert(patch_name.length() > 0);
	xml_node = xmlNewChild(xml_root_node, NULL, (xmlChar*)"name",
			       (xmlChar*)patch_name.c_str());
	
	snprintf(temp_buf, temp_buf_length, "%zd", patch_model->poly());
	xml_node = xmlNewChild(xml_root_node, NULL, (xmlChar*)"polyphony", (xmlChar*)temp_buf);
	
	// Write metadata
	for (map<string, string>::const_iterator i = patch_model->metadata().begin();
			i != patch_model->metadata().end(); ++i) {
		// Dirty hack, don't save coordinates in patch file
		if ((*i).first != "module-x" && (*i).first != "module-y"
				&& (*i).first != "filename")
			xml_node = xmlNewChild(xml_root_node, NULL,
				(xmlChar*)(*i).first.c_str(), (xmlChar*)(*i).second.c_str());

		assert((*i).first != "node");
		assert((*i).first != "subpatch");
		assert((*i).first != "name");
		assert((*i).first != "polyphony");
		assert((*i).first != "preset");
	}
	
	// Save nodes and subpatches
	for (NodeModelMap::const_iterator i = patch_model->nodes().begin(); i != patch_model->nodes().end(); ++i) {
		nm = i->second.get();
		
		if (nm->plugin()->type() == PluginModel::Patch) {  // Subpatch
			spm = (PatchModel*)i->second.get();
			xml_node = xmlNewChild(xml_root_node, NULL, (xmlChar*)"subpatch", NULL);
			
			xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"name", (xmlChar*)spm->name().c_str());
			
			string ref_filename;
			// No path
			if (spm->filename() == "") {
				ref_filename = spm->name() + ".om";
				spm->filename(dir +"/"+ ref_filename);
			// Absolute path
			} else if (spm->filename().substr(0, 1) == "/") {
				// Attempt to make it a relative path, if it's undernath this patch's dir
				if (dir.substr(0, 1) == "/" && spm->filename().substr(0, dir.length()) == dir) {
					ref_filename = spm->filename().substr(dir.length()+1);
				} else { // FIXME: not good
					ref_filename = spm->filename().substr(spm->filename().find_last_of("/")+1);
					spm->filename(dir +"/"+ ref_filename);
				}
			} else {
				ref_filename = spm->filename();
			}
			
			xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"filename", (xmlChar*)ref_filename.c_str());
			
			snprintf(temp_buf, temp_buf_length, "%zd", spm->poly());
			xml_child_node = xmlNewChild(xml_node, NULL,  (xmlChar*)"polyphony", (xmlChar*)temp_buf);
			
			// Write metadata
			for (map<string, string>::const_iterator i = nm->metadata().begin();
					i != nm->metadata().end(); ++i) {	
				// Dirty hack, don't save metadata that would be in patch file
				if ((*i).first != "polyphony" && (*i).first != "filename"
					&& (*i).first != "author" && (*i).first != "description")
					xml_child_node = xmlNewChild(xml_node, NULL,
						(xmlChar*)(*i).first.c_str(), (xmlChar*)(*i).second.c_str());
			}
	
			if (recursive)
				save_patch(spm, spm->filename(), true);

		} else {  // Normal node
			xml_node = xmlNewChild(xml_root_node, NULL, (xmlChar*)"node", NULL);
			
			xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"name", (xmlChar*)nm->name().c_str());
			
			if (!nm->plugin()) break;
	
			xml_child_node = xmlNewChild(xml_node, NULL,  (xmlChar*)"polyphonic",
				(xmlChar*)((nm->polyphonic()) ? "true" : "false"));
				
			xml_child_node = xmlNewChild(xml_node, NULL,  (xmlChar*)"type",
				(xmlChar*)nm->plugin()->type_string());
			/*
			xml_child_node = xmlNewChild(xml_node, NULL,  (xmlChar*)"plugin-label",
				(xmlChar*)(nm->plugin()->plug_label().c_str()));
	
			if (nm->plugin()->type() != PluginModel::Internal) {
				xml_child_node = xmlNewChild(xml_node, NULL,  (xmlChar*)"library-name",
					(xmlChar*)(nm->plugin()->lib_name().c_str()));
			}*/
			xml_child_node = xmlNewChild(xml_node, NULL,  (xmlChar*)"plugin-uri",
				(xmlChar*)(nm->plugin()->uri().c_str()));
		
			// Write metadata
			for (map<string, string>::const_iterator i = nm->metadata().begin(); i != nm->metadata().end(); ++i) {
				// DSSI _hack_ (FIXME: fix OSC to be more like this and not smash DSSI into metadata?)
				if ((*i).first.substr(0, 16) == "dssi-configure--") {
					xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"dssi-configure", NULL);
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL,
						(xmlChar*)"key", (xmlChar*)(*i).first.substr(16).c_str());
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL,
							(xmlChar*)"value", (xmlChar*)(*i).second.c_str());
				} else if ((*i).first == "dssi-program") {
					xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"dssi-program", NULL);
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL,
						(xmlChar*)"bank", (xmlChar*)(*i).second.substr(0, (*i).second.find("/")).c_str());
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL,
						(xmlChar*)"program", (xmlChar*)(*i).second.substr((*i).second.find("/")+1).c_str());
				} else {
					xml_child_node = xmlNewChild(xml_node, NULL,
						(xmlChar*)(*i).first.c_str(), (xmlChar*)(*i).second.c_str());
				}
			}
	
			// Write port metadata, if necessary
			for (PortModelList::const_iterator i = nm->ports().begin(); i != nm->ports().end(); ++i) {
				const PortModel* const pm = (*i).get();
				if (pm->is_input() && pm->user_min() != pm->min_val() || pm->user_max() != pm->max_val()) {
					xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"port", NULL);
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL, (xmlChar*)"name",
						(xmlChar*)pm->path().name().c_str());
					snprintf(temp_buf, temp_buf_length, "%f", pm->user_min());
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL, (xmlChar*)"user-min", (xmlChar*)temp_buf);
					snprintf(temp_buf, temp_buf_length, "%f", pm->user_max());
					xml_grandchild_node = xmlNewChild(xml_child_node, NULL, (xmlChar*)"user-max", (xmlChar*)temp_buf);
				}	
			}
		}
	}

	// Save connections
	
	const list<CountedPtr<ConnectionModel> >& cl = patch_model->connections();
	const ConnectionModel* c = NULL;
	
	for (list<CountedPtr<ConnectionModel> >::const_iterator i = cl.begin(); i != cl.end(); ++i) {
		c = (*i).get();
		xml_node = xmlNewChild(xml_root_node, NULL, (xmlChar*)"connection", NULL);
		xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"source-node",
			(xmlChar*)c->src_port_path().parent().name().c_str());
		xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"source-port",
			(xmlChar*)c->src_port_path().name().c_str());
		xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"destination-node",
			(xmlChar*)c->dst_port_path().parent().name().c_str());
		xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"destination-port",
			(xmlChar*)c->dst_port_path().name().c_str());
	}
	
    // Save control values (ie presets eventually, right now just current control vals)
	
	xmlNodePtr xml_preset_node = xmlNewChild(xml_root_node, NULL, (xmlChar*)"preset", NULL);
	xml_node = xmlNewChild(xml_preset_node, NULL, (xmlChar*)"name", (xmlChar*)"default");

	PortModel* pm = NULL;

	// Save node port controls
	for (NodeModelMap::const_iterator n = patch_model->nodes().begin(); n != patch_model->nodes().end(); ++n) {
		nm = n->second.get();
		for (PortModelList::const_iterator p = nm->ports().begin(); p != nm->ports().end(); ++p) {
			pm = (*p).get();
			if (pm->is_input() && pm->is_control()) {
				float val = pm->value();
				xml_node = xmlNewChild(xml_preset_node, NULL, (xmlChar*)"control",  NULL);
				xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"node-name",
					(xmlChar*)nm->name().c_str());
				xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"port-name",
					(xmlChar*)pm->path().name().c_str());
				snprintf(temp_buf, temp_buf_length, "%f", val);
				xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"value",
					(xmlChar*)temp_buf);
			}
		}
	}
	
	// Save patch port controls
	for (PortModelList::const_iterator p = patch_model->ports().begin();
			p != patch_model->ports().end(); ++p) {
		pm = (*p).get();
		if (pm->is_input() && pm->is_control()) {
			float val = pm->value();
			xml_node = xmlNewChild(xml_preset_node, NULL, (xmlChar*)"control",  NULL);
			xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"port-name",
				(xmlChar*)pm->path().name().c_str());
			snprintf(temp_buf, temp_buf_length, "%f", val);
			xml_child_node = xmlNewChild(xml_node, NULL, (xmlChar*)"value",
				(xmlChar*)temp_buf);
		}
	}
	
	xmlSaveFormatFile(filename.c_str(), xml_doc, 1); // 1 == pretty print

    xmlFreeDoc(xml_doc);
    xmlCleanupParser();
}


/** Load a patch in to the engine (and client) from a patch file.
 *
 * The name and poly from the passed PatchModel are used.  If the name is
 * the empty string, the name will be loaded from the file.  If the poly
 * is 0, it will be loaded from file.  Otherwise the given values will
 * be used.
 *
 * If @a wait is set, the patch will be checked for existence before
 * loading everything in to it (to prevent messing up existing patches
 * that exist at the path this one should load as).
 *
 * If the @a existing parameter is true, the patch will be loaded into a
 * currently existing patch (ie a merging will take place).  Errors will
 * result if Nodes of conflicting names exist.
 *
 * Returns the path of the newly created patch.
 */
string
PatchLibrarian::load_patch(PatchModel* pm, bool wait, bool existing)
{
	string filename = pm->filename();

	string additional_path = (!pm->parent())
		? "" : ((PatchModel*)pm->parent().get())->filename();
	additional_path = additional_path.substr(0, additional_path.find_last_of("/"));

	filename = find_file(pm->filename(), additional_path);

	size_t poly = pm->poly();

	//cerr << "[PatchLibrarian] Loading patch " << filename << "" << endl;

	const size_t temp_buf_length = 255;
	char temp_buf[temp_buf_length];
	
	bool load_name = (pm->path() == "");
	bool load_poly = (poly == 0);
	
	xmlDocPtr doc = xmlParseFile(filename.c_str());

	if (doc == NULL ) {
		cerr << "Unable to parse patch file." << endl;
		return "";
	}

	xmlNodePtr cur = xmlDocGetRootElement(doc);

	if (cur == NULL) {
		cerr << "Empty document." << endl;
		xmlFreeDoc(doc);
		return "";
	}

	if (xmlStrcmp(cur->name, (const xmlChar*) "patch")) {
		cerr << "File is not an Om patch file, root node != patch" << endl;
		xmlFreeDoc(doc);
		return "";
	}

	xmlChar* key = NULL;
	cur = cur->xmlChildrenNode;
	string path;

	pm->filename(filename);

	// Load Patch attributes
	while (cur != NULL) {
		key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"name"))) {
			if (load_name) {
				assert(key != NULL);
				if (pm->parent()) {
					path = pm->parent()->base_path() + string((char*)key);
				} else {
					path = string("/") + string((char*)key);
				}
				assert(path.find("//") == string::npos);
				assert(path.length() > 0);
				pm->set_path(path);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"polyphony"))) {
			if (load_poly) {
				poly = atoi((char*)key);
				pm->poly(poly);
			}
		} else if (xmlStrcmp(cur->name, (const xmlChar*)"connection")
				&& xmlStrcmp(cur->name, (const xmlChar*)"node")
				&& xmlStrcmp(cur->name, (const xmlChar*)"subpatch")
				&& xmlStrcmp(cur->name, (const xmlChar*)"filename")
				&& xmlStrcmp(cur->name, (const xmlChar*)"preset")) {
			// Don't know what this tag is, add it as metadata without overwriting
			// (so caller can set arbitrary parameters which will be preserved)
			if (key != NULL)
				if (pm->get_metadata((const char*)cur->name) == "")
					pm->set_metadata((const char*)cur->name, (const char*)key);
		}
		
		xmlFree(key);
		key = NULL; // Avoid a (possible?) double free

		cur = cur->next;
	}
	
	if (poly == 0) poly = 1;

	if (!existing) {
		// Wait until the patch is created or the node creations may fail
		if (wait) {
			//int id = m_osc_model_engine_interface->get_next_request_id();
			//m_osc_model_engine_interface->set_wait_response_id(id);
			m_osc_model_engine_interface->create_patch_from_model(pm);
			//bool succeeded = m_osc_model_engine_interface->wait_for_response();
	
			// If creating the patch failed, bail out so we don't load all these nodes
			// into an already existing patch
			/*if (!succeeded) {
				cerr << "[PatchLibrarian] Patch load failed (patch already exists)" << endl;
				return "";
			}*/ // FIXME
		} else {
			m_osc_model_engine_interface->create_patch_from_model(pm);
		}
	}
	

	// Set the filename metadata.  (FIXME)
	// This isn't so good, considering multiple clients on multiple machines, and
	// absolute filesystem paths obviously aren't going to be correct.  But for now
	// this is all I can figure out to have Save/Save As work properly for subpatches
	m_osc_model_engine_interface->set_metadata(pm->path(), "filename", pm->filename());

	// Load nodes
	NodeModel* nm = NULL;
	cur = xmlDocGetRootElement(doc)->xmlChildrenNode;
	
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"node"))) {
			nm = parse_node(pm, doc, cur);
			if (nm != NULL) {
				m_osc_model_engine_interface->create_node_from_model(nm);
				m_osc_model_engine_interface->set_all_metadata(nm);
				for (PortModelList::const_iterator j = nm->ports().begin(); j != nm->ports().end(); ++j) {
					// FIXME: ew
					snprintf(temp_buf, temp_buf_length, "%f", (*j)->user_min());
					m_osc_model_engine_interface->set_metadata((*j)->path(), "user-min", temp_buf);
					snprintf(temp_buf, temp_buf_length, "%f", (*j)->user_max());
					m_osc_model_engine_interface->set_metadata((*j)->path(), "user-max", temp_buf);
				}
				nm = NULL;
				usleep(10000);
			}
		}
		cur = cur->next;
	}

	// Load subpatches
	cur = xmlDocGetRootElement(doc)->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"subpatch"))) {
			load_subpatch(pm, doc, cur);
		}
		cur = cur->next;
	}
	
	// Load connections
	ConnectionModel* cm = NULL;
	cur = xmlDocGetRootElement(doc)->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"connection"))) {
			cm = parse_connection(pm, doc, cur);
			if (cm != NULL) {
				m_osc_model_engine_interface->connect(cm->src_port_path(), cm->dst_port_path());
				usleep(1000);
			}
		}
		cur = cur->next;
	}
	
	
	// Load presets (control values)
	PresetModel* preset_model = NULL;
	cur = xmlDocGetRootElement(doc)->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"preset"))) {
			preset_model = parse_preset(pm, doc, cur);
			assert(preset_model != NULL);
			if (preset_model->name() == "default")
				m_osc_model_engine_interface->set_preset(pm->path(), preset_model);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	xmlCleanupParser();

	m_osc_model_engine_interface->set_all_metadata(pm);

	if (!existing)
		m_osc_model_engine_interface->enable_patch(pm->path());

	string ret = pm->path();
	return ret;
}


/** Build a NodeModel given a pointer to a Node in a patch file.
 */
NodeModel*
PatchLibrarian::parse_node(const PatchModel* parent, xmlDocPtr doc, const xmlNodePtr node)
{
	PluginModel* plugin = new PluginModel();
	NodeModel* nm = new NodeModel(plugin, "/UNINITIALIZED"); // FIXME: ew

	xmlChar* key;
	xmlNodePtr cur = node->xmlChildrenNode;
	
	bool found_name = false;
	
	while (cur != NULL) {
		key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"name"))) {
			nm->set_path(parent->base_path() + (char*)key);
			found_name = true;
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"polyphonic"))) {
			nm->polyphonic(!strcmp((char*)key, "true"));
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"type"))) {
			plugin->set_type((const char*)key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"library-name"))) {
			plugin->lib_name((char*)key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"plugin-label"))) {
			plugin->plug_label((char*)key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"plugin-uri"))) {
			plugin->uri((char*)key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"port"))) {
			xmlNodePtr child = cur->xmlChildrenNode;
			
			string path;
			float user_min = 0.0;
			float user_max = 0.0;
			
			while (child != NULL) {
				key = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
				
				if ((!xmlStrcmp(child->name, (const xmlChar*)"name"))) {
					path = nm->base_path() + (char*)key;
				} else if ((!xmlStrcmp(child->name, (const xmlChar*)"user-min"))) {
					user_min = atof((char*)key);
				} else if ((!xmlStrcmp(child->name, (const xmlChar*)"user-max"))) {
					user_max = atof((char*)key);
				}
				
				xmlFree(key);
				key = NULL; // Avoid a (possible?) double free
		
				child = child->next;
			}

			// FIXME: nasty assumptions
			PortModel* pm = new PortModel(path,
					PortModel::CONTROL, PortModel::INPUT, PortModel::NONE,
					0.0, user_min, user_max);
			nm->add_port(pm);

		// DSSI hacks.  Stored in the patch files as special elements, but sent to
		// the engine as normal metadata with specially formatted key/values.  Not
		// sure if this is the best way to go about this, but it's the least damaging
		// right now
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"dssi-program"))) {
			xmlNodePtr child = cur->xmlChildrenNode;
			
			string bank;
			string program;
			
			while (child != NULL) {
				key = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
				
				if ((!xmlStrcmp(child->name, (const xmlChar*)"bank"))) {
					bank = (char*)key;
				} else if ((!xmlStrcmp(child->name, (const xmlChar*)"program"))) {
					program = (char*)key;
				}
				
				xmlFree(key);
				key = NULL; // Avoid a (possible?) double free
				child = child->next;
			}
			nm->set_metadata("dssi-program", bank +"/"+ program);
			
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"dssi-configure"))) {
			xmlNodePtr child = cur->xmlChildrenNode;
			
			string dssi_key;
			string dssi_value;
			
			while (child != NULL) {
				key = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
				
				if ((!xmlStrcmp(child->name, (const xmlChar*)"key"))) {
					dssi_key = (char*)key;
				} else if ((!xmlStrcmp(child->name, (const xmlChar*)"value"))) {
					dssi_value = (char*)key;
				}
				
				xmlFree(key);
				key = NULL; // Avoid a (possible?) double free
		
				child = child->next;
			}
			nm->set_metadata(string("dssi-configure--").append(dssi_key), dssi_value);
			
		} else {  // Don't know what this tag is, add it as metadata
			if (key != NULL)
				nm->set_metadata((const char*)cur->name, (const char*)key);
		}
		xmlFree(key);
		key = NULL;

		cur = cur->next;
	}
	
	if (nm->path() == "") {
		cerr << "[PatchLibrarian] Malformed patch file (node tag has empty children)" << endl;
		cerr << "[PatchLibrarian] Node ignored." << endl;
		delete nm;
		return NULL;
	} else {
		//nm->plugin(plugin);
		return nm;
	}
}


void
PatchLibrarian::load_subpatch(PatchModel* parent, xmlDocPtr doc, const xmlNodePtr subpatch)
{
	xmlChar *key;
	xmlNodePtr cur = subpatch->xmlChildrenNode;
	
	PatchModel* pm = new PatchModel("/UNINITIALIZED", 1); // FIXME: ew
	
	while (cur != NULL) {
		key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"name"))) {
			if (parent == NULL)
				pm->set_path(string("/") + (const char*)key);
			else
				pm->set_path(parent->base_path() + (const char*)key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"polyphony"))) {
			pm->poly(atoi((const char*)key));
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"filename"))) {
			pm->filename((const char*)key);
		} else {  // Don't know what this tag is, add it as metadata
			if (key != NULL && strlen((const char*)key) > 0)
				pm->set_metadata((const char*)cur->name, (const char*)key);
		}
		xmlFree(key);
		key = NULL;

		cur = cur->next;
	}

	// This needs to be done after setting the path above, to prevent
	// NodeModel::set_path from calling it's parent's rename_node with
	// an invalid (nonexistant) name
	pm->set_parent(parent);
	
	load_patch(pm, false);
}


/** Build a ConnectionModel given a pointer to a connection in a patch file.
 */
ConnectionModel*
PatchLibrarian::parse_connection(const PatchModel* parent, xmlDocPtr doc, const xmlNodePtr node)
{
	//cerr << "[PatchLibrarian] Parsing connection..." << endl;

	xmlChar *key;
	xmlNodePtr cur = node->xmlChildrenNode;
	
	string source_node, source_port, dest_node, dest_port;
	
	while (cur != NULL) {
		key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"source-node"))) {
			source_node = (char*)key;
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"source-port"))) {
			source_port = (char*)key;
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"destination-node"))) {
			dest_node = (char*)key;
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"destination-port"))) {
			dest_port = (char*)key;
		}
		
		xmlFree(key);
		key = NULL; // Avoid a (possible?) double free

		cur = cur->next;
	}

	if (source_node == "" || source_port == "" || dest_node == "" || dest_port == "") {
		cerr << "[PatchLibrarian] Malformed patch file (connection tag has empty children)" << endl;
		cerr << "[PatchLibrarian] Connection ignored." << endl;
		return NULL;
	}

	// FIXME: temporary compatibility, remove any slashes from port names
	// remove this soon once patches have migrated
	string::size_type slash_index;
	while ((slash_index = source_port.find("/")) != string::npos)
		source_port[slash_index] = '-';

	while ((slash_index = dest_port.find("/")) != string::npos)
		dest_port[slash_index] = '-';
	
	ConnectionModel* cm = new ConnectionModel(parent->base_path() + source_node +"/"+ source_port,
		parent->base_path() + dest_node +"/"+ dest_port);
	
	return cm;
}


/** Build a PresetModel given a pointer to a preset in a patch file.
 */
PresetModel*
PatchLibrarian::parse_preset(const PatchModel* patch, xmlDocPtr doc, const xmlNodePtr node)
{
	xmlNodePtr cur = node->xmlChildrenNode;
	xmlChar* key;

	PresetModel* pm = new PresetModel(patch->base_path());
	
	while (cur != NULL) {
		key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

		if ((!xmlStrcmp(cur->name, (const xmlChar*)"name"))) {
			assert(key != NULL);
			pm->name((char*)key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar*)"control"))) {
			xmlNodePtr child = cur->xmlChildrenNode;
	
			string node_name = "", port_name = "";
			float val = 0.0;
			
			while (child != NULL) {
				key = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
				
				if ((!xmlStrcmp(child->name, (const xmlChar*)"node-name"))) {
					node_name = (char*)key;
				} else if ((!xmlStrcmp(child->name, (const xmlChar*)"port-name"))) {
					port_name = (char*)key;
				} else if ((!xmlStrcmp(child->name, (const xmlChar*)"value"))) {
					val = atof((char*)key);
				}
				
				xmlFree(key);
				key = NULL; // Avoid a (possible?) double free
		
				child = child->next;
			}
			
			if (port_name == "") {
				string msg = "Unable to parse control in patch file ( node = ";
				msg.append(node_name).append(", port = ").append(port_name).append(")");
				cerr << "ERROR: " << msg << endl;
				//m_client_hooks->error(msg);
			} else {
				// FIXME: temporary compatibility, remove any slashes from port name
				// remove this soon once patches have migrated
				string::size_type slash_index;
				while ((slash_index = port_name.find("/")) != string::npos)
					port_name[slash_index] = '-';
				pm->add_control(node_name, port_name, val);
			}
		}
		xmlFree(key);
		key = NULL;
		cur = cur->next;
	}
	if (pm->name() == "") {
		cerr << "Preset in patch file has no name." << endl;
		//m_client_hooks->error("Preset in patch file has no name.");
		pm->name("Unnamed");
	}

	return pm;
}

} // namespace LibOmClient
