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

#ifndef INGEN_EVENTS_CREATEPORT_HPP
#define INGEN_EVENTS_CREATEPORT_HPP

#include "raul/Array.hpp"
#include "raul/Path.hpp"

#include "ingen/Resource.hpp"

#include "PortType.hpp"
#include "Event.hpp"

namespace Ingen {
namespace Server {

class PatchImpl;
class PortImpl;
class DriverPort;

namespace Events {

/** An event to add a Port to a Patch.
 *
 * \ingroup engine
 */
class CreatePort : public Event
{
public:
	CreatePort(Engine&                     engine,
	           ClientInterface*            client,
	           int32_t                     id,
	           SampleCount                 timestamp,
	           const Raul::Path&           path,
	           bool                        is_output,
	           const Resource::Properties& properties);

	void pre_process();
	void execute(ProcessContext& context);
	void post_process();

private:
	enum ErrorType {
		NO_ERROR,
		UNKNOWN_TYPE,
		BAD_INDEX,
		CREATION_FAILED
	};

	Raul::Path              _path;
	Raul::URI               _type;
	PortType                _data_type;
	PatchImpl*              _patch;
	PortImpl*               _patch_port;
	Raul::Array<PortImpl*>* _ports_array; ///< New (external) ports array for Patch
	DriverPort*             _driver_port; ///< Driver (eg Jack) port if this is a toplevel port
	Resource::Properties    _properties;
	bool                    _is_output;
};

} // namespace Server
} // namespace Ingen
} // namespace Events

#endif // INGEN_EVENTS_CREATEPORT_HPP
