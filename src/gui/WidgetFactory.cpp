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

#include <fstream>

#include "raul/log.hpp"

#include "ingen/shared/runtime_paths.hpp"

#include "WidgetFactory.hpp"

using namespace std;
using namespace Raul;

namespace Ingen {
namespace GUI {

Glib::ustring WidgetFactory::ui_filename = "";

inline static bool
is_readable(const std::string& filename)
{
	std::ifstream fs(filename.c_str());
	const bool fail = fs.fail();
	fs.close();
	return !fail;
}

void
WidgetFactory::find_ui_file()
{
	// Try file in bundle (directory where executable resides)
	ui_filename = Shared::bundle_file_path("ingen_gui.ui");
	if (is_readable(ui_filename))
		return;

	// Try ENGINE_UI_PATH from the environment
	const char* const env_path = getenv("INGEN_UI_PATH");
	if (env_path && is_readable(env_path)) {
		ui_filename = env_path;
		return;
	}

	// Try the default system installed path
	ui_filename = Shared::data_file_path("ingen_gui.ui");
	if (is_readable(ui_filename))
		return;

	error << "[WidgetFactory] Unable to find ingen_gui.ui in "
	      << INGEN_DATA_DIR << endl;
	throw std::runtime_error("Unable to find UI file");
}

Glib::RefPtr<Gtk::Builder>
WidgetFactory::create(const string& toplevel_widget)
{
	if (ui_filename.empty())
		find_ui_file();

	try {
		if (toplevel_widget.empty())
			return Gtk::Builder::create_from_file(ui_filename);
		else
			return Gtk::Builder::create_from_file(ui_filename, toplevel_widget.c_str());
	} catch (const Gtk::BuilderError& ex) {
		error << "[WidgetFactory] " << ex.what() << endl;
		throw ex;
	}
}

} // namespace GUI
} // namespace Ingen
