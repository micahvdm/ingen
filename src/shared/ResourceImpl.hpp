/* This file is part of Ingen.
 * Copyright (C) 2008-2009 Dave Robillard <http://drobilla.net>
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

#ifndef RESOURCEIMPL_HPP
#define RESOURCEIMPL_HPP

#include <map>
#include <sigc++/sigc++.h>
#include "raul/URI.hpp"
#include "interface/Resource.hpp"
#include "interface/DataType.hpp"

namespace Ingen {
namespace Shared {

class ResourceImpl : virtual public Resource
{
public:
	ResourceImpl(const Raul::URI& uri) : _uri(uri) {}

	virtual const Raul::URI uri()  const { return _uri.str(); }

	const Properties& properties() const { return _properties; }
	Properties&       properties()       { return _properties; }

	const Raul::Atom& get_property(const Raul::URI& uri) const;
	void              set_property(const Raul::URI& uri, const Raul::Atom& value);
	void              add_property(const Raul::URI& uri, const Raul::Atom& value);
	void              set_properties(const Properties& p);
	void              add_properties(const Properties& p);

	sigc::signal<void, const Raul::URI&, const Raul::Atom&> signal_property;

	/** Get the ingen type from a set of Properties.
	 * If some coherent ingen type is found, true is returned and the appropriate
	 * output parameter set to true.  Otherwise false is returned.
	 */
	static bool type(
			const Properties& properties,
			bool& patch,
			bool& node,
			bool& port, bool& is_output, DataType& data_type);

	static const Raul::URI meta_uri(const Raul::URI& base, const Raul::URI& uri);

private:
	Raul::URI  _uri;
	Properties _properties;
};


} // namespace Shared
} // namespace Ingen

#endif // RESOURCEIMPL_HPP

