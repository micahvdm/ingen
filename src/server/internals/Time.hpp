/*
  This file is part of Ingen.
  Copyright 2007-2015 David Robillard <http://drobilla.net/>

  Ingen is free software: you can redistribute it and/or modify it under the
  terms of the GNU Affero General Public License as published by the Free
  Software Foundation, either version 3 of the License, or any later version.

  Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU Affero General Public License for details.

  You should have received a copy of the GNU Affero General Public License
  along with Ingen.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef INGEN_INTERNALS_TIME_HPP
#define INGEN_INTERNALS_TIME_HPP

#include "InternalBlock.hpp"
#include "types.hpp"

namespace raul {
class Symbol;
} // namespace raul

namespace ingen {

class URIs;

namespace server {

class BufferFactory;
class GraphImpl;
class InternalPlugin;
class OutputPort;

namespace internals {

/** Time information block.
 *
 * This sends messages whenever the transport speed or tempo changes.
 *
 * \ingroup engine
 */
class TimeNode : public InternalBlock
{
public:
	TimeNode(InternalPlugin*     plugin,
	         BufferFactory&      bufs,
	         const raul::Symbol& symbol,
	         bool                polyphonic,
	         GraphImpl*          parent,
	         SampleRate          srate);

	void run(RunContext& ctx) override;

	static InternalPlugin* internal_plugin(URIs& uris);

private:
	OutputPort* _notify_port;
};

} // namespace internals
} // namespace server
} // namespace ingen

#endif // INGEN_INTERNALS_TIME_HPP
