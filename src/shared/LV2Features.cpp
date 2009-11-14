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

#include <cstdlib>
#include <cstring>
#include "LV2Features.hpp"
#include "LV2URIMap.hpp"

using namespace std;

namespace Ingen {
namespace Shared {


LV2Features::LV2Features()
//	: _lv2_features((LV2_Feature**)malloc(sizeof(LV2_Feature*)))
{
//	_lv2_features[0] = NULL;

	add_feature(LV2_URI_MAP_URI, SharedPtr<Feature>(new LV2URIMap()));
}


SharedPtr<LV2Features::Feature>
LV2Features::feature(const std::string& uri)
{
	Features::const_iterator i = _features.find(uri);
	if (i != _features.end())
		return i->second;
	else
		return SharedPtr<Feature>();
}


void
LV2Features::add_feature(const std::string& uri, SharedPtr<Feature> feature)
{
	_features.insert(make_pair(uri, feature));
}


SharedPtr<LV2Features::FeatureArray>
LV2Features::lv2_features(Node* node) const
{
	FeatureArray::FeatureVector vec;
	for (Features::const_iterator f = _features.begin(); f != _features.end(); ++f)
		vec.push_back(f->second->feature(node));
	return SharedPtr<FeatureArray>(new FeatureArray(vec));
}


} // namespace Shared
} // namespace Ingen
