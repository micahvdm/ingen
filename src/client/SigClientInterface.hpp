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
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SIGCLIENTINTERFACE_H
#define SIGCLIENTINTERFACE_H

#include <inttypes.h>
#include <sigc++/sigc++.h>
#include "raul/Path.hpp"
#include "interface/ClientInterface.hpp"

namespace Ingen {
namespace Client {


/** A LibSigC++ signal emitting interface for clients to use.
 *
 * This simply emits an sigc signal for every event (eg OSC message) coming from
 * the engine.  Use Store (which extends this) if you want a nice client-side
 * model of the engine.
 *
 * The signals here match the calls to ClientInterface exactly.  See the
 * documentation for ClientInterface for meanings of signal parameters.
 */
class SigClientInterface : public Ingen::Shared::ClientInterface, public sigc::trackable
{
public:
	SigClientInterface() : _enabled(true) {}
	
	bool enabled() const { return _enabled; }

	Raul::URI uri() const { return "ingen:internal"; }

	// Signal parameters match up directly with ClientInterface calls
	sigc::signal<bool, const Shared::GraphObject*> signal_new_object;
	
	sigc::signal<void, int32_t>                                     signal_response_ok;
	sigc::signal<void, int32_t, std::string>                        signal_response_error;
	sigc::signal<void>                                              signal_bundle_begin; 
	sigc::signal<void>                                              signal_bundle_end; 
	sigc::signal<void, std::string>                                 signal_error; 
	sigc::signal<void, Raul::URI, Raul::URI, Raul::Symbol>          signal_new_plugin; 
	sigc::signal<void, Raul::Path, uint32_t>                        signal_new_patch; 
	sigc::signal<void, Raul::Path, Raul::URI>                       signal_new_node; 
	sigc::signal<void, Raul::Path, Raul::URI, uint32_t, bool>       signal_new_port; 
	sigc::signal<void, Raul::Path>                                  signal_clear_patch; 
	sigc::signal<void, Raul::Path, Raul::Path>                      signal_object_renamed; 
	sigc::signal<void, Raul::Path>                                  signal_object_destroyed; 
	sigc::signal<void, Raul::Path, Raul::Path>                      signal_connection; 
	sigc::signal<void, Raul::Path, Raul::Path>                      signal_disconnection; 
	sigc::signal<void, Raul::URI, Raul::URI, Raul::Atom>            signal_variable_change; 
	sigc::signal<void, Raul::URI, Raul::URI, Raul::Atom>            signal_property_change; 
	sigc::signal<void, Raul::Path, Raul::Atom>                      signal_port_value; 
	sigc::signal<void, Raul::Path, uint32_t, Raul::Atom>            signal_voice_value; 
	sigc::signal<void, Raul::Path>                                  signal_activity; 
	sigc::signal<void, Raul::Path, uint32_t, uint32_t, std::string> signal_program_add; 
	sigc::signal<void, Raul::Path, uint32_t, uint32_t>              signal_program_remove; 
	
	/** Fire pending signals.  Only does anything on derived classes (that may queue) */
	virtual bool emit_signals() { return false; }

protected:
	
	bool _enabled;

	// ClientInterface hooks that fire the above signals
	
	void enable()  { _enabled = true; }
	void disable() { _enabled = false ; }

	void bundle_begin()
		{ if (_enabled) signal_bundle_begin.emit(); }
	
	void bundle_end()
		{ if (_enabled) signal_bundle_end.emit(); }

	void transfer_begin() {}
	void transfer_end()   {}

	void response_ok(int32_t id)
		{ if (_enabled) signal_response_ok.emit(id); }

	void response_error(int32_t id, const std::string& msg)
		{ if (_enabled) signal_response_error.emit(id, msg); }
	
	void error(const std::string& msg)
		{ if (_enabled) signal_error.emit(msg); }
	
	void new_plugin(const Raul::URI& uri, const Raul::URI& type_uri, const Raul::Symbol& symbol)
		{ if (_enabled) signal_new_plugin.emit(uri, type_uri, symbol); }
	
	bool new_object(const Shared::GraphObject* object)
		{ if (_enabled) signal_new_object.emit(object); return false; }

	void new_patch(const Raul::Path& path, uint32_t poly)
		{ if (_enabled) signal_new_patch.emit(path, poly); }
	
	void new_node(const Raul::Path& path, const Raul::URI& plugin_uri)
		{ if (_enabled) signal_new_node.emit(path, plugin_uri); }
	
	void new_port(const Raul::Path& path, const Raul::URI& type, uint32_t index, bool is_output)
		{ if (_enabled) signal_new_port.emit(path, type, index, is_output); }
	
	void connect(const Raul::Path& src_port_path, const Raul::Path& dst_port_path)
		{ if (_enabled) signal_connection.emit(src_port_path, dst_port_path); }

	void destroy(const Raul::Path& path)
		{ if (_enabled) signal_object_destroyed.emit(path); }
	
	void clear_patch(const Raul::Path& path)
		{ if (_enabled) signal_clear_patch.emit(path); }

	void rename(const Raul::Path& old_path, const Raul::Path& new_path)
		{ if (_enabled) signal_object_renamed.emit(old_path, new_path); }
	
	void disconnect(const Raul::Path& src_port_path, const Raul::Path& dst_port_path)
		{ if (_enabled) signal_disconnection.emit(src_port_path, dst_port_path); }
	
	void set_variable(const Raul::URI& path, const Raul::URI& key, const Raul::Atom& value)
		{ if (_enabled) signal_variable_change.emit(path, key, value); }
	
	void set_property(const Raul::URI& path, const Raul::URI& key, const Raul::Atom& value)
		{ if (_enabled) signal_property_change.emit(path, key, value); }

	void set_port_value(const Raul::Path& port_path, const Raul::Atom& value)
		{ if (_enabled) signal_port_value.emit(port_path, value); }
	
	void set_voice_value(const Raul::Path& port_path, uint32_t voice, const Raul::Atom& value)
		{ if (_enabled) signal_voice_value.emit(port_path, voice, value); }
	
	void activity(const Raul::Path& port_path)
		{ if (_enabled) signal_activity.emit(port_path); }

	void program_add(const Raul::Path& path, uint32_t bank, uint32_t program, const std::string& name)
		{ if (_enabled) signal_program_add.emit(path, bank, program, name); }
	
	void program_remove(const Raul::Path& path, uint32_t bank, uint32_t program)
		{ if (_enabled) signal_program_remove.emit(path, bank, program); }
};


} // namespace Client
} // namespace Ingen

#endif
