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

#include <gtkmm.h>
#include "ingen/ServerInterface.hpp"
#include "shared/LV2URIMap.hpp"
#include "ingen/client/NodeModel.hpp"
#include "ingen/client/PluginModel.hpp"
#include "App.hpp"
#include "NodeMenu.hpp"
#include "WindowFactory.hpp"
#include "WidgetFactory.hpp"

using namespace std;
using namespace Ingen::Client;

namespace Ingen {
namespace GUI {

NodeMenu::NodeMenu(BaseObjectType*                   cobject,
                   const Glib::RefPtr<Gtk::Builder>& xml)
	: ObjectMenu(cobject, xml)
	, _controls_menuitem(NULL)
	, _presets_menu(NULL)
{
	xml->get_widget("node_controls_menuitem", _controls_menuitem);
	xml->get_widget("node_popup_gui_menuitem", _popup_gui_menuitem);
	xml->get_widget("node_embed_gui_menuitem", _embed_gui_menuitem);
	xml->get_widget("node_randomize_menuitem", _randomize_menuitem);
}

void
NodeMenu::init(SharedPtr<const NodeModel> node)
{
	ObjectMenu::init(node);

	_learn_menuitem->signal_activate().connect(sigc::mem_fun(this,
			&NodeMenu::on_menu_learn));
	_controls_menuitem->signal_activate().connect(
		sigc::bind(sigc::mem_fun(App::instance().window_factory(),
		                         &WindowFactory::present_controls),
		           node));
	_popup_gui_menuitem->signal_activate().connect(
		sigc::mem_fun(signal_popup_gui, &sigc::signal<void>::emit));
	_embed_gui_menuitem->signal_toggled().connect(
		sigc::mem_fun(this, &NodeMenu::on_menu_embed_gui));
	_randomize_menuitem->signal_activate().connect(
		sigc::mem_fun(this, &NodeMenu::on_menu_randomize));

	const PluginModel* plugin = dynamic_cast<const PluginModel*>(node->plugin());
	if (plugin && plugin->type() == PluginModel::LV2 && plugin->has_ui()) {
		_popup_gui_menuitem->show();
		_embed_gui_menuitem->show();
	} else {
		_popup_gui_menuitem->hide();
		_embed_gui_menuitem->hide();
	}

	if (plugin && plugin->type() == PluginModel::LV2) {
		LilvNode* preset_pred = lilv_new_uri(
			plugin->lilv_world(),
			"http://lv2plug.in/ns/dev/presets#hasPreset");
		LilvNode* title_pred = lilv_new_uri(
			plugin->lilv_world(),
			"http://dublincore.org/documents/dcmi-namespace/title");
		LilvNodes* presets = lilv_plugin_get_value(
			plugin->lilv_plugin(), preset_pred);
		if (presets) {
			_presets_menu = Gtk::manage(new Gtk::Menu());

			LILV_FOREACH(nodes, i, presets) {
				const LilvNode* uri = lilv_nodes_get(presets, i);
				LilvNodes* titles   = lilv_world_find_nodes(
					plugin->lilv_world(), uri, title_pred, NULL);
				if (titles) {
					const LilvNode* title = lilv_nodes_get_first(titles);
					_presets_menu->items().push_back(
						Gtk::Menu_Helpers::MenuElem(
							lilv_node_as_string(title),
							sigc::bind(
								sigc::mem_fun(this, &NodeMenu::on_preset_activated),
								string(lilv_node_as_string(uri)))));

					// I have no idea why this is necessary, signal_activated doesn't work
					// in this menu (and only this menu)
					Gtk::MenuItem* item = &(_presets_menu->items().back());
					item->signal_button_release_event().connect(
						sigc::bind<0>(sigc::mem_fun(this, &NodeMenu::on_preset_clicked),
						              string(lilv_node_as_string(uri))));
				}
			}
			items().push_front(Gtk::Menu_Helpers::ImageMenuElem("_Presets",
					*(manage(new Gtk::Image(Gtk::Stock::INDEX, Gtk::ICON_SIZE_MENU)))));
			Gtk::MenuItem* presets_menu_item = &(items().front());
			presets_menu_item->set_submenu(*_presets_menu);
		}
		lilv_nodes_free(presets);
		lilv_node_free(title_pred);
		lilv_node_free(preset_pred);
	}

	if (has_control_inputs())
		_randomize_menuitem->show();
	else
		_randomize_menuitem->hide();

	if (plugin && (plugin->uri().str() == "http://drobilla.net/ns/ingen-internals#Controller"
			|| plugin->uri().str() == "http://drobilla.net/ns/ingen-internals#Trigger"))
		_learn_menuitem->show();
	else
		_learn_menuitem->hide();

	_enable_signal = true;
}

void
NodeMenu::on_menu_embed_gui()
{
	signal_embed_gui.emit(_embed_gui_menuitem->get_active());
}

void
NodeMenu::on_menu_randomize()
{
	App::instance().engine()->bundle_begin();

	const NodeModel* const nm = (NodeModel*)_object.get();
	for (NodeModel::Ports::const_iterator i = nm->ports().begin(); i != nm->ports().end(); ++i) {
		if ((*i)->is_input() && App::instance().can_control(i->get())) {
			float min = 0.0f, max = 1.0f;
			nm->port_value_range(*i, min, max, App::instance().sample_rate());
			const float val = ((rand() / (float)RAND_MAX) * (max - min) + min);
			App::instance().engine()->set_property((*i)->path(),
					App::instance().uris().ingen_value, val);
		}
	}

	App::instance().engine()->bundle_end();
}

void
NodeMenu::on_menu_disconnect()
{
	App::instance().engine()->disconnect_all(_object->parent()->path(), _object->path());
}

void
NodeMenu::on_preset_activated(const std::string& uri)
{
	const NodeModel* const   node   = (NodeModel*)_object.get();
	const PluginModel* const plugin = dynamic_cast<const PluginModel*>(node->plugin());

	LilvNode* port_pred = lilv_new_uri(
		plugin->lilv_world(),
		"http://lv2plug.in/ns/lv2core#port");
	LilvNode* symbol_pred = lilv_new_uri(
		plugin->lilv_world(),
		"http://lv2plug.in/ns/lv2core#symbol");
	LilvNode* value_pred = lilv_new_uri(
		plugin->lilv_world(),
		"http://lv2plug.in/ns/ext/presets#value");
	LilvNode*  subject = lilv_new_uri(plugin->lilv_world(), uri.c_str());
	LilvNodes* ports   = lilv_world_find_nodes(
		plugin->lilv_world(),
		subject,
		port_pred,
		NULL);
	App::instance().engine()->bundle_begin();
	LILV_FOREACH(nodes, i, ports) {
		const LilvNode* uri = lilv_nodes_get(ports, i);
		LilvNodes* values = lilv_world_find_nodes(
			plugin->lilv_world(), uri, value_pred, NULL);
		LilvNodes* symbols = lilv_world_find_nodes(
			plugin->lilv_world(), uri, symbol_pred, NULL);
		if (values && symbols) {
			const LilvNode* val = lilv_nodes_get_first(values);
			const LilvNode* sym = lilv_nodes_get_first(symbols);
			App::instance().engine()->set_property(
				node->path().base() + lilv_node_as_string(sym),
				App::instance().uris().ingen_value,
				lilv_node_as_float(val));
		}
	}
	App::instance().engine()->bundle_end();
	lilv_nodes_free(ports);
	lilv_node_free(value_pred);
	lilv_node_free(symbol_pred);
	lilv_node_free(port_pred);
}

bool
NodeMenu::on_preset_clicked(const std::string& uri, GdkEventButton* ev)
{
	on_preset_activated(uri);
	return false;
}

bool
NodeMenu::has_control_inputs()
{
	const NodeModel* const nm = (NodeModel*)_object.get();
	for (NodeModel::Ports::const_iterator i = nm->ports().begin(); i != nm->ports().end(); ++i)
		if ((*i)->is_input() && (*i)->is_numeric())
			return true;

	return false;
}

void
NodeMenu::enable_controls_menuitem()
{
	_controls_menuitem->property_sensitive() = true;
}

void
NodeMenu::disable_controls_menuitem()
{
	_controls_menuitem->property_sensitive() = false;
}

} // namespace GUI
} // namespace Ingen
