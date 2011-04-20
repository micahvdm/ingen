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
#include "raul/Path.hpp"
#include "ingen/ServerInterface.hpp"
#include "shared/LV2URIMap.hpp"
#include "client/ClientStore.hpp"
#include "client/PatchModel.hpp"
#include "App.hpp"
#include "PatchTreeWindow.hpp"
#include "SubpatchModule.hpp"
#include "WindowFactory.hpp"

#define LOG(s) s << "[PatchTreeWindow] "

using namespace std;
using namespace Raul;

namespace Ingen {
namespace GUI {

PatchTreeWindow::PatchTreeWindow(BaseObjectType*                        cobject,
                                 const Glib::RefPtr<Gnome::Glade::Xml>& xml)
	: Window(cobject)
	, _enable_signal(true)
{
	xml->get_widget_derived("patches_treeview", _patches_treeview);

	_patch_treestore = Gtk::TreeStore::create(_patch_tree_columns);
	_patches_treeview->set_window(this);
	_patches_treeview->set_model(_patch_treestore);
	Gtk::TreeViewColumn* name_col = Gtk::manage(new Gtk::TreeViewColumn(
		"Patch", _patch_tree_columns.name_col));
	Gtk::TreeViewColumn* enabled_col = Gtk::manage(new Gtk::TreeViewColumn(
		"Run", _patch_tree_columns.enabled_col));
	name_col->set_resizable(true);
	name_col->set_expand(true);

	_patches_treeview->append_column(*name_col);
	_patches_treeview->append_column(*enabled_col);
	Gtk::CellRendererToggle* enabled_renderer = dynamic_cast<Gtk::CellRendererToggle*>(
		_patches_treeview->get_column_cell_renderer(1));
	enabled_renderer->property_activatable() = true;

	_patch_tree_selection = _patches_treeview->get_selection();

	_patches_treeview->signal_row_activated().connect(
		sigc::mem_fun(this, &PatchTreeWindow::event_patch_activated));
	enabled_renderer->signal_toggled().connect(
		sigc::mem_fun(this, &PatchTreeWindow::event_patch_enabled_toggled));

	_patches_treeview->columns_autosize();
}

void
PatchTreeWindow::init(ClientStore& store)
{
	store.signal_new_object.connect(sigc::mem_fun(this, &PatchTreeWindow::new_object));
}

void
PatchTreeWindow::new_object(SharedPtr<ObjectModel> object)
{
	SharedPtr<PatchModel> patch = PtrCast<PatchModel>(object);
	if (patch)
		add_patch(patch);
}

void
PatchTreeWindow::add_patch(SharedPtr<PatchModel> pm)
{
	if (!pm->parent()) {
		Gtk::TreeModel::iterator iter = _patch_treestore->append();
		Gtk::TreeModel::Row row = *iter;
		if (pm->path().is_root()) {
			row[_patch_tree_columns.name_col] = App::instance().engine()->uri().str();
		} else {
			row[_patch_tree_columns.name_col] = pm->symbol().c_str();
		}
		row[_patch_tree_columns.enabled_col] = pm->enabled();
		row[_patch_tree_columns.patch_model_col] = pm;
		_patches_treeview->expand_row(_patch_treestore->get_path(iter), true);
	} else {
		Gtk::TreeModel::Children children = _patch_treestore->children();
		Gtk::TreeModel::iterator c = find_patch(children, pm->parent());

		if (c != children.end()) {
			Gtk::TreeModel::iterator iter = _patch_treestore->append(c->children());
			Gtk::TreeModel::Row row = *iter;
			row[_patch_tree_columns.name_col] = pm->symbol().c_str();
			row[_patch_tree_columns.enabled_col] = pm->enabled();
			row[_patch_tree_columns.patch_model_col] = pm;
			_patches_treeview->expand_row(_patch_treestore->get_path(iter), true);
		}
	}

	pm->signal_property.connect(sigc::bind(
			sigc::mem_fun(this, &PatchTreeWindow::patch_property_changed),
			pm));

	pm->signal_moved.connect(sigc::bind(
		sigc::mem_fun(this, &PatchTreeWindow::patch_moved),
		pm));

	pm->signal_destroyed.connect(sigc::bind(
		sigc::mem_fun(this, &PatchTreeWindow::remove_patch),
		pm));
}

void
PatchTreeWindow::remove_patch(SharedPtr<PatchModel> pm)
{
	Gtk::TreeModel::iterator i = find_patch(_patch_treestore->children(), pm);
	if (i != _patch_treestore->children().end())
		_patch_treestore->erase(i);
}

Gtk::TreeModel::iterator
PatchTreeWindow::find_patch(
		Gtk::TreeModel::Children       root,
		SharedPtr<Client::ObjectModel> patch)
{
	for (Gtk::TreeModel::iterator c = root.begin(); c != root.end(); ++c) {
		SharedPtr<PatchModel> pm = (*c)[_patch_tree_columns.patch_model_col];
		if (patch == pm) {
			return c;
		} else if ((*c)->children().size() > 0) {
			Gtk::TreeModel::iterator ret = find_patch(c->children(), patch);
			if (ret != c->children().end())
				return ret;
		}
	}
	return root.end();
}

/** Show the context menu for the selected patch in the patches treeview.
 */
void
PatchTreeWindow::show_patch_menu(GdkEventButton* ev)
{
	Gtk::TreeModel::iterator active = _patch_tree_selection->get_selected();
	if (active) {
		Gtk::TreeModel::Row row = *active;
		SharedPtr<PatchModel> pm = row[_patch_tree_columns.patch_model_col];
		if (pm)
			warn << "TODO: patch menu from tree window" << endl;
	}
}

void
PatchTreeWindow::event_patch_activated(const Gtk::TreeModel::Path& path, Gtk::TreeView::Column* col)
{
	Gtk::TreeModel::iterator active = _patch_treestore->get_iter(path);
	Gtk::TreeModel::Row row = *active;
	SharedPtr<PatchModel> pm = row[_patch_tree_columns.patch_model_col];

	App::instance().window_factory()->present_patch(pm);
}

void
PatchTreeWindow::event_patch_enabled_toggled(const Glib::ustring& path_str)
{
	Gtk::TreeModel::Path path(path_str);
	Gtk::TreeModel::iterator active = _patch_treestore->get_iter(path);
	Gtk::TreeModel::Row row = *active;

	SharedPtr<PatchModel> pm = row[_patch_tree_columns.patch_model_col];
	assert(pm);

	if (_enable_signal)
		App::instance().engine()->set_property(pm->path(),
				App::instance().uris().ingen_enabled, (bool)!pm->enabled());
}

void
PatchTreeWindow::patch_property_changed(const URI& key, const Atom& value,
		SharedPtr<PatchModel> patch)
{
	const LV2URIMap& uris = App::instance().uris();
	_enable_signal = false;
	if (key == uris.ingen_enabled && value.type() == Atom::BOOL) {
		Gtk::TreeModel::iterator i = find_patch(_patch_treestore->children(), patch);
		if (i != _patch_treestore->children().end()) {
			Gtk::TreeModel::Row row = *i;
			row[_patch_tree_columns.enabled_col] = value.get_bool();
		} else {
			LOG(error) << "Unable to find patch " << patch->path() << endl;
		}
	}
	_enable_signal = true;
}

void
PatchTreeWindow::patch_moved(SharedPtr<PatchModel> patch)
{
	_enable_signal = false;

	Gtk::TreeModel::iterator i
		= find_patch(_patch_treestore->children(), patch);

	if (i != _patch_treestore->children().end()) {
		Gtk::TreeModel::Row row = *i;
		row[_patch_tree_columns.name_col] = patch->symbol().c_str();
	} else {
		LOG(error) << "Unable to find patch " << patch->path() << endl;
	}

	_enable_signal = true;
}

} // namespace GUI
} // namespace Ingen
