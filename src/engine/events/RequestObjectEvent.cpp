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

#include "RequestObjectEvent.hpp"
#include "interface/ClientInterface.hpp"
#include "Responder.hpp"
#include "Engine.hpp"
#include "EngineStore.hpp"
#include "ClientBroadcaster.hpp"
#include "PatchImpl.hpp"
#include "NodeImpl.hpp"
#include "PortImpl.hpp"
#include "ObjectSender.hpp"
#include "ProcessContext.hpp"

using namespace Raul;

namespace Ingen {


RequestObjectEvent::RequestObjectEvent(Engine& engine, SharedPtr<Responder> responder, SampleCount timestamp, const Path& path)
	: QueuedEvent(engine, responder, timestamp)
	, _path(path)
	, _object(NULL)
{
}


void
RequestObjectEvent::pre_process()
{
	_object = _engine.engine_store()->find_object(_path);

	QueuedEvent::pre_process();
}


void
RequestObjectEvent::execute(ProcessContext& context)
{
	QueuedEvent::execute(context);
	assert(_time >= context.start() && _time <= context.end());
}


void
RequestObjectEvent::post_process()
{
	if (!_object) {
		_responder->respond_error("Unable to find object requested.");
	} else if (_responder->client()) {	
		ObjectSender::send_object(_responder->client(), _object, true);
	} else {
		_responder->respond_error("Unable to find client to send object.");
	}
}


} // namespace Ingen

